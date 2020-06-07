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
#include <map>
#include <tuple>

using namespace tao::pegtl;
using std::unique_ptr;
using std::make_unique;
using std::vector;
using std::map;
using std::tuple;

/*** Convenience types ***/
template <typename... Ts>
struct sseq : seq<pad<Ts, space>...> {};
template <typename... Ts>
struct sstar : star<pad<Ts, space>...> {};
template <typename... Ts>
struct sopt : opt<pad<Ts, space>...> {};
template <typename T>
struct cslist : list<T, pad<one<','>, space>> {};
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
    struct kw_for : TAO_PEGTL_STRING("for") {};
    struct kw_in_range : sseq<TAO_PEGTL_STRING("in"), TAO_PEGTL_STRING("range")> {};
    struct kw_of : TAO_PEGTL_STRING("of") {};
    struct kw_with_trait : sseq<TAO_PEGTL_STRING("with"), TAO_PEGTL_STRING("trait")> {};
    struct kw_unit : TAO_PEGTL_STRING("unit") {};
    struct kw_becomes : TAO_PEGTL_STRING("becomes") {};
    struct kw_abs_assignment : TAO_PEGTL_STRING(":=") {};
    struct kw_rel_assignment : TAO_PEGTL_STRING("+=") {};
    struct kw_rate_field : TAO_PEGTL_STRING("->rate") {};

    /*** Types ***/
    struct ty_bool : TAO_PEGTL_STRING("bool") {};
    struct ty_float : TAO_PEGTL_STRING("float") {};
    struct ty_int : sseq<TAO_PEGTL_STRING("int"), one<'<'>, val_int, one<','>, val_int, one<'>'>> {};
    struct variable_type : sor<ty_bool, ty_float, ty_int> {};

    /*******************************/
    /****** Always block code ******/
    /*******************************/
    struct unit_object : sor<kw_this, kw_type, identifier> {};
    struct member_operator : sor<TAO_PEGTL_STRING("::"), TAO_PEGTL_STRING("."), TAO_PEGTL_STRING("->")> {};
    // TODO: hacky addition to add on rate, will change if more subfields are added on
    struct field : sseq<unit_object, member_operator, identifier, opt<kw_rate_field>> {};

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

    struct comparison : sseq<arithmetic, sor<eq_op, neq_op, gte_op, lte_op, gt_op, lt_op>, arithmetic> {};

    struct logical; struct negated; struct or_expr; struct and_factor; struct and_expr;
    struct logical_value : sor<comparison, val_bool, negated, sseq<one<'('>, logical, one<')'>>, field> {};
    struct negated : sseq<not_op, logical_value> {};
    struct logical : sseq<and_factor, star<or_expr>> {};
    struct or_expr : sseq<or_op, and_factor> {};
    struct and_factor : sseq<logical_value, star<and_expr>> {};
    struct and_expr : sseq<and_op, logical_value> {};

    /*** Always body and expressions ***/
    struct always_body;
    struct assignment : sseq<field, sor<kw_abs_assignment, kw_rel_assignment>, sor<arithmetic, logical>, one<';'>> {};
    struct continuous_if : sseq<kw_if, logical, one<'{'>, always_body, one<'}'>> {};
    struct transition_if : sseq<kw_if, kw_becomes, logical, one<'{'>, always_body, one<'}'>> {};
    struct for_in : sseq<kw_for, identifier, kw_in_range, sor<val_float, val_int>, kw_of, unit_object,
        opt<kw_with_trait, cslist<identifier>>, one<'{'>, always_body, one<'}'>> {};

    struct always_body : sstar<sor<assignment, continuous_if, transition_if, for_in>> {};

    /*** Program constructs ***/
    struct variable_decl : sseq<identifier, one<':'>, variable_type> {};
    struct properties : sseq<kw_properties, one<'{'>, opt<cslist<variable_decl>>, one<'}'>> {};
    struct always : sseq<kw_always, one<'{'>, always_body, one<'}'>> {};
    struct trait : sseq<kw_trait, identifier, one<'{'>, properties, always, one<'}'>> {};

    struct trait_property_init : sseq<identifier, one<'='>, sor<val_bool, val_float, val_int>> {};
    struct trait_initializer : sseq<identifier, opt<one<'('>, cslist<trait_property_init>, one<')'>>> {};
    struct unit_traits : sseq<kw_unit, identifier, one<':'>, cslist<trait_initializer>, one<';'>> {};

    /*** Root node ***/
    struct program : sseq<sstar<sor<trait, unit_traits>>, eof> {};
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

// Pointer to pass manager to report errors
pass_manager* pm;

// The latest position reached by the parser in case of parsing error
size_t latest_line, latest_col;

// Stores input to parser that will be used to track line / col numbers
// Input is created by concatenating all input files together
unique_ptr<string_input<>> input;

// Map from starting line number in input to corresponding input file
map<size_t, std::string> line_to_file;

// Stores resulting program AST
unique_ptr<ast::program> program_ast;

/*** Selectors for AST nodes ***/
namespace selectors {
    template <typename Target, typename Source>
    auto own_as(unique_ptr<Source>& src) -> unique_ptr<Target> {
        return unique_ptr<Target>(static_cast<Target*>(src.release()));
    }

    namespace utility {
        static auto get_pos_in_file(size_t line_number) -> tuple<std::string, size_t> {
            auto result_file = std::string();
            auto result_line = 0UL;
            for (auto& [line, filename] : line_to_file) {
                if (line_number >= line) {
                    result_file = filename;
                    result_line = line_number - line;
                }
            }
            return {result_file, result_line};
        }

        // Add tracking info of filename, line number, and column to the given AST node
        template <typename AstNode>
        static void add_tracking(AstNode& n) {
            // Find the filename and its start line
            auto const& [filename, line] = get_pos_in_file(input->line());
            n.filename() = filename;
            n.line() = line;
            n.col() = input->byte_in_line() + 1;
        }

        // Base selector class used by all others that sets line and col number
        // Expects that n->data is set by Impl
        template <typename Impl>
        struct generic_selector : parse_tree::apply<generic_selector<Impl>> {
            template <typename Node, typename... States>
            static void transform(unique_ptr<Node>& n, States&&... st) {
                latest_line = input->line();
                latest_col = input->byte_in_line() + 1;

                Impl::transform(n);

                if (n->data->get_id() == ast::program::id()) {
                    program_ast = own_as<ast::program>(n->data);
                }
            }
        };

        // Selector class used when producing only one type of AST node
        template <typename Impl, typename ASTNodeType>
        struct selector : generic_selector<selector<Impl, ASTNodeType>> {
            template <typename Node>
            static void transform(unique_ptr<Node>& n) {
                auto data = make_unique<ASTNodeType>();
                add_tracking(*data);

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
        struct preserve_sel : generic_selector<preserve_sel<I>> {
            template <typename Node>
            static void transform(unique_ptr<Node>& n) {
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
            try {
                data->value = std::stod(n.string());
            } catch (...) {
                pm->error<parser>(*data, "Float value " + n.string() + " is out of bounds");
            }
        }
    };
    using val_float_sel = typename selector<val_float, ast::val_float>::on<rules::val_float>;

    struct val_int {
        static void apply(ast_node& n, ast::val_int *data) {
            try {
                data->value = std::stol(n.string());
            } catch (...) {
                pm->error<parser>(*data, "Integer value " + n.string() + " is out of bounds");
            }
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
            auto type_id = n.children[0]->data->get_id();

            if (type_id == ast::ty_bool::id()) {
                data->type = ast::type_enum::BOOL;
            }
            else if (type_id == ast::ty_float::id()) {
                data->type = ast::type_enum::FLOAT;
            }
            else if (type_id == ast::ty_int::id()) {
                data->type = ast::type_enum::INT;
                auto int_child = own_as<ast::ty_int>(n.children[0]->data);
                data->min = int_child->min;
                data->max = int_child->max;
            }
            else {
                assert(false && "Unexpected type");
            }
        }
    };
    using variable_type_sel = typename selector<variable_type, ast::variable_type>::on<rules::variable_type>;

    struct variable_decl {
        static void apply(ast_node& n, ast::variable_decl *data) {
            data->name = n.children[0]->string();
            data->type = own_as<ast::variable_type>(n.children[1]->data);
            ast::set_parent(data, data->type);
        };
    };
    using variable_decl_sel = typename selector<variable_decl, ast::variable_decl>::on<rules::variable_decl>;

    struct properties {
        static void apply(ast_node& n, ast::properties *data) {
            for (auto& child : n.children) {
                ast::set_parent(data, child->data);
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
            ast::set_parent(data, data->props, data->body);
        }
    };
    using trait_sel = typename selector<trait, ast::trait>::on<rules::trait>;

    auto parse_unit_object(ast_node& n) -> ast::unit_object {
        auto unit = ast::unit_object();

        if (n.is_type<rules::kw_this>()) {
            unit = ast::this_unit();
        }
        else if (n.is_type<rules::kw_type>()) {
            unit = ast::type_unit();
        }
        else if (n.is_type<identifier>()) {
            unit = ast::identifier_unit(n.string());
        }
        else {
            assert(false && "Provided parse tree node does not contain a unit object");
        }

        return unit;
    }

    struct field {
        static void apply(ast_node& n, ast::field *data) {
            data->unit = parse_unit_object(*n.children[0]);

            auto& member_op = n.children[1];

            if (member_op->string() == "::")
                data->member_op = ast::member_op_enum::BUILTIN;
            else if (member_op->string() == ".")
                data->member_op = ast::member_op_enum::CUSTOM;
            else if (member_op->string() == "->")
                data->member_op = ast::member_op_enum::LANGUAGE;

            data->field_name = n.children[2]->string();
            data->is_rate = n.children.size() > 3;
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
    //  must CRTP this class and implement a function create_op: this function takes the rule corresponding to the
    //  operation, and the LHS and RHS expressions, and returns a unique_ptr<T> representing the binary operation,
    //  setting all parent-child relationships correctly. It can do this by using the construct_op function in this struct
    template <typename Impl, typename T>
    struct expression_tree_builder : generic_selector<expression_tree_builder<Impl, T>> {
        template <typename Node>
        static void transform(unique_ptr<Node>& n) {
            if (n->children.size() == 1) {
                n->data = std::move(n->children[0]->data);
                return;
            }

            // Construct a left associative tree of operations
            auto cur_root = own_as<T>(n->children[0]->data);
            for (unsigned i = 1; i < n->children.size(); i++) {
                auto expr_1 = std::move(cur_root);
                auto expr_2 = own_as<T>(n->children[i]->data);

                // Extract the operator from the current child and provide it to the Impl
                //  so that the Impl creates the correct node type
                auto& op = n->children[i]->children[0];
                cur_root = Impl::create_op(*op, std::move(expr_1), std::move(expr_2));
            }

            // Set the final node containing the total "product" as the data for this node in the parse tree
            n->data = std::move(cur_root);
        }

        template <typename Op>
        static auto construct_op(unique_ptr<T>&& expr_1, unique_ptr<T>&& expr_2) -> unique_ptr<T> {
            auto op = make_unique<Op>();
            op->col() = expr_2->col();
            op->line() = expr_2->line();
            op->filename() = expr_2->filename();

            op->expr_1 = std::move(expr_1);
            op->expr_2 = std::move(expr_2);
            ast::set_parent(op, op->expr_1, op->expr_2);

            auto wrapper = make_unique<T>();
            wrapper->col() = op->col();
            wrapper->line() = op->line();
            wrapper->filename() = op->filename();

            ast::set_parent(wrapper, op);
            wrapper->expr = std::move(op);
            return wrapper;
        }
    };

    namespace arithmetic {
        // The following selectors generate a tree of nodes of type ast::arithmetic

        struct arithmetic_value : generic_selector<arithmetic_value> {
            template <typename Node>
            static void transform(unique_ptr<Node>& n) {
                auto& child = n->children[0];
                auto& child_data = n->children[0]->data;

                if (child->template is_type<rules::arithmetic>()) {
                    n->data = own_as<ast::arithmetic>(child_data);
                    return;
                }

                // If the child is not an arithmetic expression already, we must wrap it in an arithmetic_value object
                auto arithmetic_val = make_unique<ast::arithmetic_value>();
                ast::set_parent(arithmetic_val, child_data);
                if (child->template is_type<rules::val_int>())        arithmetic_val->value = own_as<ast::val_int>(child_data)->value;
                else if (child->template is_type<rules::val_float>()) arithmetic_val->value = own_as<ast::val_float>(child_data)->value;
                else if (child->template is_type<rules::field>())     arithmetic_val->value = own_as<ast::field>(child_data);
                add_tracking(*arithmetic_val);

                auto data = make_unique<ast::arithmetic>();
                ast::set_parent(data, arithmetic_val);
                data->expr = std::move(arithmetic_val);
                n->data = std::move(data);
                add_tracking(*n->data);
            }
        };
        using arithmetic_value_sel = typename arithmetic_value::on<rules::arithmetic_value>;

        struct arithmetic : expression_tree_builder<arithmetic, ast::arithmetic> {
            static auto create_op(ast_node& op, unique_ptr<ast::arithmetic>&& expr_1, unique_ptr<ast::arithmetic>&& expr_2) -> unique_ptr<ast::arithmetic> {
                if (op.is_type<rules::add_op>())
                    return construct_op<ast::add>(std::move(expr_1), std::move(expr_2));
                else if (op.is_type<rules::sub_op>())
                    return construct_op<ast::sub>(std::move(expr_1), std::move(expr_2));
                else
                    assert(false && "Unexpected operation");
            }
        };
        using arithmetic_sel = typename arithmetic::on<rules::arithmetic>;

        struct mul_factor : expression_tree_builder<mul_factor, ast::arithmetic> {
            static auto create_op(ast_node& op, unique_ptr<ast::arithmetic>&& expr_1, unique_ptr<ast::arithmetic>&& expr_2) -> unique_ptr<ast::arithmetic> {
                if (op.is_type<rules::mul_op>())
                    return construct_op<ast::mul>(std::move(expr_1), std::move(expr_2));
                else if (op.is_type<rules::div_op>())
                    return construct_op<ast::div>(std::move(expr_1), std::move(expr_2));
                else if (op.is_type<rules::mod_op>())
                    return construct_op<ast::mod>(std::move(expr_1), std::move(expr_2));
                else
                    assert(false && "Unexpected operation");
            }
        };
        using mul_factor_sel = typename mul_factor::on<rules::mul_factor>;

        struct exp_factor : expression_tree_builder<exp_factor, ast::arithmetic> {
            static auto create_op(ast_node& op, unique_ptr<ast::arithmetic>&& expr_1, unique_ptr<ast::arithmetic>&& expr_2) -> unique_ptr<ast::arithmetic> {
                return construct_op<ast::exp>(std::move(expr_1), std::move(expr_2));
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
                ast::set_parent(data, data->lhs, data->rhs);

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
                ast::set_parent(data, data->expr);
            };
        };
        using negated_sel = typename selector<negated, ast::negated>::on<rules::negated>;

        struct logical_value : generic_selector<logical_value> {
            template <typename Node>
            static void transform(unique_ptr<Node>& n) {
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
                ast::set_parent(data, child_data);
                if (child->template is_type<rules::field>())
                    data->expr = own_as<ast::field>(child_data);
                else if (child->template is_type<rules::val_bool>())
                    data->expr = own_as<ast::val_bool>(child_data);
                else if (child->template is_type<rules::comparison>())
                    data->expr = own_as<ast::comparison>(child_data);
                else if (child->template is_type<rules::negated>())
                    data->expr = own_as<ast::negated>(child_data);

                n->data = std::move(data);
                add_tracking(*n->data);
            }
        };
        using logical_value_sel = typename logical_value::on<rules::logical_value>;

        struct logical : expression_tree_builder<logical, ast::logical> {
            static auto create_op(ast_node& op, unique_ptr<ast::logical>&& expr_1, unique_ptr<ast::logical>&& expr_2) -> unique_ptr<ast::logical> {
                return construct_op<ast::or_op>(std::move(expr_1), std::move(expr_2));
            }
        };
        using logical_sel = typename logical::on<rules::logical>;

        struct and_factor : expression_tree_builder<and_factor, ast::logical> {
            static auto create_op(ast_node& op, unique_ptr<ast::logical>&& expr_1, unique_ptr<ast::logical>&& expr_2) -> unique_ptr<ast::logical> {
                return construct_op<ast::and_op>(std::move(expr_1), std::move(expr_2));
            }
        };
        using and_factor_sel = typename and_factor::on<rules::and_factor>;

        using or_expr_sel = preserve_sel<1>::on<rules::or_expr>;
        using and_expr_sel = preserve_sel<1>::on<rules::and_expr>;
    }

    struct assignment {
        static void apply(ast_node& n, ast::assignment *data) {
            ast::set_parent(data, n.children[0]->data, n.children[2]->data);

            data->lhs = own_as<ast::field>(n.children[0]->data);

            auto& assignment_type = n.children[1];
            if (assignment_type->template is_type<rules::kw_abs_assignment>()) {
                data->assignment_type = ast::assignment_enum::ABSOLUTE;
            } else if (assignment_type->template is_type<rules::kw_rel_assignment>()) {
                data->assignment_type = ast::assignment_enum::RELATIVE;
            }

            auto& rhs = n.children[2];
            if (rhs->template is_type<rules::arithmetic>()) {
                data->rhs = own_as<ast::arithmetic>(rhs->data);
            } else if (rhs->template is_type<rules::logical>()) {
                data->rhs = own_as<ast::logical>(rhs->data);
            }
        }
    };
    using assignment_sel = typename selector<assignment, ast::assignment>::on<rules::assignment>;

    template <typename IfAstType>
    struct if_expr {
        static void apply(ast_node& n, IfAstType *data) {
            data->condition = own_as<ast::logical>(n.children[0]->data);
            data->body = own_as<ast::always_body>(n.children[1]->data);
            ast::set_parent(data, data->condition, data->body);
        }
    };
    using continuous_if_sel = typename selector<if_expr<ast::continuous_if>, ast::continuous_if>::on<rules::continuous_if>;
    using transition_if_sel = typename selector<if_expr<ast::transition_if>, ast::transition_if>::on<rules::transition_if>;

    struct for_in {
        static void apply(ast_node& n, ast::for_in *data) {
            data->variable = n.children[0]->string();

            if (n.children[1]->template is_type<rules::val_int>())
                data->range = own_as<ast::val_int>(n.children[1]->data)->value;
            else if (n.children[1]->template is_type<rules::val_float>())
                data->range = own_as<ast::val_float>(n.children[1]->data)->value;

            data->range_unit = parse_unit_object(*n.children[2]);

            // Parse the next set of children as required traits as long as they are identifiers
            for (unsigned i = 3; n.children[i]->template is_type<identifier>(); i++) {
                data->traits.push_back(n.children[i]->string());
            }

            data->body = own_as<ast::always_body>(n.children.back()->data);
            ast::set_parent(data, data->body);
        }
    };
    using for_in_sel = typename selector<for_in, ast::for_in>::on<rules::for_in>;

    struct always_body {
        static void apply(ast_node& n, ast::always_body *data) {
            for (auto& child : n.children) {
                ast::set_parent(data, child->data);

                if (child->template is_type<rules::assignment>()) {
                    data->exprs.push_back(own_as<ast::assignment>(child->data));
                } else if (child->template is_type<rules::continuous_if>()) {
                    data->exprs.push_back(own_as<ast::continuous_if>(child->data));
                } else if (child->template is_type<rules::transition_if>()) {
                    data->exprs.push_back(own_as<ast::transition_if>(child->data));
                } else if (child->template is_type<rules::for_in>()) {
                    data->exprs.push_back(own_as<ast::for_in>(child->data));
                } else {
                    assert("Unhandled child in rule always_body");
                }
            }
        }
    };
    using always_body_sel = typename selector<always_body, ast::always_body>::on<rules::always_body>;

    struct trait_initializer {
        static void apply(ast_node& n, ast::trait_initializer *data) {
            data->name = n.children[0]->string();
            for (unsigned i = 1; i < n.children.size(); i += 2) {
                auto property = n.children[i]->string();
                auto& value_node = n.children[i + 1];

                auto value = ast::literal_value();
                if (value_node->template is_type<rules::val_bool>())
                    value = own_as<ast::val_bool>(value_node->data)->value;
                else if (value_node->template is_type<rules::val_int>())
                    value = own_as<ast::val_int>(value_node->data)->value;
                else if (value_node->template is_type<rules::val_float>())
                    value = own_as<ast::val_float>(value_node->data)->value;

                data->initial_values[property] = value;
            }
        }
    };
    using trait_initializer_sel = typename selector<trait_initializer, ast::trait_initializer>::on<rules::trait_initializer>;

    struct unit_traits {
        static void apply(ast_node& n, ast::unit_traits *data) {
            data->name = n.children[0]->string();
            for (unsigned i = 1; i < n.children.size(); i++) {
                ast::set_parent(data, n.children[i]->data);
                data->traits.push_back(own_as<ast::trait_initializer>(n.children[i]->data));
            }
        }
    };
    using unit_traits_sel = typename selector<unit_traits, ast::unit_traits>::on<rules::unit_traits>;

    struct program : generic_selector<program> {
        template <typename Node>
        static void transform(unique_ptr<Node>& n) {
            auto data = make_unique<ast::program>();
            for (auto& child : n->children) {
                ast::set_parent(data, child->data);

                if (child->template is_type<rules::trait>()) {
                    data->traits.push_back(own_as<ast::trait>(child->data));
                } else if (child->template is_type<rules::unit_traits>()) {
                    data->all_unit_traits.push_back(own_as<ast::unit_traits>(child->data));
                } else {
                    assert("Unhandled child in rule program");
                }
            }

            n->data = std::move(data);
            add_tracking(*n->data);
        }
    };
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
        assignment_sel, continuous_if_sel, transition_if_sel, for_in_sel,
        always_body_sel,
        trait_sel, unit_traits_sel, trait_initializer_sel, program_sel,

        parse_tree::store_content::on<
            identifier,
            rules::member_operator, rules::kw_this, rules::kw_type,
            rules::kw_abs_assignment, rules::kw_rel_assignment, rules::kw_rate_field,
            rules::add_op, rules::sub_op, rules::mul_op, rules::div_op, rules::mod_op, rules::exp_op,
            rules::and_op, rules::or_op,
            rules::eq_op, rules::neq_op, rules::gt_op, rules::lt_op, rules::gte_op, rules::lte_op
        >
    >;
};

parser::parser(pass_manager& pm, std::string input_file) {
    ::pm = &pm;
    latest_line = 1;
    latest_col = 1;

    assert(analyze<rules::program>() == 0);

    std::ifstream file(input_file);
    std::stringstream buffer;
    buffer << file.rdbuf();

    input = make_unique<string_input<>>(buffer.str(), "");
    line_to_file[0] = input_file;

    parse_tree::parse<rules::program, ast_node, selectors::ast_selector>(*input);

    program = std::move(program_ast);
    if (!program) {
        auto const& [filename, line] = selectors::utility::get_pos_in_file(latest_line);
        pm.error<parser>(filename + ":" + std::to_string(line) + ":" + std::to_string(latest_col) + ": Syntax error: parsing failed");
    }
}
