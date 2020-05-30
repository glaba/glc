#include "collapse_traits.h"
#include "parser.h"
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

collapse_traits::collapse_traits(pass_manager& pm)
	: pm(pm), program(*pm.get_pass<parser>()->program)
{
	rename_variables();
	create_collapsed_trait();
}

// Renames each variable v within a trait t to ~t~v, where the dollar sign is used to ensure
//  that there are no naming conflicts, since it is a disallowed character

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

		std::visit(ast::overloaded {
			[&] (ast::this_unit& _) {
				auto cur_trait = ast::find_parent<ast::trait>(f);
				f.field_name = cur_trait->name + "~" + f.field_name;
			},
			[&] (ast::type_unit& _) {
				assert(false);
			},
			[&] (ast::identifier_unit& _) {
				auto origin_trait = f.get_trait()->name;
				assert(!origin_trait.empty());
				f.field_name = origin_trait + "~" + f.field_name;
			}
		}, f.unit);
	}

	void operator()(ast::trait_initializer& t) {
		auto new_initial_values = map<string, ast::literal_value>();
		for (auto& [field_name, value] : t.initial_values) {
			new_initial_values[t.name + "~" + field_name] = value;
		}
		t.initial_values = new_initial_values;
	}
};

struct rename_variable_decls_visitor {
	string trait_name;

	rename_variable_decls_visitor(string trait_name) : trait_name(trait_name) {}

	void operator()(ast::variable_decl& decl) {
		decl.name = trait_name + "~" + decl.name;
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

// Returns a comparison that evaluates true when the provided unit object has the specified trait
auto get_trait_check(ast::unit_object const& unit, map<string, tuple<string, unsigned>>& trait_bitfield, string const& trait)
	-> unique_ptr<ast::comparison>
{
	using namespace ast;

	auto &[bitfield_name, bitposition] = trait_bitfield[trait];

	// Create this expression: <uni>.<bitfield_name> % <2^(bitposition+1)> >= <2^bitposition>
	// <unit>.<bitfield_name>
	auto mod_lhs = arithmetic::from_value(
		field::make(unit, member_op_enum::CUSTOM, bitfield_name));
	// <2^(bitposition+1)>
	auto mod_rhs = arithmetic::from_value(1L << (bitposition + 1));
	// <unit>.<bitfield_name> % <2^(bitposition+1)>
	auto mod_expr = arithmetic::make(mod::make(std::move(mod_lhs), std::move(mod_rhs)));
	// <2^bitposition>
	auto comp_rhs = arithmetic::from_value(1L << bitposition);
	// <unit>.<bitfield_name> % <2^(bitposition+1)> >= <2^bitposition>
	return comparison::make(std::move(mod_expr), std::move(comp_rhs), comparison_enum::GTE);
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
			// Generate a tree corresponding to the following expression
			//   if (<loop variable has trait>) {
			//       <original body of for_in>
			//   }
			// where the expressions in <> are inserted as constant values into the AST
			using namespace ast;

			auto comp_expr = get_trait_check(identifier_unit(loop.variable), trait_bitfield, trait);
			// if (<loop.variable>.<bitfield_name> % <2^(bitposition+1)> >= <2^bitposition>) { <original body> }
			auto if_stmt = continuous_if::make(logical::make(std::move(comp_expr)), std::move(loop.body));

			auto new_loop_body_exprs = vector<ast::expression>();
			new_loop_body_exprs.emplace_back(std::move(if_stmt));
			auto new_loop_body = always_body::make(std::move(new_loop_body_exprs));

			loop.replace_body(std::move(new_loop_body));
			loop.traits.clear();
			loop.traits.push_back("main");
		}
	}
};

void collapse_traits::create_collapsed_trait() {
	auto new_trait = ast::trait::make("main", make_unique<ast::properties>(), make_unique<ast::always_body>());

	// Copy the variables from the old traits
	for (auto& trait : program.traits) {
		for (auto& property : trait->props->variable_declarations) {
			new_trait->props->add_decl(std::move(property));
		}
	}

	// Add the new trait_bitfield property(s)
	auto num_trait_bitfields = program.traits.size() / ast::ty_int::num_bits + 1;
	for (size_t i = 0; i < num_trait_bitfields; i++) {
		auto num_bits = ast::ty_int::num_bits;
		if (i == num_trait_bitfields - 1) {
			num_bits = program.traits.size() - (num_trait_bitfields - 1) * ast::ty_int::num_bits;
		}

		auto type = ast::variable_type::make(ast::type_enum::INT, 0, (1 << num_bits) - 1);
		auto decl = ast::variable_decl::make(std::move(type), "trait_bitfield" + std::to_string(i));

		new_trait->props->add_decl(std::move(decl));
	}

	// Map from trait name to variable name / bitposition pair
	auto trait_bitfield = map<string, tuple<string, unsigned>>();
	for (size_t i = 0; i < program.traits.size(); i++) {
		auto variable_name = "trait_bitfield" + std::to_string(i / ast::ty_int::num_bits);
		auto bitposition = i % ast::ty_int::num_bits;
		trait_bitfield[program.traits[i]->name] = {variable_name, bitposition};
	}

	// Copy the logic from the old traits
	for (auto& trait : program.traits) {
		// Comparison expression checking if 'this' has the trait
		auto comp_expr = get_trait_check(ast::this_unit(), trait_bitfield, trait->name);

		// if (<'this' has the current trait>) { <original trait body> }
		auto trait_check = ast::continuous_if::make(ast::logical::make(std::move(comp_expr)), std::move(trait->body));

		// Insert the transformed body into the new trait
		new_trait->body->insert_expr(std::move(trait_check));
	}

	// Find all for_in loops and transform trait checks to explicit if statements
	auto itc = insert_trait_checks_visitor(trait_bitfield);
	visit<ast::trait, decltype(itc)>()(*new_trait, itc);

	// Remove old traits and insert new trait
	program.traits.clear();
	program.insert_trait(std::move(new_trait));

	// Transform initializers for original traits into initializers for new trait
	for (auto& cur_unit_traits : program.all_unit_traits) {
		auto main_trait_initializer = make_unique<ast::trait_initializer>();
		main_trait_initializer->name = "main";

		auto& initial_values = main_trait_initializer->initial_values;

		// Iterate through all the old traits
		for (auto& trait_initializer : cur_unit_traits->traits) {
			// Add the initial values for the current trait with transformed variable names
			for (auto& [field_name, initial_value] : trait_initializer->initial_values) {
				initial_values["~" + trait_initializer->name + "~" + field_name] = initial_value;
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
		cur_unit_traits->insert_initializer(std::move(main_trait_initializer));
	}
}
