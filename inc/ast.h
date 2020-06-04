#pragma once

#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <functional>
#include <typeinfo>
#include <map>

namespace ast {
	using std::string;
	using std::vector;
	using std::variant;
	using std::unique_ptr;
	using std::make_unique;
	using std::function;
	using std::map;

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

		static auto make() -> unique_ptr<Impl> {
			return make_unique<Impl>();
		}

		virtual auto clone() -> unique_ptr<Impl> {
			return make_unique<Impl>();
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

	// Sets the parent of the child nodes to point to the provided parent node
    // U, T, and Rest... should all be pointer-like objects to ast::node
    template <typename U> void set_parent(U& parent) {}
    template <typename U, typename T, typename... Rest>
    void set_parent(U&& parent, T& child, Rest&... rest) {
        child->parent() = static_cast<ast::node*>(&*parent);
        set_parent(parent, rest...);
    }

    // Forward declarations of AST types
    struct add; struct mul; struct sub; struct div; struct mod; struct exp;
    struct and_op; struct or_op; struct comparison; struct negated;
    struct for_in;
    struct trait;
    struct program;

	template <typename Impl, typename T>
	struct val_T : node_impl<Impl> {
		T value;

		static auto make(T value) -> unique_ptr<Impl> {
			auto result = make_unique<Impl>();
			result->value = value;
			return std::move(result);
		}

		virtual auto clone() -> unique_ptr<Impl> {
			return make(value);
		}
	};
	struct val_bool : val_T<val_bool, bool> {};
	struct val_float : val_T<val_float, double> {};
	struct val_int : val_T<val_int, long> {};
	using literal_value = variant<bool, double, long>;

	enum type_enum { BOOL, INT, FLOAT };
	struct ty_bool : node_impl<ty_bool> {};
	struct ty_float : node_impl<ty_float> {};
	struct ty_int : node_impl<ty_int> {
		static constexpr auto num_bits = 52;
		long min, max;

		static auto make(long min, long max) -> unique_ptr<ty_int>;
		auto clone() -> unique_ptr<ty_int>;
	};

	struct variable_type : node_impl<variable_type> {
		type_enum type;
		long min, max;

		static auto make_value(type_enum type, long min, long max) -> variable_type;
		static auto make(type_enum type, long min, long max) -> unique_ptr<variable_type>;
		auto clone() -> unique_ptr<variable_type>;
		auto is_arithmetic() -> bool;
		auto is_logical() -> bool;
	};

	struct variable_decl : node_impl<variable_decl> {
		unique_ptr<variable_type> type;
		string name;

		static auto make(unique_ptr<variable_type>&& type, string name) -> unique_ptr<variable_decl>;
		auto clone() -> unique_ptr<variable_decl>;
	};

	struct properties : node_impl<properties> {
		vector<unique_ptr<variable_decl>> variable_declarations;

		static auto make(vector<unique_ptr<variable_decl>>&& decls) -> unique_ptr<properties>;
		auto clone() -> unique_ptr<properties>;
		void add_decl(unique_ptr<variable_decl>&& decl);
	};

	enum member_op_enum { BUILTIN, CUSTOM, LANGUAGE };
	struct this_unit {};
	struct type_unit {};
	struct identifier_unit {
		identifier_unit(string identifier) : identifier(identifier) {}
		string identifier;
	};

	using unit_object = variant<this_unit, type_unit, identifier_unit>;
	struct field : node_impl<field> {
		unit_object unit;
		member_op_enum member_op;
		string field_name;
		bool is_rate;

		static auto make(unit_object unit, member_op_enum member_op, string field_name, bool is_rate = false) -> unique_ptr<field>;
		auto clone() -> unique_ptr<field>;
		auto get_type() -> variable_type*;
		// If the unit_object has type identifier_unit, returns the loop where the unit object was declared
		auto get_loop_from_identifier() -> for_in*;
		// If the field is of the form <valid unit object>.<field_name>, returns the trait that the field_name belongs to
		auto get_trait() -> trait*;

		// Returns a map from field name to its type for all the modifiable fields built-in to the game
		static auto get_builtin_fields() -> map<string, variable_type>&;
	};

	struct arithmetic_value : node_impl<arithmetic_value> {
		variant<unique_ptr<field>, long, double> value;

		static auto make(variant<unique_ptr<field>, long, double>&& value) -> unique_ptr<arithmetic_value>;
		auto clone() -> unique_ptr<arithmetic_value>;
	};

	struct arithmetic : node_impl<arithmetic> {
		using arithmetic_expr = variant<
			unique_ptr<add>, unique_ptr<mul>, unique_ptr<sub>, unique_ptr<div>,
			unique_ptr<mod>, unique_ptr<exp>, unique_ptr<arithmetic_value>>;
		arithmetic_expr expr;

		static auto make(arithmetic_expr&& expr) -> unique_ptr<arithmetic>;
		template <typename V>
		static auto from_value(V&& value) {
			return make(arithmetic_value::make(std::move(value)));
		}
		auto clone() -> unique_ptr<arithmetic>;
	};

	template <typename Impl>
	struct arithmetic_op : node_impl<Impl> {
		unique_ptr<arithmetic> expr_1, expr_2;

		static auto make(unique_ptr<arithmetic>&& expr_1, unique_ptr<arithmetic>&& expr_2) -> unique_ptr<Impl> {
			auto result = make_unique<Impl>();
			result->expr_1 = std::move(expr_1);
			result->expr_2 = std::move(expr_2);
			set_parent(result, result->expr_1, result->expr_2);
			return std::move(result);
		}

		auto clone() -> unique_ptr<Impl> {
			return make(expr_1->clone(), expr_2->clone());
		}
	};

	struct add : arithmetic_op<add> {};
	struct mul : arithmetic_op<mul> {};
	struct sub : arithmetic_op<sub> {};
	struct div : arithmetic_op<div> {};
	struct mod : arithmetic_op<mod> {};
	struct exp : arithmetic_op<exp> {};

	enum comparison_enum { EQ, NEQ, GT, LT, GTE, LTE };
	struct comparison : node_impl<comparison> {
		unique_ptr<arithmetic> lhs, rhs;
		comparison_enum comparison_type;

		static auto make(unique_ptr<arithmetic>&& lhs, unique_ptr<arithmetic>&& rhs,
			comparison_enum comparison_type) -> unique_ptr<comparison>;
		auto clone() -> unique_ptr<comparison>;
	};

	struct logical : node_impl<logical> {
		using logical_expr = variant<
			unique_ptr<and_op>, unique_ptr<or_op>,
			unique_ptr<field>, unique_ptr<val_bool>, unique_ptr<comparison>, unique_ptr<negated>>;
		logical_expr expr;

		static auto make(logical_expr&& expr) -> unique_ptr<logical>;
		auto clone() -> unique_ptr<logical>;
	};

	template <typename Impl>
	struct logical_op : node_impl<Impl> {
		unique_ptr<logical> expr_1, expr_2;

		static auto make(unique_ptr<logical>&& expr_1, unique_ptr<logical>&& expr_2) -> unique_ptr<Impl> {
			auto result = make_unique<Impl>();
			result->expr_1 = std::move(expr_1);
			result->expr_2 = std::move(expr_2);
			set_parent(result, result->expr_1, result->expr_2);
			return std::move(result);
		}

		auto clone() -> unique_ptr<Impl> {
			return make(expr_1->clone(), expr_2->clone());
		}
	};
	struct and_op : logical_op<and_op> {};
	struct or_op : logical_op<or_op> {};

	struct negated : node_impl<negated> {
		unique_ptr<logical> expr;

		static auto make(unique_ptr<logical>&& expr) -> unique_ptr<negated>;
		auto clone() -> unique_ptr<negated>;
	};

	enum assignment_enum { ABSOLUTE, RELATIVE };
	struct assignment : node_impl<assignment> {
		using rhs_t = variant<unique_ptr<arithmetic>, unique_ptr<logical>>;

		unique_ptr<field> lhs;
		assignment_enum assignment_type;
		rhs_t rhs;

		static auto make(unique_ptr<field>&& lhs, assignment_enum assignment_type, rhs_t&& rhs) -> unique_ptr<assignment>;
		auto clone() -> unique_ptr<assignment>;
	};

	struct always_body;
	struct continuous_if : node_impl<continuous_if> {
		unique_ptr<logical> condition;
		unique_ptr<always_body> body;

		static auto make(unique_ptr<logical>&& condition, unique_ptr<always_body>&& body) -> unique_ptr<continuous_if>;
		auto clone() -> unique_ptr<continuous_if>;
	};

	struct transition_if : node_impl<transition_if> {
		unique_ptr<logical> condition;
		unique_ptr<always_body> body;

		static auto make(unique_ptr<logical>&& condition, unique_ptr<always_body>&& body) -> unique_ptr<transition_if>;
		auto clone() -> unique_ptr<transition_if>;
	};

	struct for_in : node_impl<for_in> {
		string variable;
		double range;
		unit_object range_unit;
		vector<string> traits;
		unique_ptr<always_body> body;

		static auto make(string variable, double range, unit_object range_unit, vector<string>& traits,
			unique_ptr<always_body>&& body) -> unique_ptr<for_in>;
		auto clone() -> unique_ptr<for_in>;

		void replace_body(unique_ptr<always_body>&& new_body);
		// Returns the loop the range_unit was declared in if the unit is an identifier_unit
		auto get_loop_from_identifier() -> for_in*;
	};

	using expression = variant<unique_ptr<assignment>, unique_ptr<continuous_if>, unique_ptr<transition_if>, unique_ptr<for_in>>;
	struct always_body : node_impl<always_body> {
		vector<expression> exprs;

		static auto make(vector<expression>&& exprs) -> unique_ptr<always_body>;
		auto clone() -> unique_ptr<always_body>;
		void insert_expr(expression&& expr);
	};

	struct trait : node_impl<trait> {
		string name;
		unique_ptr<properties> props;
		unique_ptr<always_body> body;

		static auto make(string name, unique_ptr<properties>&& props, unique_ptr<always_body>&& body) -> unique_ptr<trait>;
		auto clone() -> unique_ptr<trait>;
		auto get_property(string name) -> variable_decl*;
	};

	struct trait_initializer : node_impl<trait_initializer> {
		string name;
		map<string, literal_value> initial_values;

		static auto make(string name, map<string, literal_value> initial_values) -> unique_ptr<trait_initializer>;
		auto clone() -> unique_ptr<trait_initializer>;
	};

	struct unit_traits : node_impl<unit_traits> {
		string name;
		vector<unique_ptr<trait_initializer>> traits;

		static auto make(string name, vector<unique_ptr<trait_initializer>>&& traits) -> unique_ptr<unit_traits>;
		auto clone() -> unique_ptr<unit_traits>;
		void insert_initializer(unique_ptr<trait_initializer>&& trait);
	};

	struct program : node_impl<program> {
		vector<unique_ptr<trait>> traits;
		vector<unique_ptr<unit_traits>> all_unit_traits;

		static auto make(vector<unique_ptr<trait>>&& traits, vector<unique_ptr<unit_traits>>&& all_unit_traits) -> unique_ptr<program>;
		auto clone() -> unique_ptr<program>;

		void insert_trait(unique_ptr<trait>&& trait);
		auto get_trait(string name) -> trait*;
	};

	template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
	template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

	string print_arithmetic(arithmetic& root);
	string print_logical(logical& root);
	void print_program(program& root);
};
