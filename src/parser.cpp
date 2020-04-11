#include "parser.h"
#include "ast.h"
#include "pegtl.hpp"

#include <iostream>
#include <cassert>
#include <memory>
#include <vector>
#include <string>
#include <functional>

using namespace tao::pegtl;
using std::unique_ptr;
using std::make_unique;
using std::vector;

template <typename Compare, typename T>
auto isa(T *val) -> bool {
    return dynamic_cast<Compare*>(val) != nullptr;
}

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

    /*** Types ***/
    struct ty_bool : TAO_PEGTL_STRING("bool") {};
    struct ty_float : TAO_PEGTL_STRING("float") {};
    struct ty_int : sseq<TAO_PEGTL_STRING("int"), sopt<one<'<'>, val_int, one<','>, val_int, one<'>'>>> {};
    struct variable_type : sor<ty_bool, ty_float, ty_int> {};

    /*** Always block code ***/
    struct unit : sor<TAO_PEGTL_STRING("this"), TAO_PEGTL_STRING("type"), identifier> {};
    struct member_operator : sor<TAO_PEGTL_STRING("::"), TAO_PEGTL_STRING("."), TAO_PEGTL_STRING("->")> {};
    struct field : sseq<unit, member_operator, identifier> {};

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

    struct assignment : sseq<field, one<'='>, arithmetic> {};

    /*** Program constructs ***/
    struct variable_decl : sseq<identifier, one<':'>, variable_type> {};
    struct properties : sseq<kw_properties, one<'{'>, cslist<variable_decl>, one<'}'>> {};
    struct always : sseq<kw_always, one<'{'>, assignment, one<'}'>> {};
    struct trait : sseq<kw_trait, identifier, one<'{'>, properties, always, one<'}'>> {};

    /*** Root node ***/
    struct program : sstar<trait> {};

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
            data->props = static_cast<ast::properties*>(n.children[1]->data.get());
            data->logic = static_cast<ast::always*>(n.children[2]->data.get());
        }
    };
    using trait_sel = typename selector<trait, ast::trait>::on<rules::trait>;

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

        // For nodes of the form                        o
        //                        |            |                |              |
        //                     arithmetic  op arithmetic  op arithmetic  op arithmetic
        //
        // where op represents an arithmetic operation, such as +, -, *, /, %, or ^,
        // constructs a left-associative parse tree of the corresponding arithmetic expression
        //
        // The rules that this applies to are arithmetic, mul_factor, and exp_factor. Their selectors
        //  must CRTP this class and implement a function identify_op, which takes: an ast_node object
        //  containing the operator, an ast::arithmetic_op object containing two operands, and an ast::arithmetic
        //  object where a casted version of the ast::arithmetic_op object to one of ast::add, ast::mul, etc should be placed
        template <typename Impl>
        struct arithmetic_tree_builder : parse_tree::apply<arithmetic_tree_builder<Impl>> {
            template <typename Node, typename... States>
            static void transform(unique_ptr<Node>& n, States&&... st) {
                if (n->children.size() == 1) {
                    n->data = std::move(n->children[0]->data);
                    return;
                }

                // Construct a left associative tree of operations
                auto cur_root = own_as<ast::arithmetic>(n->children[0]->data);
                for (unsigned i = 1; i < n->children.size(); i++) {
                    auto data = make_unique<ast::arithmetic_op>();
                    data->expr_1 = std::move(cur_root);
                    data->expr_2 = own_as<ast::arithmetic>(n->children[i]->data);

                    // Extract the operator from the current child and provide it to the Impl
                    //  so that the Impl creates the correct node type
                    cur_root = make_unique<ast::arithmetic>();
                    auto& op = n->children[i]->children[0];

                    Impl::identify_op(*op, data, cur_root);
                }

                // Set the final node containing the total "product" as the data for this node in the parse tree
                n->data = std::move(cur_root);

                std::cout << ast::print(*static_cast<ast::arithmetic*>(n->data.get())) << std::endl;
            }
        };

        struct arithmetic : arithmetic_tree_builder<arithmetic> {
            static void identify_op(ast_node& op, unique_ptr<ast::arithmetic_op>& data, unique_ptr<ast::arithmetic>& dest) {
                if (op.is_type<rules::add_op>())
                    dest->expr = own_as<ast::add>(data);
                else if (op.is_type<rules::sub_op>())
                    dest->expr = own_as<ast::sub>(data);
            }
        };
        using arithmetic_sel = typename arithmetic::on<rules::arithmetic>;

        struct mul_factor : arithmetic_tree_builder<mul_factor> {
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

        struct exp_factor : arithmetic_tree_builder<exp_factor> {
            static void identify_op(ast_node& op, unique_ptr<ast::arithmetic_op>& data, unique_ptr<ast::arithmetic>& dest) {
                if (op.is_type<rules::exp_op>())
                    dest->expr = own_as<ast::exp>(data);
            }
        };
        using exp_factor_sel = typename exp_factor::on<rules::exp_factor>;

        // Selector for all the RHS rules of arithmetic expressions, which include add, mul, and exp
        //  which have forms +\- <arithmetic>, *\/\% <arithmetic>, and ^ <arithmetic>, respectively
        // This selector simply takes that data from the second child (<arithmetic>) and sets it as its own
        struct rhs_sel : parse_tree::apply<rhs_sel> {
            template <typename Node, typename... States>
            static void transform(unique_ptr<Node>& n, States&&... st) {
                n->data = std::move(n->children[1]->data);
            }
        };
        using add_sel = typename rhs_sel::on<rules::add>;
        using mul_sel = typename rhs_sel::on<rules::mul>;
        using exp_sel = typename rhs_sel::on<rules::exp>;
    }

    using namespace arithmetic;

    template <typename Rule>
    using ast_selector = parse_tree::selector<Rule,
        val_bool_sel, val_float_sel, val_int_sel,
        empty_sel<ast::ty_bool>::on<rules::ty_bool>, empty_sel<ast::ty_float>::on<rules::ty_float>, ty_int_sel,
        variable_type_sel, variable_decl_sel, properties_sel,
        trait_sel,
        arithmetic_value_sel, arithmetic_sel, mul_factor_sel, exp_factor_sel, add_sel, mul_sel, exp_sel,

        parse_tree::store_content::on<
            rules::add_op, rules::sub_op, rules::mul_op, rules::div_op, rules::mod_op, rules::exp_op, rules::field,
            identifier, rules::always, rules::program
        >
    >;
};


void parser::run() {
    assert(analyze<rules::program>() == 0);

    auto const root = parse_tree::parse<rules::program, ast_node, selectors::ast_selector>(string_input(input_file, ""));
    parse_tree::print_dot(std::cout, *root);

    return;
}