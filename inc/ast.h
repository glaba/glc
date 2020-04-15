#pragma once

#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <iostream>

namespace ast {
	using std::string;
	using std::vector;
	using std::variant;
	using std::unique_ptr;

	struct node {
		virtual ~node() {}
	};

	struct val_bool : node { bool value; };
	struct val_float : node { double value; };
	struct val_int : node { int value; };

	enum type_enum { BOOL, INT, FLOAT };
	struct ty : node {
		virtual type_enum get_type() = 0;
	};
	struct ty_bool : ty {
		virtual type_enum get_type() { return type_enum::BOOL; }
	};
	struct ty_float : ty {
		virtual type_enum get_type() { return type_enum::FLOAT; }
	};
	struct ty_int : ty {
		virtual type_enum get_type() { return type_enum::INT; }
		int min, max;
	};
	struct variable_type : node {
		type_enum type;
		int min, max;
	};
	struct variable_decl : node {
		string name;
		unique_ptr<variable_type> type;
	};
	struct properties : node {
		vector<unique_ptr<variable_decl>> variable_declarations;
	};

	enum member_op_enum { BUILTIN, CUSTOM, LANGUAGE };
	struct this_field {};
	struct type_field {};
	struct unit_field { string identifier; };
	struct field : node {
		variant<this_field, type_field, unit_field> field_type;
		member_op_enum member_op;
		string field_name;
	};

	struct arithmetic;
	struct arithmetic_op : node {
		unique_ptr<arithmetic> expr_1, expr_2;
	};
	struct add : arithmetic_op {};
	struct mul : arithmetic_op {};
	struct sub : arithmetic_op {};
	struct div : arithmetic_op {};
	struct mod : arithmetic_op {};
	struct exp : arithmetic_op {};
	struct arithmetic_value : node { variant<unique_ptr<field>, unique_ptr<val_int>, unique_ptr<val_float>> value; };
	struct arithmetic : node {
		variant<
			unique_ptr<add>, unique_ptr<mul>, unique_ptr<sub>, unique_ptr<div>,
			unique_ptr<mod>, unique_ptr<exp>, unique_ptr<arithmetic_value>> expr;
	};

	enum comparison_enum { EQ, NEQ, GT, LT, GTE, LTE };
	struct comparison : node {
		unique_ptr<arithmetic> lhs, rhs;
		comparison_enum comparison_type;
	};

	struct logical;
	struct logical_op : node {
		unique_ptr<logical> expr_1, expr_2;
	};
	struct and_op : logical_op {};
	struct or_op : logical_op {};
	struct negated : node { unique_ptr<logical> expr; };
	struct logical : node {
		variant<
			unique_ptr<and_op>, unique_ptr<or_op>,
			unique_ptr<field>, unique_ptr<val_bool>, unique_ptr<comparison>, unique_ptr<negated>> expr;
	};

	struct assignment : node {
		field *lhs;
		arithmetic *rhs;
	};

	using expression = variant<assignment*>;

	struct always : node {
		vector<expression*> exprs;
	};

	struct trait : node {
		string name;
		properties* props;
		always* logic;
	};

	struct program : node {
		vector<trait*> traits;
	};

	template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
	template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

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
};
