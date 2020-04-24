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
		unique_ptr<field> lhs;
		variant<unique_ptr<arithmetic>, unique_ptr<logical>> rhs;
	};

	struct always_body;
	struct continuous_if : node {
		unique_ptr<logical> condition;
		unique_ptr<always_body> body;
	};

	using expression = variant<unique_ptr<assignment>, unique_ptr<continuous_if>>;
	struct always_body : node {
		vector<expression> exprs;
	};

	struct trait : node {
		string name;
		unique_ptr<properties> props;
		unique_ptr<always_body> body;
	};

	struct unit : node {
		string name;
		vector<string> traits;
	};

	struct program : node {
		vector<unique_ptr<trait>> traits;
		vector<unique_ptr<unit>> units;
	};

	template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
	template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

	string print_arithmetic(arithmetic& root);
	string print_logical(logical& root);
};
