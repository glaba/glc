#include "semantic_checker.h"
#include "parser.h"
#include "ast.h"
#include "visitor.h"

#include <string>
#include <set>
#include <memory>
#include <type_traits>

using std::string;
using std::set;
using std::unique_ptr;

auto quote(string const& input) -> string {
	return "\'" + input + "\'";
}

struct semantic_checker_visitor {
	pass_manager& pm;
	ast::program& program;
	set<ast::node*> errored_nodes;

	semantic_checker_visitor(pass_manager& pm, ast::program& program) : pm(pm), program(program) {}

	template <typename AstNode>
	void error(AstNode& n, string const& err) {
		// Mark this node as well as all parent nodes as errored
		for (auto cur = static_cast<ast::node*>(&n); cur; cur = cur->parent()) {
			errored_nodes.insert(static_cast<ast::node*>(cur));
		}
		pm.error<semantic_checker>(n, err);
	}

	void operator()(ast::variable_type& n) {
		constexpr auto min_value = -(1L << (ast::ty_int::num_bits - 1));
		constexpr auto max_value = 1L << (ast::ty_int::num_bits - 1);

		if (n.type == ast::type_enum::INT) {
			if (n.min < min_value || n.min > max_value) {
				error(n, "Lower bound " + std::to_string(n.min) + " of int type is out of bounds");
			}
			if (n.max < min_value || n.max > max_value) {
				error(n, "Upper bound " + std::to_string(n.max) + " of int type is out of bounds");
			}
			if (n.max <= n.min) {
				error(n, "Upper bound of int type must be greater than lower bound");
			}
		}
	}

	void operator()(ast::properties& n) {
		set<string> variable_names;
		for (auto& decl : n.variable_declarations) {
			if (variable_names.find(decl->name) != variable_names.end()) {
				auto& trait_name = ast::find_parent<ast::trait>(n)->name;
				error(*decl, "Multiple variables with name " + quote(decl->name) + " in trait " + quote(trait_name));
			}
			variable_names.insert(decl->name);
		}
	}

	void operator()(ast::field& n) {
		if (n.member_op == ast::member_op_enum::CUSTOM) {
			std::visit(ast::overloaded {
				[&] (ast::this_unit& _) {
					// Check that the current trait contains the field
					auto trait = n.get_trait();
					auto found_decl = false;
					for (auto& decl : trait->props->variable_declarations) {
						if (decl->name == n.field_name) {
							found_decl = true;
							break;
						}
					}

					if (!found_decl) {
						error(n, "Trait " + quote(trait->name) + " does not contain property " + quote(n.field_name));
					}
				},
				[&] (ast::type_unit& _) {
					error(n, "Cannot access custom properties of special unit object 'type'");
				},
				[&] (ast::identifier_unit& u) {
					auto loop = n.get_loop_from_identifier();

					if (!loop) {
						error(n, "Undeclared identifier " + quote(u.identifier));
						return;
					}

					if (!n.get_trait()) {
						error(n, "None of the traits specified for unit object " + quote(u.identifier) +
							" contain property " + quote(n.field_name));
					}
				}
			}, n.unit);
		} else {
			// TODO: add checks for BUILTIN and LANGUAGE fields
			std::cout << "Semantic checker does not yet support builtin and language fields" << std::endl;
		}
	}

	void operator()(ast::arithmetic_value& n) {
		if (!std::holds_alternative<unique_ptr<ast::field>>(n.value)) {
			// The value is either a literal int or float
			return;
		}

		// Check that the field has the correct type
		auto field = std::get<unique_ptr<ast::field>>(n.value).get();
		if (!field->get_type()->is_arithmetic()) {
			error(n, "Field " + quote(field->field_name) + " used in arithmetic expression is neither an int nor a float");
		}
	}

	void operator()(ast::logical& n) {
		if (!std::holds_alternative<unique_ptr<ast::field>>(n.expr)) {
			// The expression is either a logical expression (inductively correct) or a literal
			return;
		}

		// Check that the field has the correct type
		auto field = std::get<unique_ptr<ast::field>>(n.expr).get();
		if (!field->get_type()->is_logical()) {
			error(n, "Field " + quote(field->field_name) + " used in logical expression is not of type bool");
		}
	}

	void operator()(ast::assignment& n) {
		auto lhs_type = n.lhs->get_type();

		std::visit(ast::overloaded {
			[&] (unique_ptr<ast::arithmetic>& _) {
				if (!lhs_type->is_arithmetic()) {
					error(n, "Cannot assign arithmetic value to non-arithmetic field " + quote(n.lhs->field_name));
				}
			},
			[&] (unique_ptr<ast::logical>& _) {
				if (!lhs_type->is_logical()) {
					error(n, "Cannot assign logical value to non-bool field " + quote(n.lhs->field_name));
				}
			}
		}, n.rhs);
	}

	void operator()(ast::for_in& n) {
		// Make sure the range_unit is valid if it's an identifier
		if (std::holds_alternative<ast::identifier_unit>(n.range_unit)) {
			auto& identifier = std::get<ast::identifier_unit>(n.range_unit).identifier;
			auto loop = n.get_loop_from_identifier();
			if (!loop) {
				error(n, "Undeclared identifier " + quote(identifier));
			}
		}

		// Make sure the traits are valid
		for (auto& trait : n.traits) {
			if (!program.get_trait(trait)) {
				error(n, "Undeclared trait " + quote(trait));
			}
		}

		// Check that the range is positive
		if (n.range < 0) {
			error(n, "Invalid range " + quote(std::to_string(n.range)) + "; ranges must be positive");
		}
	}

	void operator()(ast::trait_initializer& n) {
		// Check that the trait exists
		auto trait = program.get_trait(n.name);
		if (!trait) {
			error(n, "Undeclared trait " + quote(n.name) + " in trait initializer");
			return;
		}

		// Check that the properties exist and that the initial values match in type
		for (auto& [property_name, value] : n.initial_values) {
			auto property = trait->get_property(property_name);
			if (!property) {
				error(n, "Undeclared property " + quote(property_name) + " in trait initializer");
				continue;
			}

			auto& type = property->type;
			std::visit(ast::overloaded {
				[&] (bool value) {
					if (type->type != ast::type_enum::BOOL) {
						error(n, "Initial value of " + quote(value ? "true" : "false") + " cannot be assigned to property " +
							quote(property_name) + " which is not of type bool");
					}
				},
				[&] (double value) {
					if (type->type != ast::type_enum::FLOAT) {
						error(n, "Initial value of " + quote(std::to_string(value)) + " cannot be assigned to property " +
							quote(property_name) + " which is not of type float");
					}
				},
				[&] (long value) {
					if (type->type != ast::type_enum::INT) {
						error(n, "Initial value of " + quote(std::to_string(value)) + " cannot be assigned to property " +
							quote(property_name) + " which is not of type int");
					} else if (value < type->min || value > type->max) {
						error(n, "Initial value of " + quote(std::to_string(value)) +
							" is out of the specified bounds for property " + quote(property_name));
					}
				}
			}, value);
		}
	}

	void operator()(ast::program& n) {
		// Check for repeats in the list of traits
		set<string> traits;
		for (auto& trait : n.traits) {
			if (traits.find(trait->name) != traits.end()) {
				error(n, "Trait " + quote(trait->name) + " declared more than once");
			}
			traits.insert(trait->name);
		}

		// Check for repeats in the list of units
		set<string> units;
		for (auto& unit : n.all_unit_traits) {
			if (units.find(unit->name) != units.end()) {
				error(n, "Unit " + quote(unit->name) + " has multiple trait assignments");
			}
			units.insert(unit->name);
		}
	}
};

// Wrapper struct that will skip semantic checks on nodes whose children have semantic errors
//  on the conservative assumption that invalid children cause any analysis on parents to be meaningless
// Additionally, checks that the AST is well formed and all parent pointers are correct
template <typename Impl>
struct optionally_skip_checks : Impl {
	optionally_skip_checks(pass_manager& pm, ast::program& program) : Impl(pm, program) {}

	template <typename AstNode, typename = typename std::enable_if<has_visitor<Impl, AstNode>::value>::type>
	void operator()(AstNode& n) {
		if (Impl::errored_nodes.find(static_cast<ast::node*>(&n)) == Impl::errored_nodes.end()) {
			Impl::operator()(n);
		}

		if constexpr (!std::is_same<AstNode, ast::program>::value) {
			assert(ast::find_parent<ast::program>(n));
		}
	}
};

semantic_checker::semantic_checker(pass_manager& pm)
	: pm(pm), program(*pm.get_pass<parser>()->program)
{
	auto scv = optionally_skip_checks<semantic_checker_visitor>(pm, program);
	visit<ast::program, decltype(scv)>()(program, scv);
}
