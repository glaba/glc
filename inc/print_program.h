#pragma once

#include "ast.h"
#include "pass_manager.h"

#include <string>

class print_program : public pass {
public:
	print_program(pass_manager& pm);

	auto get_output() -> std::string;

private:
	auto print(ast::val_bool& v) -> std::string;
	auto print(ast::val_float& v) -> std::string;
	auto print(ast::val_int& v) -> std::string;
	auto print(ast::variable_type& v) -> std::string;
	auto print(ast::variable_decl& v) -> std::string;
	auto print(ast::properties& v) -> std::string;
	auto print(ast::field& v) -> std::string;
	template <typename OpNode, char Op>
	auto print_binary_op(OpNode& v) -> std::string;
	auto print(ast::add& v) -> std::string;
	auto print(ast::mul& v) -> std::string;
	auto print(ast::sub& v) -> std::string;
	auto print(ast::div& v) -> std::string;
	auto print(ast::mod& v) -> std::string;
	auto print(ast::exp& v) -> std::string;
	auto print(ast::arithmetic_value& v) -> std::string;
	auto print(ast::arithmetic& v) -> std::string;
	auto print(ast::comparison& v) -> std::string;
	auto print(ast::and_op& v) -> std::string;
	auto print(ast::or_op& v) -> std::string;
	auto print(ast::negated& v) -> std::string;
	auto print(ast::logical& v) -> std::string;
	auto print(ast::assignment& v, size_t indent) -> std::string;
	auto print(ast::continuous_if& v, size_t indent) -> std::string;
	auto print(ast::transition_if& v, size_t indent) -> std::string;
	auto print(ast::for_in& v, size_t indent) -> std::string;
	auto print(ast::always_body& v, size_t indent) -> std::string;
	auto print(ast::trait& v) -> std::string;
	auto print(ast::trait_initializer& v) -> std::string;
	auto print(ast::unit_traits& v) -> std::string;
	auto print(ast::program& v) -> std::string;

	pass_manager& pm;
	ast::program& program;
};
