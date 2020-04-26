#pragma once

#include "ast.h"

#include <type_traits>
#include <cassert>
#include <memory>

// Struct whose () operator takes a root AST node and a visitor, and calls the visit methods
// in the provided Visitor on the tree formed by the root AstNode in reverse topological order
template <typename AstNode, typename Visitor>
struct visit;

template <typename T, typename AstNode, typename = void>
struct has_visitor : std::false_type {};

template <typename T, typename AstNode>
struct has_visitor<T, AstNode, std::void_t<decltype(std::declval<T>()(std::declval<AstNode&>()))>> : std::true_type {};

template <typename AstNode, typename Visitor, typename Impl>
struct default_visit {
	void operator()(AstNode& n, Visitor& visitor) {
		// Visit children first so that nodes are traversed in reverse topological order
		Impl::visit_children(n, visitor);
		// Default behavior for any AST node is to visit it
		if constexpr (has_visitor<Visitor, AstNode>::value) {
			visitor(n);
		}
	}
};

template <typename AstNode, typename Visitor>
struct no_children { static void visit_children(AstNode& n, Visitor& visitor) {} };

template <typename AstNode, typename Visitor>
struct visit : default_visit<AstNode, Visitor, visit<AstNode, Visitor>>, no_children<AstNode, Visitor> {};

template <typename Visitor>
struct visit<ast::variable_decl, Visitor> : default_visit<ast::variable_decl, Visitor, visit<ast::variable_decl, Visitor>> {
	static void visit_children(ast::variable_decl& n, Visitor& visitor) {
		visit<ast::variable_type, Visitor>()(*n.type, visitor);
	}
};

template <typename Visitor>
struct visit<ast::properties, Visitor> : default_visit<ast::properties, Visitor, visit<ast::properties, Visitor>> {
	static void visit_children(ast::properties& n, Visitor& visitor) {
		for (auto& decl : n.variable_declarations) {
			visit<ast::variable_decl, Visitor>()(*decl, visitor);
		}
	}
};

template <typename ChildType, typename Op, typename Visitor>
struct visit_binary_op : default_visit<Op, Visitor, visit_binary_op<ChildType, Op, Visitor>> {
	static void visit_children(Op& n, Visitor& visitor) {
		visit<ChildType, Visitor>()(*n.expr_1, visitor);
		visit<ChildType, Visitor>()(*n.expr_2, visitor);
	}
};

template <typename Visitor> struct visit<ast::add, Visitor> : visit_binary_op<ast::arithmetic, ast::add, Visitor> {};
template <typename Visitor> struct visit<ast::sub, Visitor> : visit_binary_op<ast::arithmetic, ast::sub, Visitor> {};
template <typename Visitor> struct visit<ast::mul, Visitor> : visit_binary_op<ast::arithmetic, ast::mul, Visitor> {};
template <typename Visitor> struct visit<ast::div, Visitor> : visit_binary_op<ast::arithmetic, ast::div, Visitor> {};
template <typename Visitor> struct visit<ast::mod, Visitor> : visit_binary_op<ast::arithmetic, ast::mod, Visitor> {};
template <typename Visitor> struct visit<ast::exp, Visitor> : visit_binary_op<ast::arithmetic, ast::exp, Visitor> {};

// Visits a variant of unique_ptrs, which is a common idiom used in our AST
template <typename Variant, typename Visitor, typename... Types>
struct visit_variant;
template <typename Variant, typename Visitor, typename T, typename... Rest>
struct visit_variant<Variant, Visitor, T, Rest...> {
	static void visit(Variant& v, Visitor& visitor) {
		if (std::holds_alternative<std::unique_ptr<T>>(v)) {
			::visit<T, Visitor>()(*std::get<std::unique_ptr<T>>(v), visitor);
		} else {
			visit_variant<Variant, Visitor, Rest...>::visit(v, visitor);
		}
	}
};
template <typename Variant, typename Visitor>
struct visit_variant<Variant, Visitor> { static void visit(Variant& v, Visitor& visitor) {} };

template <typename Visitor>
struct visit<ast::arithmetic_value, Visitor> : default_visit<ast::arithmetic_value, Visitor, visit<ast::arithmetic_value, Visitor>> {
	static void visit_children(ast::arithmetic_value& n, Visitor& visitor) {
		visit_variant<decltype(n.value), Visitor, ast::field>::visit(n.value, visitor);
	}
};

template <typename Visitor>
struct visit<ast::arithmetic, Visitor> : default_visit<ast::arithmetic, Visitor, visit<ast::arithmetic, Visitor>> {
	static void visit_children(ast::arithmetic& n, Visitor& visitor) {
		visit_variant<decltype(n.expr), Visitor,
			ast::add, ast::mul, ast::sub, ast::div, ast::mod, ast::exp, ast::arithmetic_value>::visit(n.expr, visitor);
	}
};

template <typename Visitor>
struct visit<ast::comparison, Visitor> : default_visit<ast::comparison, Visitor, visit<ast::comparison, Visitor>> {
	static void visit_children(ast::comparison& n, Visitor& visitor) {
		visit<ast::arithmetic, Visitor>()(*n.lhs, visitor);
		visit<ast::arithmetic, Visitor>()(*n.rhs, visitor);
	}
};

template <typename Visitor> struct visit<ast::and_op, Visitor> : visit_binary_op<ast::logical, ast::and_op, Visitor> {};
template <typename Visitor> struct visit<ast::or_op, Visitor> : visit_binary_op<ast::logical, ast::or_op, Visitor> {};

template <typename Visitor>
struct visit<ast::negated, Visitor> : default_visit<ast::negated, Visitor, visit<ast::negated, Visitor>> {
	static void visit_children(ast::negated& n, Visitor& visitor) {
		visit<ast::logical, Visitor>()(*n.expr, visitor);
	}
};

template <typename Visitor>
struct visit<ast::logical, Visitor> : default_visit<ast::logical, Visitor, visit<ast::logical, Visitor>> {
	static void visit_children(ast::logical& n, Visitor& visitor) {
		visit_variant<decltype(n.expr), Visitor,
			ast::and_op, ast::or_op, ast::field, ast::val_bool, ast::comparison, ast::negated>::visit(n.expr, visitor);
	}
};

template <typename Visitor>
struct visit<ast::assignment, Visitor> : default_visit<ast::assignment, Visitor, visit<ast::assignment, Visitor>> {
	static void visit_children(ast::assignment& n, Visitor& visitor) {
		visit<ast::field, Visitor>()(*n.lhs, visitor);
		visit_variant<decltype(n.rhs), Visitor, ast::arithmetic, ast::logical>::visit(n.rhs, visitor);
	}
};

template <typename AstType, typename Visitor>
struct visit_if_expr : default_visit<AstType, Visitor, visit_if_expr<AstType, Visitor>> {
	static void visit_children(AstType& n, Visitor& visitor) {
		visit<ast::logical, Visitor>()(*n.condition, visitor);
		visit<ast::always_body, Visitor>()(*n.body, visitor);
	}
};

template <typename Visitor> struct visit<ast::continuous_if, Visitor> : visit_if_expr<ast::continuous_if, Visitor> {};
template <typename Visitor> struct visit<ast::transition_if, Visitor> : visit_if_expr<ast::transition_if, Visitor> {};

template <typename Visitor>
struct visit<ast::for_in, Visitor> : default_visit<ast::for_in, Visitor, visit<ast::for_in, Visitor>> {
	static void visit_children(ast::for_in& n, Visitor& visitor) {
		visit<ast::always_body, Visitor>()(*n.body, visitor);
	}
};

template <typename Visitor>
struct visit<ast::always_body, Visitor> : default_visit<ast::always_body, Visitor, visit<ast::always_body, Visitor>> {
	static void visit_children(ast::always_body& n, Visitor& visitor) {
		for (auto& expr : n.exprs) {
			visit_variant<decltype(expr), Visitor,
				ast::assignment, ast::continuous_if, ast::transition_if, ast::for_in>::visit(expr, visitor);
		}
	}
};

template <typename Visitor>
struct visit<ast::trait, Visitor> : default_visit<ast::trait, Visitor, visit<ast::trait, Visitor>> {
	static void visit_children(ast::trait& n, Visitor& visitor) {
		visit<ast::properties, Visitor>()(*n.props, visitor);
		visit<ast::always_body, Visitor>()(*n.body, visitor);
	}
};

template <typename Visitor>
struct visit<ast::program, Visitor> : default_visit<ast::program, Visitor, visit<ast::program, Visitor>> {
	static void visit_children(ast::program& n, Visitor& visitor) {
		for (auto& trait : n.traits) {
			visit<ast::trait, Visitor>()(*trait, visitor);
		}
		for (auto& unit_traits : n.all_unit_traits) {
			visit<ast::unit_traits, Visitor>()(*unit_traits, visitor);
		}
	}
};
