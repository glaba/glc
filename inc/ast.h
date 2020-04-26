#pragma once

#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <functional>
#include <typeinfo>

namespace ast {
	using std::string;
	using std::vector;
	using std::variant;
	using std::unique_ptr;
	using std::function;

	struct node {
		virtual ~node() {}
		virtual auto get_id() -> size_t = 0;
		virtual auto parent() -> node*& = 0;
		virtual auto filename() -> string& = 0;
		virtual auto line() -> size_t& = 0;
		virtual auto col() -> size_t& = 0;
	};

	template <typename Impl>
	struct node_impl : node {
		static auto id() -> size_t {
			return typeid(Impl).hash_code();
		}

		virtual auto get_id() -> size_t {
			return id();
		}

		virtual auto parent() -> node*& {
			return parent_;
		}

		virtual auto filename() -> string& {
			return filename_;
		}

		virtual auto line() -> size_t& {
			return line_;
		}

		virtual auto col() -> size_t& {
			return col_;
		}

	private:
		node* parent_;
		string filename_;
		size_t line_;
		size_t col_;
	};

	template <typename Check>
	auto isa(node& n) -> bool {
		return n.get_id() == Check::id();
	}

	template <typename Target>
	auto find_parent(node& n, function<bool(Target&)> predicate = [] (Target& _) { return true; }) -> Target* {
		auto cur = n.parent();
		auto target_id = Target::id();
		while (cur && (cur->get_id() != target_id || !predicate(*static_cast<Target*>(cur)))) {
			cur = cur->parent();
		}
		return static_cast<Target*>(cur);
	}

	struct val_bool : node_impl<val_bool> { bool value; };
	struct val_float : node_impl<val_float> { double value; };
	struct val_int : node_impl<val_int> { int value; };

	enum type_enum { BOOL, INT, FLOAT };
	struct ty_bool : node_impl<ty_bool> {};
	struct ty_float : node_impl<ty_float> {};
	struct ty_int : node_impl<ty_int> {
		int min, max;
	};
	struct variable_type : node_impl<variable_type> {
		type_enum type;
		int min, max;
	};
	struct variable_decl : node_impl<variable_decl> {
		string name;
		unique_ptr<variable_type> type;
	};
	struct properties : node_impl<properties> {
		vector<unique_ptr<variable_decl>> variable_declarations;
	};

	enum member_op_enum { BUILTIN, CUSTOM, LANGUAGE };
	struct this_unit {};
	struct type_unit {};
	struct identifier_unit { string identifier; };
	using unit_object = variant<this_unit, type_unit, identifier_unit>;
	struct field : node_impl<field> {
		unit_object unit;
		member_op_enum member_op;
		string field_name;
	};

	struct arithmetic;
	struct arithmetic_op {
		unique_ptr<arithmetic> expr_1, expr_2;
	};
	struct add : arithmetic_op, node_impl<add> {};
	struct mul : arithmetic_op, node_impl<mul> {};
	struct sub : arithmetic_op, node_impl<sub> {};
	struct div : arithmetic_op, node_impl<div> {};
	struct mod : arithmetic_op, node_impl<mod> {};
	struct exp : arithmetic_op, node_impl<exp> {};
	struct arithmetic_value : node_impl<arithmetic_value> { variant<unique_ptr<field>, int, double> value; };
	struct arithmetic : node_impl<arithmetic> {
		variant<
			unique_ptr<add>, unique_ptr<mul>, unique_ptr<sub>, unique_ptr<div>,
			unique_ptr<mod>, unique_ptr<exp>, unique_ptr<arithmetic_value>> expr;
	};

	enum comparison_enum { EQ, NEQ, GT, LT, GTE, LTE };
	struct comparison : node_impl<comparison> {
		unique_ptr<arithmetic> lhs, rhs;
		comparison_enum comparison_type;
	};

	struct logical;
	struct logical_op {
		unique_ptr<logical> expr_1, expr_2;
	};
	struct and_op : logical_op, node_impl<and_op> {};
	struct or_op : logical_op, node_impl<or_op> {};
	struct negated : node_impl<negated> { unique_ptr<logical> expr; };
	struct logical : node_impl<logical> {
		variant<
			unique_ptr<and_op>, unique_ptr<or_op>,
			unique_ptr<field>, unique_ptr<val_bool>, unique_ptr<comparison>, unique_ptr<negated>> expr;
	};

	struct assignment : node_impl<assignment> {
		unique_ptr<field> lhs;
		variant<unique_ptr<arithmetic>, unique_ptr<logical>> rhs;
	};

	struct always_body;
	struct continuous_if : node_impl<continuous_if> {
		unique_ptr<logical> condition;
		unique_ptr<always_body> body;
	};
	struct transition_if : node_impl<transition_if> {
		unique_ptr<logical> condition;
		unique_ptr<always_body> body;
	};
	struct for_in : node_impl<for_in> {
		string variable;
		unit_object range_unit;
		vector<string> traits;
		unique_ptr<always_body> body;
	};

	using expression = variant<unique_ptr<assignment>, unique_ptr<continuous_if>, unique_ptr<transition_if>, unique_ptr<for_in>>;
	struct always_body : node_impl<always_body> {
		vector<expression> exprs;
	};

	struct trait : node_impl<trait> {
		string name;
		unique_ptr<properties> props;
		unique_ptr<always_body> body;
	};

	struct unit_traits : node_impl<unit_traits> {
		string name;
		vector<string> traits;
	};

	struct program : node_impl<program> {
		vector<unique_ptr<trait>> traits;
		vector<unique_ptr<unit_traits>> all_unit_traits;
	};

	template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
	template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

	string print_arithmetic(arithmetic& root);
	string print_logical(logical& root);
	void print_program(program& root);
};
