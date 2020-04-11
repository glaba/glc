#pragma once

#include <string>
#include <vector>
#include <variant>
#include <memory>

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

	struct this_field : node {};
	struct type_field : node {};
	struct unit_field : node { string identifier; };
	struct builtin_member_op : node {};
	struct custom_member_op : node {};
	struct language_member_op : node {};
	struct field : node {
		variant<this_field, type_field, unit_field> field_type; 
		variant<builtin_member_op, custom_member_op, language_member_op> member_op;
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
	string print(arithmetic& root) {
		string output;
		std::visit(overloaded {
			[&] (unique_ptr<add> const& expr) {output = "(" + print(*expr->expr_1) + "+" + print(*expr->expr_2) + ")";},
			[&] (unique_ptr<sub> const& expr) {output = "(" + print(*expr->expr_1) + "-" + print(*expr->expr_2) + ")";},
			[&] (unique_ptr<mul> const& expr) {output = "(" + print(*expr->expr_1) + "*" + print(*expr->expr_2) + ")";},
			[&] (unique_ptr<div> const& expr) {output = "(" + print(*expr->expr_1) + "/" + print(*expr->expr_2) + ")";},
			[&] (unique_ptr<mod> const& expr) {output = "(" + print(*expr->expr_1) + "%" + print(*expr->expr_2) + ")";},
			[&] (unique_ptr<exp> const& expr) {output = "(" + print(*expr->expr_1) + "^" + print(*expr->expr_2) + ")";},
			[&] (unique_ptr<arithmetic_value> const& expr) {
				std::visit(overloaded {
					[&] (unique_ptr<val_int> const& value) {output = std::to_string(value->value);},
					[&] (unique_ptr<val_float> const& value) {output = std::to_string(value->value);},
					[&] (unique_ptr<field> const& value) {output = "field";}
				}, expr->value);
			}
		}, root.expr);
		return output;
	}
};
