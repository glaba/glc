#include "print_program.h"
#include "parser.h"

#include <cassert>
#include <memory>

using std::string;
using std::unique_ptr;

print_program::print_program(pass_manager& pm)
	: pm(pm), program(*pm.get_pass<parser>()->program) {}

auto print_program::get_output() -> string {
	return print(program);
}

auto print_program::print(ast::val_bool& v) -> string {
	return v.value ? "true" : "false";
}

auto print_program::print(ast::val_float& v) -> string {
	return std::to_string(v.value);
}

auto print_program::print(ast::val_int& v) -> string {
	return std::to_string(v.value);
}

auto print_program::print(ast::variable_type& v) -> string {
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

auto print_program::print(ast::variable_decl& v) -> string {
	return v.name + " : " + print(*v.type);
}

auto print_program::print(ast::properties& v) -> string {
	auto output = string("\tproperties {\n");
	for (size_t i = 0; i < v.variable_declarations.size(); i++) {
		output += "\t\t" + print(*v.variable_declarations[i]);
		if (i < v.variable_declarations.size() - 1) {
			output += ",";
		}
		output += "\n";
	}
	output += "\t}\n";
	return output;
}

auto print_program::print(ast::field& v) -> string {
	auto output = string();
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

template <typename OpNode, char Op>
auto print_program::print_binary_op(OpNode& v) -> string {
	auto output = "(" + print(*v.expr_1);
	output += " " + std::string(1, Op) + " ";
	output += print(*v.expr_2) + ")";
	return output;
}

auto print_program::print(ast::add& v) -> string {
	return print_binary_op<ast::add, '+'>(v);
}

auto print_program::print(ast::mul& v) -> string {
	return print_binary_op<ast::mul, '*'>(v);
}

auto print_program::print(ast::sub& v) -> string {
	return print_binary_op<ast::sub, '-'>(v);
}

auto print_program::print(ast::div& v) -> string {
	return print_binary_op<ast::div, '/'>(v);
}

auto print_program::print(ast::mod& v) -> string {
	return print_binary_op<ast::mod, '%'>(v);
}

auto print_program::print(ast::exp& v) -> string {
	return print_binary_op<ast::exp, '^'>(v);
}

auto print_program::print(ast::arithmetic_value& v) -> string {
	auto output = string();
	std::visit(ast::overloaded {
		[&] (unique_ptr<ast::field>& val) { output = print(*val); },
		[&] (long val) { output = std::to_string(val); },
		[&] (double val) { output = std::to_string(val); }
	}, v.value);
	return output;
}

auto print_program::print(ast::arithmetic& v) -> string {
	auto output = string("(");
	std::visit([&] (auto& val) {
		output += print(*val);
	}, v.expr);
	output += ")";
	return output;
}

auto print_program::print(ast::comparison& v) -> string {
	auto output = string();
	output += print(*v.lhs);
	switch (v.comparison_type) {
		case ast::comparison_enum::EQ: output += " == "; break;
		case ast::comparison_enum::NEQ: output += " != "; break;
		case ast::comparison_enum::GT: output += " > "; break;
		case ast::comparison_enum::LT: output += " < "; break;
		case ast::comparison_enum::GTE: output += " >= "; break;
		case ast::comparison_enum::LTE: output += " <= "; break;
		default: assert(false);
	}
	output += "(" + print(*v.rhs) + ")";
	return output;
}

auto print_program::print(ast::and_op& v) -> string {
	return "(" + print(*v.expr_1) + " and " + print(*v.expr_2) + ")";
}

auto print_program::print(ast::or_op& v) -> string {
	return "(" + print(*v.expr_1) + " or " + print(*v.expr_2) + ")";
}

auto print_program::print(ast::negated& v) -> string {
	return "not " + print(*v.expr);
}

auto print_program::print(ast::logical& v) -> string {
	auto output = string("(");
	std::visit([&] (auto& val) {
		output += print(*val);
	}, v.expr);
	output += ")";
	return output;
}

auto print_indent(size_t indent) -> string {
	auto output = string();
	for (size_t i = 0; i < indent; i++) {
		output += "\t";
	}
	return output;
}

auto print_program::print(ast::assignment& v, size_t indent) -> string {
	auto output = print_indent(indent) + print(*v.lhs) + " = ";
	std::visit([&] (auto& rhs) {
		output += print(*rhs);
	}, v.rhs);
	output += ";\n";
	return output;
}

auto print_program::print(ast::continuous_if& v, size_t indent) -> string {
	auto output = print_indent(indent) + "if " + print(*v.condition) + " {\n";
	output += print(*v.body, indent + 1);
	output += print_indent(indent) + "}\n";
	return output;
}

auto print_program::print(ast::transition_if& v, size_t indent) -> string {
	auto output = print_indent(indent) + "if becomes " + print(*v.condition) + " {\n";
	output += print(*v.body, indent + 1);
	output += print_indent(indent) + "}\n";
	return output;
}

auto print_program::print(ast::for_in& v, size_t indent) -> string {
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
	output += print(*v.body, indent + 1);
	output += print_indent(indent) + "}\n";
	return output;
}

auto print_program::print(ast::always_body& v, size_t indent) -> string {
	auto output = string();
	for (auto& expr : v.exprs) {
		std::visit([&] (auto& n) {
			output += print(*n, indent);
		}, expr);
	}
	return output;
}

auto print_program::print(ast::trait& v) -> string {
	auto output = "trait " + v.name + " {\n";
	output += print(*v.props);
	output += "\n\talways {\n" + print(*v.body, 2);
	output += "\t}\n";
	output += "}\n\n";
	return output;
}

auto print_program::print(ast::trait_initializer& v) -> string {
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

auto print_program::print(ast::unit_traits& v) -> string {
	auto output = "unit " + v.name + " : ";
	for (auto& trait_init : v.traits) {
		output += print(*trait_init);
	}
	output += ";\n";
	return output;
}

auto print_program::print(ast::program& v) -> string {
	auto output = string();

	for (auto& trait : v.traits) {
		output += print(*trait);
	}

	for (auto& unit_trait : v.all_unit_traits) {
		output += print(*unit_trait);
	}

	return output;
}
