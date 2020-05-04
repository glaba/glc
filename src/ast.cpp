#include "ast.h"
#include "visitor.h"

#include <iostream>
#include <algorithm>

namespace ast {
	auto ty_int::make(long min, long max) -> unique_ptr<ty_int> {
		auto result = make_unique<ty_int>();
		result->min = min;
		result->max = max;
		return std::move(result);
	}

	auto variable_type::make(type_enum type, long min, long max) -> unique_ptr<variable_type> {
		auto result = make_unique<variable_type>();
		result->type = type;
		result->min = min;
		result->max = max;
		return std::move(result);
	}

	auto variable_type::is_arithmetic() -> bool {
		return type == type_enum::INT || type == type_enum::FLOAT;
	}

	auto variable_type::is_logical() -> bool {
		return type == type_enum::BOOL;
	}

	auto variable_decl::make(unique_ptr<variable_type>&& type, string name) -> unique_ptr<variable_decl> {
		auto result = make_unique<variable_decl>();
		result->type = std::move(type);
		result->name = name;
		set_parent(result, result->type);
		return std::move(result);
	}

	auto properties::make(vector<unique_ptr<variable_decl>>&& decls) -> unique_ptr<properties> {
		auto result = make_unique<properties>();
		result->variable_declarations = std::move(decls);
		for (auto& decl : result->variable_declarations) {
			set_parent(result, decl);
		}
		return std::move(result);
	}

	auto field::make(unit_object unit, member_op_enum member_op, string field_name) -> unique_ptr<field> {
		auto result = make_unique<field>();
		result->unit = unit;
		result->member_op = member_op;
		result->field_name = field_name;
		return std::move(result);
	}

	auto field::get_type() -> variable_type* {
		if (member_op != member_op_enum::CUSTOM) {
			// TODO: add checks for non-custom properties
			assert(false);
		}

		auto unit_trait = get_trait();
		if (!unit_trait) {
			return nullptr;
		} else {
			return unit_trait->get_property(field_name)->type.get();
		}
	}

	auto field::get_loop_from_identifier() -> for_in* {
		auto& identifier = std::get<identifier_unit>(unit).identifier;
		return find_parent<for_in>(*this, [&] (for_in& loop) { return loop.variable == identifier; });
	}

	auto field::get_trait() -> trait* {
		if (!std::holds_alternative<identifier_unit>(unit)) {
			return find_parent<trait>(*this);
		}

		auto& p = *find_parent<program>(*this);
		auto loop = get_loop_from_identifier();
		auto& trait_candidates = loop->traits;

		auto result = static_cast<trait*>(nullptr);
		auto decl_visitor = [&] (variable_decl& decl) {
			if (decl.name == field_name) {
				// Find the trait corresponding to this declaration
				auto trait_node = find_parent<trait>(decl);
				assert(trait_node != nullptr);
				if (std::find(trait_candidates.begin(), trait_candidates.end(), trait_node->name) != trait_candidates.end()) {
					result = trait_node;
				}
			}
		};
		visit<program, decltype(decl_visitor)>()(p, decl_visitor);
		return result;
	}

	auto arithmetic_value::make(variant<unique_ptr<field>, long, double>&& value) -> unique_ptr<arithmetic_value> {
		auto result = make_unique<arithmetic_value>();
		result->value = std::move(value);
		if (std::holds_alternative<unique_ptr<field>>(result->value)) {
			set_parent(result, std::get<unique_ptr<field>>(result->value));
		}
		return std::move(result);
	}

	auto arithmetic::make(arithmetic::arithmetic_expr&& expr) -> unique_ptr<arithmetic> {
		auto result = make_unique<arithmetic>();
		result->expr = std::move(expr);
		std::visit([&] (auto& node) { set_parent(result, node); }, result->expr);
		return std::move(result);
	}

	auto comparison::make(unique_ptr<arithmetic>&& lhs, unique_ptr<arithmetic>&& rhs,
		comparison_enum comparison_type) -> unique_ptr<comparison>
	{
		auto result = make_unique<comparison>();
		result->lhs = std::move(lhs);
		result->rhs = std::move(rhs);
		result->comparison_type = comparison_type;
		set_parent(result, result->lhs, result->rhs);
		return std::move(result);
	}

	auto negated::make(unique_ptr<logical>&& expr) -> unique_ptr<negated> {
		auto result = make_unique<negated>();
		result->expr = std::move(expr);
		set_parent(result, result->expr);
		return std::move(result);
	}

	auto logical::make(logical::logical_expr&& expr) -> unique_ptr<logical> {
		auto result = make_unique<logical>();
		result->expr = std::move(expr);
		std::visit([&] (auto& node) { set_parent(result, node); }, result->expr);
		return std::move(result);
	}

	auto assignment::make(unique_ptr<field>&& lhs, variant<unique_ptr<arithmetic>, unique_ptr<logical>>&& rhs) -> unique_ptr<assignment> {
		auto result = make_unique<assignment>();
		result->lhs = std::move(lhs);
		result->rhs = std::move(rhs);
		set_parent(result, result->lhs);
		std::visit([&] (auto& node) { set_parent(result, node); }, result->rhs);
		return std::move(result);
	}

	auto continuous_if::make(unique_ptr<logical>&& condition, unique_ptr<always_body>&& body) -> unique_ptr<continuous_if> {
		auto result = make_unique<continuous_if>();
		result->condition = std::move(condition);
		result->body = std::move(body);
		set_parent(result, result->condition, result->body);
		return std::move(result);
	}

	auto transition_if::make(unique_ptr<logical>&& condition, unique_ptr<always_body>&& body) -> unique_ptr<transition_if> {
		auto result = make_unique<transition_if>();
		result->condition = std::move(condition);
		result->body = std::move(body);
		set_parent(result, result->condition, result->body);
		return std::move(result);
	}

	auto for_in::make(string variable, unit_object range_unit, vector<string>& traits, unique_ptr<always_body>&& body) -> unique_ptr<for_in> {
		auto result = make_unique<for_in>();
		result->variable = variable;
		result->range_unit = range_unit;
		result->traits = traits;
		result->body = std::move(body);
		set_parent(result, result->body);
		return std::move(result);
	}

	auto for_in::get_loop_from_identifier() -> for_in* {
		auto identifier = std::get<identifier_unit>(range_unit).identifier;
		return find_parent<for_in>(*this->parent(), [&] (for_in& loop) { return loop.variable == identifier; });
	}

	auto always_body::make(vector<expression>&& exprs) -> unique_ptr<always_body> {
		auto result = make_unique<always_body>();
		result->exprs = std::move(exprs);
		for (auto& expr : result->exprs) {
			std::visit([&] (auto& node) { set_parent(result, node); }, expr);
		}
		return std::move(result);
	}

	auto trait::make(string name, unique_ptr<properties>&& props, unique_ptr<always_body>&& body) -> unique_ptr<trait> {
		auto result = make_unique<trait>();
		result->name = name;
		result->props = std::move(props);
		result->body = std::move(body);
		set_parent(result, result->props, result->body);
		return std::move(result);
	}

	auto trait::get_property(string name) -> variable_decl* {
		for (auto& prop : props->variable_declarations) {
			if (prop->name == name) {
				return prop.get();
			}
		}
		return nullptr;
	}

	auto trait_initializer::make(string name, map<string, literal_value> initial_values) -> unique_ptr<trait_initializer> {
		auto result = make_unique<trait_initializer>();
		result->name = name;
		result->initial_values = initial_values;
		return std::move(result);
	}

	auto unit_traits::make(string name, vector<unique_ptr<trait_initializer>>&& traits) -> unique_ptr<unit_traits> {
		auto result = make_unique<unit_traits>();
		result->name = name;
		result->traits = std::move(traits);
		for (auto& trait : result->traits) {
			set_parent(result, trait);
		}
		return std::move(result);
	}

	auto program::make(vector<unique_ptr<trait>>&& traits, vector<unique_ptr<unit_traits>>&& all_unit_traits) -> unique_ptr<program> {
		auto result = make_unique<program>();
		result->traits = std::move(traits);
		result->all_unit_traits = std::move(all_unit_traits);
		for (auto& trait : result->traits) {
			set_parent(result, trait);
		}
		for (auto& single_unit_traits : result->all_unit_traits) {
			set_parent(result, single_unit_traits);
		}
		return std::move(result);
	}

	auto program::get_trait(string name) -> trait* {
		for (auto& trait : traits) {
			if (trait->name == name) {
				return trait.get();
			}
		}
		return nullptr;
	}

	string print_arithmetic(arithmetic& root) {
		string output;
		std::visit(overloaded {
			[&] (unique_ptr<add> const& expr) {output = "(" + print_arithmetic(*expr->expr_1) + "+" + print_arithmetic(*expr->expr_2) + ")";},
			[&] (unique_ptr<sub> const& expr) {output = "(" + print_arithmetic(*expr->expr_1) + "-" + print_arithmetic(*expr->expr_2) + ")";},
			[&] (unique_ptr<mul> const& expr) {output = "(" + print_arithmetic(*expr->expr_1) + "*" + print_arithmetic(*expr->expr_2) + ")";},
			[&] (unique_ptr<div> const& expr) {output = "(" + print_arithmetic(*expr->expr_1) + "/" + print_arithmetic(*expr->expr_2) + ")";},
			[&] (unique_ptr<mod> const& expr) {output = "(" + print_arithmetic(*expr->expr_1) + "%" + print_arithmetic(*expr->expr_2) + ")";},
			[&] (unique_ptr<exp> const& expr) {output = "(" + print_arithmetic(*expr->expr_1) + "^" + print_arithmetic(*expr->expr_2) + ")";},
			[&] (unique_ptr<arithmetic_value> const& expr) {
				std::visit(overloaded {
					[&] (long value) {output = std::to_string(value);},
					[&] (double value) {output = std::to_string(value);},
					[&] (unique_ptr<field> const& value) {
						std::visit(overloaded {
							[&] (this_unit _) {output = "this";},
							[&] (type_unit _) {output = "type";},
							[&] (identifier_unit unit) {output = unit.identifier;}
						}, value->unit);
						switch (value->member_op) {
							case member_op_enum::BUILTIN: output += "::"; break;
							case member_op_enum::CUSTOM: output += "."; break;
							case member_op_enum::LANGUAGE: output += "->"; break;
						}
						output += value->field_name;
					}
				}, expr->value);
			}
		}, root.expr);
		return output;
	}

	string print_logical(logical& root) {
		string output;

		std::visit(overloaded {
			[&] (unique_ptr<and_op> const& expr) {output = "(" + print_logical(*expr->expr_1) + " and " + print_logical(*expr->expr_2) + ")";},
			[&] (unique_ptr<or_op> const& expr) {output = "(" + print_logical(*expr->expr_1) + " or " + print_logical(*expr->expr_2) + ")";},
			[&] (unique_ptr<val_bool> const& value) {output = value->value ? "true" : "false";},
			[&] (unique_ptr<comparison> const& value) {
				output = print_arithmetic(*value->lhs);
				switch (value->comparison_type) {
					case comparison_enum::EQ: output += "=="; break;
					case comparison_enum::NEQ: output += "!="; break;
					case comparison_enum::GT: output += ">"; break;
					case comparison_enum::LT: output += "<"; break;
					case comparison_enum::GTE: output += ">="; break;
					case comparison_enum::LTE: output += "<="; break;
				}
				output += print_arithmetic(*value->rhs);
			},
			[&] (unique_ptr<field> const& value) {
				std::visit(overloaded {
					[&] (this_unit _) {output = "this";},
					[&] (type_unit _) {output = "type";},
					[&] (identifier_unit unit) {output = unit.identifier;}
				}, value->unit);
				switch (value->member_op) {
					case member_op_enum::BUILTIN: output += "::"; break;
					case member_op_enum::CUSTOM: output += "."; break;
					case member_op_enum::LANGUAGE: output += "->"; break;
				}
				output += value->field_name;
			},
			[&] (unique_ptr<negated> const& value) {
				output = "(not " + print_logical(*value->expr) + ")";
			}
		}, root.expr);
		return output;
	}

	void print_program(program& root) {
		auto print_visitor = overloaded {
			[] (variable_type& n) { std::cout << "variable_type (" << &n << "): " << n.type << ", " << n.min << ", " << n.max << std::endl; },
			[] (variable_decl& n) { std::cout << "variable_decl (" << &n << "): " << n.name << ", " << n.type.get() << std::endl; },
			[] (field& n) {
				std::cout << "field (" << &n << "): ";
				if (std::holds_alternative<this_unit>(n.unit)) std::cout << "this, ";
				else if (std::holds_alternative<type_unit>(n.unit)) std::cout << "type, ";
				else std::cout << std::get<identifier_unit>(n.unit).identifier << ", ";
				std::cout << n.member_op << ", " << n.field_name << std::endl;
			}
		};
		visit<ast::program, decltype(print_visitor)>()(root, print_visitor);
	}
}