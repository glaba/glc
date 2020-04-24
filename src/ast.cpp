#include "ast.h"

namespace ast {
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
					[&] (unique_ptr<val_int> const& value) {output = std::to_string(value->value);},
					[&] (unique_ptr<val_float> const& value) {output = std::to_string(value->value);},
					[&] (unique_ptr<field> const& value) {
						std::visit(overloaded {
							[&] (this_field _) {output = "this";},
							[&] (type_field _) {output = "type";},
							[&] (unit_field unit) {output = unit.identifier;}
						}, value->field_type);
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
					[&] (this_field _) {output = "this";},
					[&] (type_field _) {output = "type";},
					[&] (unit_field unit) {output = unit.identifier;}
				}, value->field_type);
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
}