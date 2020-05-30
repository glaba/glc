#pragma once

#include "ast.h"
#include "visitor.h"

#include <string>
#include <type_traits>

class print_program {
public:
	print_program(ast::program& program) : program(program) {}

	auto get_output() -> std::string {
		return get_output_for_node(program);
	}

	template <typename CustomPrinter>
	auto get_output(CustomPrinter*** cp = nullptr) -> std::string {
		return get_output_for_node<ast::program, CustomPrinter>(program);
	}

	template <typename T>
	auto get_output_for_node(T& v) -> std::string {
		return get_output_for_node<T, void>(v);
	}

	template <typename T, typename CustomPrinter>
	auto get_output_for_node(T& v, CustomPrinter*** cp = nullptr) -> std::string {
		return print<CustomPrinter>(v);
	}

private:
	template <typename T, typename AstNode, typename = void>
	struct has_printer : std::false_type {};

	template <typename T, typename AstNode>
	struct has_printer<T, AstNode,
		std::void_t<decltype(std::declval<T>()(std::declval<print_program&>(), std::declval<AstNode&>()))>> : std::true_type {};

	template <typename CustomPrinter, typename T, typename... Args>
	auto print(T& v, Args... args) -> std::string {
		if constexpr (std::is_same<CustomPrinter, void>::value) {
			return print_impl<CustomPrinter>(v, args...);
		} else {
			if constexpr (has_printer<CustomPrinter, T>::value) {
				return CustomPrinter()(*this, v);
			} else {
				return print_impl<CustomPrinter>(v, args...);
			}
		}
	}

	template <typename CustomPrinter> auto print_impl(ast::val_bool& v) -> std::string {
		return v.value ? "true" : "false";
	}

	template <typename CustomPrinter> auto print_impl(ast::val_float& v) -> std::string {
		return std::to_string(v.value);
	}

	template <typename CustomPrinter> auto print_impl(ast::val_int& v) -> std::string {
		return std::to_string(v.value);
	}

	template <typename CustomPrinter> auto print_impl(ast::variable_type& v) -> std::string {
		switch (v.type) {
			case ast::type_enum::BOOL:
				return "bool";
			case ast::type_enum::INT:
				return "int<" + std::to_string(v.min) + ", " + std::to_string(v.max) + ">";
			case ast::type_enum::FLOAT:
				return "float";
			default:
				assert(false);
		}
	}

	template <typename CustomPrinter> auto print_impl(ast::variable_decl& v) -> std::string {
		return v.name + " : " + print<CustomPrinter>(*v.type);
	}

	template <typename CustomPrinter> auto print_impl(ast::properties& v) -> std::string {
		auto output = std::string("\tproperties {\n");
		for (size_t i = 0; i < v.variable_declarations.size(); i++) {
			output += "\t\t" + print<CustomPrinter>(*v.variable_declarations[i]);
			if (i < v.variable_declarations.size() - 1) {
				output += ",";
			}
			output += "\n";
		}
		output += "\t}\n";
		return output;
	}

	template <typename CustomPrinter> auto print_impl(ast::field& v) -> std::string {
		auto output = std::string();
		std::visit(ast::overloaded {
			[&] (ast::this_unit _) { output = "this"; },
			[&] (ast::type_unit _) { output = "type"; },
			[&] (ast::identifier_unit u) { output = u.identifier; },
		}, v.unit);
		switch (v.member_op) {
			case ast::member_op_enum::BUILTIN: output += "::"; break;
			case ast::member_op_enum::CUSTOM: output += "."; break;
			case ast::member_op_enum::LANGUAGE: output += "->"; break;
			default: assert(false);
		}
		output += v.field_name;
		return output;
	}

	template <typename OpNode, char Op, typename CustomPrinter>
	auto print_binary_op(OpNode& v) -> std::string {
		auto output = "(" + print<CustomPrinter>(*v.expr_1);
		output += " " + std::string(1, Op) + " ";
		output += print<CustomPrinter>(*v.expr_2) + ")";
		return output;
	}

	template <typename CustomPrinter> auto print_impl(ast::add& v) -> std::string {
		return print_binary_op<ast::add, '+', CustomPrinter>(v);
	}

	template <typename CustomPrinter> auto print_impl(ast::mul& v) -> std::string {
		return print_binary_op<ast::mul, '*', CustomPrinter>(v);
	}

	template <typename CustomPrinter> auto print_impl(ast::sub& v) -> std::string {
		return print_binary_op<ast::sub, '-', CustomPrinter>(v);
	}

	template <typename CustomPrinter> auto print_impl(ast::div& v) -> std::string {
		return print_binary_op<ast::div, '/', CustomPrinter>(v);
	}

	template <typename CustomPrinter> auto print_impl(ast::mod& v) -> std::string {
		return print_binary_op<ast::mod, '%', CustomPrinter>(v);
	}

	template <typename CustomPrinter> auto print_impl(ast::exp& v) -> std::string {
		return print_binary_op<ast::exp, '^', CustomPrinter>(v);
	}

	template <typename CustomPrinter> auto print_impl(ast::arithmetic_value& v) -> std::string {
		auto output = std::string();
		std::visit(ast::overloaded {
			[&] (std::unique_ptr<ast::field>& val) { output = print<CustomPrinter>(*val); },
			[&] (long val) { output = std::to_string(val); },
			[&] (double val) { output = std::to_string(val); }
		}, v.value);
		return output;
	}

	template <typename CustomPrinter> auto print_impl(ast::arithmetic& v) -> std::string {
		auto output = std::string("(");
		std::visit([&] (auto& val) {
			output += print<CustomPrinter>(*val);
		}, v.expr);
		output += ")";
		return output;
	}

	template <typename CustomPrinter> auto print_impl(ast::comparison& v) -> std::string {
		auto output = std::string();
		output += print<CustomPrinter>(*v.lhs);
		switch (v.comparison_type) {
			case ast::comparison_enum::EQ: output += " == "; break;
			case ast::comparison_enum::NEQ: output += " != "; break;
			case ast::comparison_enum::GT: output += " > "; break;
			case ast::comparison_enum::LT: output += " < "; break;
			case ast::comparison_enum::GTE: output += " >= "; break;
			case ast::comparison_enum::LTE: output += " <= "; break;
			default: assert(false);
		}
		output += "(" + print<CustomPrinter>(*v.rhs) + ")";
		return output;
	}

	template <typename CustomPrinter> auto print_impl(ast::and_op& v) -> std::string {
		return "(" + print<CustomPrinter>(*v.expr_1) + " and " + print<CustomPrinter>(*v.expr_2) + ")";
	}

	template <typename CustomPrinter> auto print_impl(ast::or_op& v) -> std::string {
		return "(" + print<CustomPrinter>(*v.expr_1) + " or " + print<CustomPrinter>(*v.expr_2) + ")";
	}

	template <typename CustomPrinter> auto print_impl(ast::negated& v) -> std::string {
		return "not " + print<CustomPrinter>(*v.expr);
	}

	template <typename CustomPrinter> auto print_impl(ast::logical& v) -> std::string {
		auto output = std::string("(");
		std::visit([&] (auto& val) {
			output += print<CustomPrinter>(*val);
		}, v.expr);
		output += ")";
		return output;
	}

	auto print_indent(size_t indent = 0) -> std::string {
		auto output = std::string();
		for (size_t i = 0; i < indent; i++) {
			output += "\t";
		}
		return output;
	}

	template <typename CustomPrinter> auto print_impl(ast::assignment& v, size_t indent = 0) -> std::string {
		auto output = print_indent(indent) + print<CustomPrinter>(*v.lhs) + " = ";
		std::visit([&] (auto& rhs) {
			output += print<CustomPrinter>(*rhs);
		}, v.rhs);
		output += ";\n";
		return output;
	}

	template <typename CustomPrinter> auto print_impl(ast::continuous_if& v, size_t indent = 0) -> std::string {
		auto output = print_indent(indent) + "if " + print<CustomPrinter>(*v.condition) + " {\n";
		output += print<CustomPrinter>(*v.body, indent + 1);
		output += print_indent(indent) + "}\n";
		return output;
	}

	template <typename CustomPrinter> auto print_impl(ast::transition_if& v, size_t indent = 0) -> std::string {
		auto output = print_indent(indent) + "if becomes " + print<CustomPrinter>(*v.condition) + " {\n";
		output += print<CustomPrinter>(*v.body, indent + 1);
		output += print_indent(indent) + "}\n";
		return output;
	}

	template <typename CustomPrinter> auto print_impl(ast::for_in& v, size_t indent = 0) -> std::string {
		auto output = print_indent(indent) + "for " + v.variable + " in range " + std::to_string(v.range) +
			" of ";
		std::visit(ast::overloaded {
			[&] (ast::this_unit _) { output += "this"; },
			[&] (ast::type_unit _) { output += "type"; },
			[&] (ast::identifier_unit u) { output += u.identifier; },
		}, v.range_unit);

		if (!v.traits.empty()) {
			output += " with trait ";
			for (size_t i = 0; i < v.traits.size(); i++) {
				output += v.traits[i];
				if (i < v.traits.size() - 1) {
					output += ", ";
				}
			}
		}
		output += " {\n";
		output += print<CustomPrinter>(*v.body, indent + 1);
		output += print_indent(indent) + "}\n";
		return output;
	}

	template <typename CustomPrinter> auto print_impl(ast::always_body& v, size_t indent = 0) -> std::string {
		auto output = std::string();
		for (auto& expr : v.exprs) {
			std::visit([&] (auto& n) {
				output += print<CustomPrinter>(*n, indent);
			}, expr);
		}
		return output;
	}

	template <typename CustomPrinter> auto print_impl(ast::trait& v) -> std::string {
		auto output = "trait " + v.name + " {\n";
		output += print<CustomPrinter>(*v.props);
		output += "\n\talways {\n" + print<CustomPrinter>(*v.body, 2);
		output += "\t}\n";
		output += "}\n\n";
		return output;
	}

	template <typename CustomPrinter> auto print_impl(ast::trait_initializer& v) -> std::string {
		auto output = v.name;
		if (!v.initial_values.empty()) {
			output += "(";
			for (auto it = v.initial_values.begin(); it != v.initial_values.end();) {
				auto& [property, value] = *it;

				output += property + " = ";
				std::visit(ast::overloaded {
					[&] (bool val) { output += val ? "true" : "false"; },
					[&] (double val) { output += std::to_string(val); },
					[&] (long val) { output += std::to_string(val); }
				}, value);

				if (++it != v.initial_values.end()) {
					output += ", ";
				}
			}
			output += ")";
		}
		return output;
	}

	template <typename CustomPrinter> auto print_impl(ast::unit_traits& v) -> std::string {
		auto output = "unit " + v.name + " : ";
		for (auto& trait_init : v.traits) {
			output += print<CustomPrinter>(*trait_init);
		}
		output += ";\n";
		return output;
	}

	template <typename CustomPrinter> auto print_impl(ast::program& v) -> std::string {
		auto output = std::string();

		for (auto& trait : v.traits) {
			output += print<CustomPrinter>(*trait);
		}

		for (auto& unit_trait : v.all_unit_traits) {
			output += print<CustomPrinter>(*unit_trait);
		}

		return output;
	}

	ast::program& program;
};
