#include "collapse_traits.h"
#include "visitor.h"

#include <iostream>
#include <variant>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <tuple>

using std::string;
using std::vector;
using std::unique_ptr;
using std::make_unique;
using std::map;
using std::tuple;

collapse_traits::collapse_traits(pass_manager& pm, ast::program& program)
	: pm(pm), program(program)
{
	rename_variables();
	create_collapsed_trait();
}

struct rename_variable_uses_visitor {
	pass_manager& pm;
	ast::program& program;
	string trait_name;

	rename_variable_uses_visitor(pass_manager& pm, ast::program& program, string trait_name)
		: pm(pm), program(program), trait_name(trait_name) {}

	void operator()(ast::field& f) {
		// Only user-defined custom fields in traits are being renamed
		if (f.member_op != ast::member_op_enum::CUSTOM) {
			return;
		}

		if (std::holds_alternative<ast::identifier_unit>(f.unit)) {
			auto identifier = std::get<ast::identifier_unit>(f.unit).identifier;

			// Follow parent pointers until we find the for_in loop where the identifier was declared
			auto parent = ast::find_parent<ast::for_in>(f, [&] (ast::for_in& loop) { return loop.variable == identifier; });

			if (!parent) {
				pm.error<collapse_traits>(f, "Undeclared identifier " + identifier);
				return;
			}

			auto& traits = static_cast<ast::for_in*>(parent)->traits;

			// Find out which trait the field name came from by visiting all variable declarations
			auto trait = string();
			auto decl_visitor = [&] (ast::variable_decl& decl) {
				if (decl.name == f.field_name) {
					// Find the trait corresponding to this declaration
					auto trait_node = ast::find_parent<ast::trait>(decl);
					assert(trait_node != nullptr);
					if (std::find(traits.begin(), traits.end(), trait_node->name) != traits.end()) {
						trait = trait_node->name;
					}
				}
			};
			visit<ast::program, decltype(decl_visitor)>()(program, decl_visitor);

			if (trait == "") {
				pm.error<collapse_traits>(f, "Field " + f.field_name + " not a member of any specified trait");
			} else {
				f.field_name = "_" + trait + "_" + f.field_name;
			}
		} else if (std::holds_alternative<ast::this_unit>(f.unit)) {
			f.field_name = "_" + trait_name + "_" + f.field_name;
		} else {
			// This unit is type, which means that none of its field names should be changed,
			//  since any field should only be referring to built-in or language properties
			pm.error<collapse_traits>(f, "Cannot access custom properties of special unit object 'type'");
		}
	}

	void operator()(ast::trait_initializer& t) {
		auto new_initial_values = map<string, ast::literal_value>();
		for (auto& [field_name, value] : t.initial_values) {
			new_initial_values["_" + t.name + "_" + field_name] = value;
		}
		t.initial_values = new_initial_values;
	}
};

struct rename_variable_decls_visitor {
	string trait_name;

	rename_variable_decls_visitor(string trait_name) : trait_name(trait_name) {}

	void operator()(ast::variable_decl& decl) {
		decl.name = "_" + trait_name + "_" + decl.name;
	}
};

void collapse_traits::rename_variables() {
	for (auto& trait : program.traits) {
		auto rvu = rename_variable_uses_visitor(pm, program, trait->name);
		visit<ast::trait, decltype(rvu)>()(*trait, rvu);
	}

	for (auto& trait : program.traits) {
		auto rvd = rename_variable_decls_visitor(trait->name);
		visit<ast::trait, decltype(rvd)>()(*trait, rvd);
	}
}

struct insert_trait_checks_visitor {
	map<string, tuple<string, unsigned>>& trait_bitfield;

	insert_trait_checks_visitor(map<string, tuple<string, unsigned>>& trait_bitfield)
		: trait_bitfield(trait_bitfield) {}

	void operator()(ast::for_in& loop) {
		if (loop.traits.size() == 0) {
			return;
		}

		// Create the condition that checks for all the traits
		for (auto &trait : loop.traits) {
			auto &[bitfield_name, bitposition] = trait_bitfield[trait];

			// Generate a tree corresponding to the following expression
			//   if (<loop.variable>.<bitfield_name> % <2^(bitposition+1)> >= <2^bitposition>) {
			//       <original body of for_in>
			//   }
			// where the expressions in <> are inserted as constant values into the AST
			using namespace ast;

			// <loop.variable>.<bitfield_name>
			auto mod_lhs = arithmetic::from_value(
				field::make(identifier_unit(loop.variable), member_op_enum::CUSTOM, bitfield_name));
			// <2^(bitposition+1)>
			auto mod_rhs = arithmetic::from_value(1L << (bitposition + 1));
			// <loop.variable>.<bitfield_name> % <2^(bitposition+1)>
			auto mod_expr = arithmetic::make(mod::make(std::move(mod_lhs), std::move(mod_rhs)));
			// <2^bitposition>
			auto comp_rhs = arithmetic::from_value(1L << bitposition);
			// <loop.variable>.<bitfield_name> % <2^(bitposition+1)> >= <2^bitposition>
			auto comp_expr = comparison::make(std::move(mod_expr), std::move(comp_rhs), comparison_enum::GTE);
			// if (<loop.variable>.<bitfield_name> % <2^(bitposition+1)> >= <2^bitposition>) { <original body> }
			auto if_stmt = continuous_if::make(logical::make(std::move(comp_expr)), std::move(loop.body));

			auto new_loop_body = make_unique<always_body>();
			new_loop_body->exprs.emplace_back(std::move(if_stmt));
			loop.body = std::move(new_loop_body);
			loop.traits.clear();
			set_parent(&loop, loop.body);
		}
	}
};

void collapse_traits::create_collapsed_trait() {
	auto new_trait = make_unique<ast::trait>();
	new_trait->name = "main";
	new_trait->props = make_unique<ast::properties>();
	new_trait->body = make_unique<ast::always_body>();

	// Copy the variables and logic from the old traits
	for (auto& trait : program.traits) {
		for (auto& property : trait->props->variable_declarations) {
			new_trait->props->variable_declarations.push_back(std::move(property));
		}

		for (auto& expr : trait->body->exprs) {
			new_trait->body->exprs.push_back(std::move(expr));
		}
	}

	// Add the new trait_bitfield property(s)
	auto num_trait_bitfields = program.traits.size() / ast::ty_int::num_bits + 1;
	for (size_t i = 0; i < num_trait_bitfields; i++) {
		auto num_bits = ast::ty_int::num_bits;
		if (i == num_trait_bitfields - 1) {
			num_bits = program.traits.size() - (num_trait_bitfields - 1) * ast::ty_int::num_bits;
		}

		auto type = make_unique<ast::variable_type>();
		type->type = ast::type_enum::INT;
		type->min = 0;
		type->max = (1 << num_bits) - 1;

		auto decl = make_unique<ast::variable_decl>();
		decl->name = "trait_bitfield" + std::to_string(i);
		decl->type = std::move(type);

		new_trait->props->variable_declarations.push_back(std::move(decl));
	}

	// Map from trait name to variable name / bitposition pair
	auto trait_bitfield = map<string, tuple<string, unsigned>>();
	for (size_t i = 0; i < program.traits.size(); i++) {
		auto variable_name = "trait_bitfield" + std::to_string(i / ast::ty_int::num_bits);
		auto bitposition = i % ast::ty_int::num_bits;
		trait_bitfield[program.traits[i]->name] = {variable_name, bitposition};
	}

	// Find all for_in loops and transform trait checks to explicit if statements
	auto itc = insert_trait_checks_visitor(trait_bitfield);
	visit<ast::trait, decltype(itc)>()(*new_trait, itc);

	// Remove old traits and insert new trait
	program.traits.clear();
	program.traits.push_back(std::move(new_trait));

	// Transform initializers for original traits into initializers for new trait
	for (auto& cur_unit_traits : program.all_unit_traits) {
		auto main_trait_initializer = make_unique<ast::trait_initializer>();
		main_trait_initializer->name = "main";

		auto& initial_values = main_trait_initializer->initial_values;

		// Iterate through all the old traits
		for (auto& trait_initializer : cur_unit_traits->traits) {
			// Add the initial values for the current trait with transformed variable names
			for (auto& [field_name, initial_value] : trait_initializer->initial_values) {
				initial_values["_" + trait_initializer->name + "_" + field_name] = initial_value;
			}

			// Add a bit in the bitfield indicating that this trait is active
			auto &[variable_name, bitposition] = trait_bitfield[trait_initializer->name];
			if (initial_values.find(variable_name) == initial_values.end()) {
				initial_values[variable_name] = 0L;
			}
			auto value = std::get<long>(initial_values[variable_name]);
			initial_values[variable_name] = value | (1L << bitposition);
		}

		cur_unit_traits->traits.clear();
		cur_unit_traits->traits.push_back(std::move(main_trait_initializer));
	}
	int a = 5;
}

register_pass<collapse_traits> register_collapse_traits;
