#include "parser.h"
#include "pegtl.hpp"

#include <iostream>
#include <cassert>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <type_traits>
#include <fstream>
#include <sstream>

using namespace tao::pegtl;
using std::unique_ptr;
using std::make_unique;
using std::vector;

/*** Convenience types ***/
template <typename... Ts>
struct sseq : seq<pad<Ts, space>...> {};
template <typename... Ts>
struct sstar : star<pad<Ts, space>...> {};
template <typename... Ts>
struct sopt : opt<pad<Ts, space>...> {};
template <typename T>
struct cslist : opt<list<T, pad<one<','>, space>>> {};
struct spaces : plus<space> {};

namespace rules {
    /*** Values ***/
    struct val_bool : sor<TAO_PEGTL_STRING("true"), TAO_PEGTL_STRING("false")> {};
    struct val_float : seq<opt<one<'-'>>, plus<digit>, one<'.'>, plus<digit>> {};
    struct val_int : seq<opt<one<'-'>>, plus<digit>> {};

    /*** Keywords ***/
    struct kw_trait : TAO_PEGTL_STRING("trait") {};
    struct kw_properties : TAO_PEGTL_STRING("properties") {};
    struct kw_always : TAO_PEGTL_STRING("always") {};
    struct kw_this : TAO_PEGTL_STRING("this") {};
    struct kw_type : TAO_PEGTL_STRING("type") {};
    struct kw_if : TAO_PEGTL_STRING("if") {};
    struct kw_unit : TAO_PEGTL_STRING("unit") {};

    /*** Types ***/
    struct ty_bool : TAO_PEGTL_STRING("bool") {};
    struct ty_float : TAO_PEGTL_STRING("float") {};
    struct ty_int : sseq<TAO_PEGTL_STRING("int"), sopt<one<'<'>, val_int, one<','>, val_int, one<'>'>>> {};
    struct variable_type : sor<ty_bool, ty_float, ty_int> {};

    /*******************************/
    /****** Always block code ******/
    /*******************************/
    struct unit_object : sor<kw_this, kw_type, identifier> {};
    struct member_operator : sor<TAO_PEGTL_STRING("::"), TAO_PEGTL_STRING("."), TAO_PEGTL_STRING("->")> {};
    struct field : sseq<unit_object, member_operator, identifier> {};

    /*** Arithmetic ***/
    struct add_op : one<'+'> {};
    struct sub_op : one<'-'> {};
    struct mul_op : one<'*'> {};
    struct div_op : one<'/'> {};
    struct mod_op : one<'%'> {};
    struct exp_op : one<'^'> {};

    struct arithmetic; struct add; struct mul_factor; struct mul; struct exp_factor; struct exp;
    struct arithmetic_value : sor<field, val_float, val_int, sseq<one<'('>, arithmetic, one<')'>>> {};
    struct arithmetic : sseq<mul_factor, star<add>> {};
    struct add : sseq<sor<add_op, sub_op>, mul_factor> {};
    struct mul_factor : sseq<exp_factor, star<mul>> {};
    struct mul : sseq<sor<mul_op, div_op, mod_op>, exp_factor> {};
    struct exp_factor : sseq<arithmetic_value, opt<exp>> {};
    struct exp : sseq<exp_op, arithmetic_value> {};

    /*** Logical ***/
    struct and_op : TAO_PEGTL_STRING("and") {};
    struct or_op : TAO_PEGTL_STRING("or") {};
    struct not_op : TAO_PEGTL_STRING("not") {};
    struct eq_op : TAO_PEGTL_STRING("==") {};
    struct neq_op : TAO_PEGTL_STRING("!=") {};
    struct gt_op : one<'>'> {};
    struct lt_op : one<'<'> {};
    struct gte_op : seq<one<'>'>, one<'='>> {};
    struct lte_op : seq<one<'<'>, one<'='>> {};

    struct comparison : sseq<arithmetic, sor<eq_op, neq_op, gt_op, lt_op, gte_op, lte_op>, arithmetic> {};

    struct logical; struct negated; struct or_expr; struct and_factor; struct and_expr;
    struct logical_value : sor<comparison, val_bool, negated, sseq<one<'('>, logical, one<')'>>, field> {};
    struct negated : sseq<not_op, logical_value> {};
    struct logical : sseq<and_factor, star<or_expr>> {};
    struct or_expr : sseq<or_op, and_factor> {};
    struct and_factor : sseq<logical_value, star<and_expr>> {};
    struct and_expr : sseq<and_op, logical_value> {};

    struct always_body;
    struct assignment : sseq<field, one<'='>, arithmetic, one<';'>> {};
    struct continuous_if : sseq<kw_if, logical, one<'{'>, always_body, one<'}'>> {};

    struct always_body : sstar<sor<assignment, continuous_if>> {};

    /*** Program constructs ***/
    struct variable_decl : sseq<identifier, one<':'>, variable_type> {};
    struct properties : sseq<kw_properties, one<'{'>, cslist<variable_decl>, one<'}'>> {};
    struct always : sseq<kw_always, one<'{'>, always_body, one<'}'>> {};
    struct trait : sseq<kw_trait, identifier, one<'{'>, properties, always, one<'}'>> {};
    struct unit : sseq<kw_unit, identifier, one<':'>, plus<identifier>, one<';'>> {};

    /*** Root node ***/
    struct program : sstar<sor<trait, unit>> {};
};

/*** Custom node type for parse tree ***/
struct ast_node : parse_tree::node {
    template <typename... States>
    void emplace_back(std::unique_ptr<ast_node>&& child, States&&... st) {
        children.emplace_back(std::move(child));
    }

    unique_ptr<ast::node> data;
    vector<unique_ptr<ast_node>> children;
};

/*** Selectors for AST nodes ***/
namespace selectors {
    template <typename Target, typename Source>
    auto own_as(unique_ptr<Source>& src) -> unique_ptr<Target> {
        return unique_ptr<Target>(static_cast<Target*>(src.release()));
    }

    namespace utility {
        template <typename Impl, typename ASTNodeType>
        struct selector : parse_tree::apply<selector<Impl, ASTNodeType>> {
            template <typename Node, typename... States>
            static void transform(unique_ptr<Node>& n, States&&... st) {
                auto data = make_unique<ASTNodeType>();
                Impl::apply(*n.get(), data.get());
                n->data = std::move(data);
            }
        };

        template <typename ASTNodeType>
        struct empty {
            static void apply(ast_node& n, ASTNodeType *data) {}
        };
        template <typename ASTNodeType>
        using empty_sel = selector<empty<ASTNodeType>, ASTNodeType>;

        // This selector simply takes that data from the Ith child and sets it as its own
        template <unsigned I>
        struct preserve_sel : parse_tree::apply<preserve_sel<I>> {
            template <typename Node, typename... States>
            static void transform(unique_ptr<Node>& n, States&&... st) {
                n->data = std::move(n->children[I]->data);
            }
        };
    };

    using namespace utility;

    struct val_bool {
        static void apply(ast_node& n, ast::val_bool *data) {
            data->value = n.string() == "true" ? true : false;
        }
    };
    using val_bool_sel = typename selector<val_bool, ast::val_bool>::on<rules::val_bool>;

    struct val_float {
        static void apply(ast_node& n, ast::val_float *data) {
            data->value = std::stod(n.string());
        }
    };
    using val_float_sel = typename selector<val_float, ast::val_float>::on<rules::val_float>;

    struct val_int {
        static void apply(ast_node& n, ast::val_int *data) {
            data->value = std::stoi(n.string());
        }
    };
    using val_int_sel = typename selector<val_int, ast::val_int>::on<rules::val_int>;

    struct ty_int {
        static void apply(ast_node& n, ast::ty_int *data) {
            data->min = own_as<ast::val_int>(n.children[0]->data)->value;
            data->max = own_as<ast::val_int>(n.children[1]->data)->value;
        }
    };
    using ty_int_sel = typename selector<ty_int, ast::ty_int>::on<rules::ty_int>;

    struct variable_type {
        static void apply(ast_node& n, ast::variable_type *data) {
            auto child = own_as<ast::ty>(n.children[0]->data);
            data->type = child->get_type();
            if (data->type == ast::type_enum::INT) {
                auto int_child = own_as<ast::ty_int>(child);
                data->min = int_child->min;
                data->max = int_child->max;
            }
        }
    };
    using variable_type_sel = typename selector<variable_type, ast::variable_type>::on<rules::variable_type>;

    struct variable_decl {
        static void apply(ast_node& n, ast::variable_decl *data) {
            data->name = n.children[0]->string();
            data->type = own_as<ast::variable_type>(n.children[1]->data);
        };
    };
    using variable_decl_sel = typename selector<variable_decl, ast::variable_decl>::on<rules::variable_decl>;

    struct properties {
        static void apply(ast_node& n, ast::properties *data) {
            for (auto& child : n.children) {
                data->variable_declarations.push_back(own_as<ast::variable_decl>(child->data));
            }
        }
    };
    using properties_sel = typename selector<properties, ast::properties>::on<rules::properties>;

    struct trait {
        static void apply(ast_node& n, ast::trait *data) {
            data->name = n.children[0]->string();
            data->props = own_as<ast::properties>(n.children[1]->data);
            data->body = own_as<ast::always_body>(n.children[2]->data);
        }
    };
    using trait_sel = typename selector<trait, ast::trait>::on<rules::trait>;

    struct unit {
        static void apply(ast_node& n, ast::unit *data) {
            data->name = n.children[0]->string();
            for (unsigned i = 1; i < n.children.size(); i++) {
                data->traits.push_back(n.children[i]->string());
            }
        }
    };
    using unit_sel = typename selector<unit, ast::unit>::on<rules::unit>;

    struct field {
        static void apply(ast_node& n, ast::field *data) {
            auto& field_type = n.children[0];

            if (field_type->template is_type<rules::kw_this>()) {
                data->field_type = ast::this_field();
            }
            else if (field_type->template is_type<rules::kw_type>()) {
                data->field_type = ast::type_field();
            }
            else if (field_type->template is_type<identifier>()) {
                auto unit = ast::unit_field();
                unit.identifier = field_type->string();
                data->field_type = unit;
            }

            auto& member_op = n.children[1];

            if (member_op->string() == "::")
                data->member_op = ast::member_op_enum::BUILTIN;
            else if (member_op->string() == ".")
                data->member_op = ast::member_op_enum::CUSTOM;
            else if (member_op->string() == "->")
                data->member_op = ast::member_op_enum::LANGUAGE;

            data->field_name = n.children[2]->string();
        }
    };
    using field_sel = typename selector<field, ast::field>::on<rules::field>;

    // For nodes of the form                        o
    //                        |            |                |              |
    //                        T           Op T             Op T           Op T
    //
    // where op represents a binary operation, and T represents a term in the algebra,
    // constructs a left-associative expression tree from the parse tree
    //
    // The rules that this applies to are arithmetic, mul_factor, exp_factor, logical, and and_factor. Their selectors
    //  must CRTP this class and implement a function identify_op, which takes: an ast_node object
    //  containing the operator, an Op object containing two operands, and an object of type T where a
    //  casted version of the generic Op object to a specific AST type representing the operation should be placed
    template <typename Impl, typename T, typename Op>
    struct expression_tree_builder : parse_tree::apply<expression_tree_builder<Impl, T, Op>> {
        template <typename Node, typename... States>
        static void transform(unique_ptr<Node>& n, States&&... st) {
            if (n->children.size() == 1) {
                n->data = std::move(n->children[0]->data);
                return;
            }

            // Construct a left associative tree of operations
            auto cur_root = own_as<T>(n->children[0]->data);
            for (unsigned i = 1; i < n->children.size(); i++) {
                auto data = make_unique<Op>();
                data->expr_1 = std::move(cur_root);
                data->expr_2 = own_as<T>(n->children[i]->data);

                // Extract the operator from the current child and provide it to the Impl
                //  so that the Impl creates the correct node type
                cur_root = make_unique<T>();
                auto& op = n->children[i]->children[0];

                Impl::identify_op(*op, data, cur_root);
            }

            // Print the parsed expression for debugging
            if constexpr (std::is_same<T, ast::logical>::value) {
                DEBUG(std::cout << ast::print_logical(*cur_root) << std::endl);
            } else if constexpr (std::is_same<T, ast::arithmetic>::value) {
                DEBUG(std::cout << ast::print_arithmetic(*cur_root) << std::endl);
            }

            // Set the final node containing the total "product" as the data for this node in the parse tree
            n->data = std::move(cur_root);
        }
    };

    namespace arithmetic {
        // The following selectors generate a tree of nodes of type ast::arithmetic

        struct arithmetic_value : parse_tree::apply<arithmetic_value> {
            template <typename Node, typename... States>
            static void transform(unique_ptr<Node>& n, States&&... st) {
                auto& child = n->children[0];
                auto& child_data = n->children[0]->data;

                if (child->template is_type<rules::arithmetic>()) {
                    n->data = own_as<ast::arithmetic>(child_data);
                    return;
                }

                // If the child is not an arithmetic expression already, we must wrap it in an arithmetic_value object
                auto arithmetic_val = make_unique<ast::arithmetic_value>();
                if (child->template is_type<rules::val_int>())        arithmetic_val->value = own_as<ast::val_int>(child_data);
                else if (child->template is_type<rules::val_float>()) arithmetic_val->value = own_as<ast::val_float>(child_data);
                else if (child->template is_type<rules::field>())     arithmetic_val->value = own_as<ast::field>(child_data);

                auto data = make_unique<ast::arithmetic>();
                data->expr = std::move(arithmetic_val);
                n->data = std::move(data);
            }
        };
        using arithmetic_value_sel = typename arithmetic_value::on<rules::arithmetic_value>;

        struct arithmetic : expression_tree_builder<arithmetic, ast::arithmetic, ast::arithmetic_op> {
            static void identify_op(ast_node& op, unique_ptr<ast::arithmetic_op>& data, unique_ptr<ast::arithmetic>& dest) {
                if (op.is_type<rules::add_op>())
                    dest->expr = own_as<ast::add>(data);
                else if (op.is_type<rules::sub_op>())
                    dest->expr = own_as<ast::sub>(data);
            }
        };
        using arithmetic_sel = typename arithmetic::on<rules::arithmetic>;

        struct mul_factor : expression_tree_builder<mul_factor, ast::arithmetic, ast::arithmetic_op> {
            static void identify_op(ast_node& op, unique_ptr<ast::arithmetic_op>& data, unique_ptr<ast::arithmetic>& dest) {
                if (op.is_type<rules::mul_op>())
                    dest->expr = own_as<ast::mul>(data);
                else if (op.is_type<rules::div_op>())
                    dest->expr = own_as<ast::div>(data);
                else if (op.is_type<rules::mod_op>())
                    dest->expr = own_as<ast::mod>(data);
            }
        };
        using mul_factor_sel = typename mul_factor::on<rules::mul_factor>;

        struct exp_factor : expression_tree_builder<exp_factor, ast::arithmetic, ast::arithmetic_op> {
            static void identify_op(ast_node& op, unique_ptr<ast::arithmetic_op>& data, unique_ptr<ast::arithmetic>& dest) {
                if (op.is_type<rules::exp_op>())
                    dest->expr = own_as<ast::exp>(data);
            }
        };
        using exp_factor_sel = typename exp_factor::on<rules::exp_factor>;

        using add_sel = typename preserve_sel<1>::on<rules::add>;
        using mul_sel = typename preserve_sel<1>::on<rules::mul>;
        using exp_sel = typename preserve_sel<1>::on<rules::exp>;
    }

    namespace logical {
        // The following selectors generate a tree of nodes of type ast::logical

        struct comparison {
            static void apply(ast_node& n, ast::comparison *data) {
                data->lhs = own_as<ast::arithmetic>(n.children[0]->data);
                data->rhs = own_as<ast::arithmetic>(n.children[2]->data);

                auto& op = n.children[1];
                if (op->template is_type<rules::eq_op>()) data->comparison_type = ast::comparison_enum::EQ;
                if (op->template is_type<rules::neq_op>()) data->comparison_type = ast::comparison_enum::NEQ;
                if (op->template is_type<rules::gt_op>()) data->comparison_type = ast::comparison_enum::GT;
                if (op->template is_type<rules::lt_op>()) data->comparison_type = ast::comparison_enum::LT;
                if (op->template is_type<rules::gte_op>()) data->comparison_type = ast::comparison_enum::GTE;
                if (op->template is_type<rules::lte_op>()) data->comparison_type = ast::comparison_enum::LTE;
            };
        };
        using comparison_sel = typename selector<comparison, ast::comparison>::on<rules::comparison>;

        struct negated {
            static void apply(ast_node& n, ast::negated *data) {
                data->expr = own_as<ast::logical>(n.children[0]->data);
            };
        };
        using negated_sel = typename selector<negated, ast::negated>::on<rules::negated>;

        struct logical_value : parse_tree::apply<logical_value> {
            template <typename Node, typename... States>
            static void transform(unique_ptr<Node>& n, States&&... st) {
                auto& child = n->children[0];
                auto& child_data = n->children[0]->data;

                if (child->template is_type<rules::logical>()) {
                    auto temp = own_as<ast::logical>(child_data);
                    n->data = own_as<ast::logical>(temp);
                    return;
                }

                // If the child is not a logical expression already, we must wrap it in one
                // comparison, val_bool, negated, sseq<one<'('>, logical, one<')'>>, field
                auto data = make_unique<ast::logical>();
                if (child->template is_type<rules::field>())
                    data->expr = own_as<ast::field>(child_data);
                else if (child->template is_type<rules::val_bool>())
                    data->expr = own_as<ast::val_bool>(child_data);
                else if (child->template is_type<rules::comparison>())
                    data->expr = own_as<ast::comparison>(child_data);
                else if (child->template is_type<rules::negated>())
                    data->expr = own_as<ast::negated>(child_data);

                n->data = std::move(data);
            }
        };
        using logical_value_sel = typename logical_value::on<rules::logical_value>;

        struct logical : expression_tree_builder<logical, ast::logical, ast::logical_op> {
            static void identify_op(ast_node& op, unique_ptr<ast::logical_op>& data, unique_ptr<ast::logical>& dest) {
                dest->expr = own_as<ast::or_op>(data);
            }
        };
        using logical_sel = typename logical::on<rules::logical>;

        struct and_factor : expression_tree_builder<and_factor, ast::logical, ast::logical_op> {
            static void identify_op(ast_node& op, unique_ptr<ast::logical_op>& data, unique_ptr<ast::logical>& dest) {
                dest->expr = own_as<ast::and_op>(data);
            }
        };
        using and_factor_sel = typename and_factor::on<rules::and_factor>;

        using or_expr_sel = preserve_sel<1>::on<rules::or_expr>;
        using and_expr_sel = preserve_sel<1>::on<rules::and_expr>;
    }

    struct assignment {
        static void apply(ast_node& n, ast::assignment *data) {
            data->lhs = own_as<ast::field>(n.children[0]->data);

            auto& rhs = n.children[1];
            if (rhs->template is_type<rules::arithmetic>()) {
                data->rhs = own_as<ast::arithmetic>(rhs->data);
            } else if (rhs->template is_type<rules::logical>()) {
                data->rhs = own_as<ast::logical>(rhs->data);
            }
        }
    };
    using assignment_sel = typename selector<assignment, ast::assignment>::on<rules::assignment>;

    struct continuous_if {
        static void apply(ast_node& n, ast::continuous_if *data) {
            data->condition = own_as<ast::logical>(n.children[0]->data);
            data->body = own_as<ast::always_body>(n.children[1]->data);
        }
    };
    using continuous_if_sel = typename selector<continuous_if, ast::continuous_if>::on<rules::continuous_if>;

    struct always_body {
        static void apply(ast_node& n, ast::always_body *data) {
            for (auto& child : n.children) {
                if (child->template is_type<rules::assignment>()) {
                    data->exprs.push_back(own_as<ast::assignment>(child->data));
                } else if (child->template is_type<rules::continuous_if>()) {
                    data->exprs.push_back(own_as<ast::continuous_if>(child->data));
                } else {
                    assert("Unhandled child in rule always_body");
                }
            }
        }
    };
    using always_body_sel = typename selector<always_body, ast::always_body>::on<rules::always_body>;

    struct program : parse_tree::apply<program> {
        static unique_ptr<ast::program> program_ast;

        template <typename Node, typename... States>
        static void transform(unique_ptr<Node>& n, States&&... st) {
            auto data = make_unique<ast::program>();
            for (auto& child : n->children) {
                if (child->template is_type<rules::trait>()) {
                    data->traits.push_back(own_as<ast::trait>(child->data));
                } else if (child->template is_type<rules::unit>()) {
                    data->units.push_back(own_as<ast::unit>(child->data));
                } else {
                    assert("Unhandled child in rule program");
                }
            }

            program_ast = std::move(data);
        }
    };
    unique_ptr<ast::program> program::program_ast;
    using program_sel = typename program::on<rules::program>;

    using namespace arithmetic;
    using namespace logical;

    template <typename Rule>
    using ast_selector = parse_tree::selector<Rule,
        val_bool_sel, val_float_sel, val_int_sel,
        empty_sel<ast::ty_bool>::on<rules::ty_bool>, empty_sel<ast::ty_float>::on<rules::ty_float>, ty_int_sel,
        variable_type_sel, variable_decl_sel, properties_sel,
        field_sel,
        arithmetic_value_sel, arithmetic_sel, mul_factor_sel, exp_factor_sel, add_sel, mul_sel, exp_sel,
        comparison_sel, negated_sel, logical_value_sel, logical_sel, and_factor_sel, or_expr_sel, and_expr_sel,
        assignment_sel, continuous_if_sel,
        always_body_sel,
        trait_sel, unit_sel, program_sel,

        parse_tree::store_content::on<
            identifier,
            rules::member_operator, rules::kw_this, rules::kw_type,
            rules::add_op, rules::sub_op, rules::mul_op, rules::div_op, rules::mod_op, rules::exp_op,
            rules::and_op, rules::or_op,
            rules::eq_op, rules::neq_op, rules::gt_op, rules::lt_op, rules::gte_op, rules::lte_op
        >
    >;
};

auto parser::run() -> unique_ptr<ast::program> {
    assert(analyze<rules::program>() == 0);

    std::ifstream file(input_file);
    std::stringstream buffer;
    buffer << file.rdbuf();

    auto const root = parse_tree::parse<rules::program, ast_node, selectors::ast_selector>(string_input(buffer.str(), ""));
    DEBUG(parse_tree::print_dot(std::cout, *root));

    return std::move(selectors::program::program_ast);
}