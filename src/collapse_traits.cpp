#include "collapse_traits.h"
#include "visitor.h"

#include <iostream>
#include <variant>
#include <string>
#include <vector>

using std::string;
using std::vector;

collapse_traits::collapse_traits(pass_manager& pm, ast::program& program)
	: pm(pm), program(program)
{
	rename_variables();
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
				f.field_name = trait + "_" + f.field_name;
			}
		} else if (std::holds_alternative<ast::this_unit>(f.unit)) {
			f.field_name = trait_name + "_" + f.field_name;
		} else {
			// This unit is type, which means that none of its field names should be changed,
			//  since any field should only be referring to built-in or language properties
			pm.error<collapse_traits>(f, "Cannot access custom properties of special unit object 'type'");
		}
	}
};

struct rename_variable_decls_visitor {
	string trait_name;

	rename_variable_decls_visitor(string trait_name) : trait_name(trait_name) {}

	void operator()(ast::variable_decl& decl) {
		decl.name = trait_name + "_" + decl.name;
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

	ast::print_program(program);
}

register_pass<collapse_traits> register_collapse_traits;