#include "merge_ifs.h"
#include "parser.h"
#include "visitor.h"
#include "print_program.h"
#include "maude.h"

#include <vector>
#include <memory>
#include <string>
#include <cassert>
#include <map>
#include <set>

using std::vector;
using std::unique_ptr;
using std::make_unique;
using std::string;
using std::map;
using std::set;

// Custom printer that will cause logical / arithmetic expressions to print according to the syntax in lwg.maude
struct maude_printer {
    auto operator()(print_program& pp, ast::arithmetic_value& v) -> string {
        auto output = string();
        std::visit(ast::overloaded {
            [&] (unique_ptr<ast::field>& val) { output = pp.get_output_for_node<ast::field, maude_printer>(*val); },
            [&] (long val) { output = std::to_string(val); },
            [&] (double val) { output = std::to_string(val); }
        }, v.value);
        return output + ":Arithmetic";
    }

    auto operator()(print_program& pp, ast::logical& v) -> string {
    	auto output = std::string("(");
		std::visit([&] (auto& val) {
			output += pp.get_output_for_node<decltype(*val), maude_printer>(*val);
		}, v.expr);
		if (std::holds_alternative<unique_ptr<ast::field>>(v.expr) || std::holds_alternative<unique_ptr<ast::val_bool>>(v.expr)) {
			output += ":Logical";
		}
		output += ")";
		return output;
    }

    auto operator()(print_program& pp, ast::comparison& v) -> string {
    	auto lhs = pp.get_output_for_node<ast::arithmetic, maude_printer>(*v.lhs);
    	auto rhs = pp.get_output_for_node<ast::arithmetic, maude_printer>(*v.rhs);
    	switch (v.comparison_type) {
    		case ast::comparison_enum::EQ: return lhs + " eqs " + rhs;
    		case ast::comparison_enum::NEQ: return lhs + " neq " + rhs;
    		case ast::comparison_enum::GT: return lhs + " gt " + rhs;
    		case ast::comparison_enum::LT: return lhs + " lt " + rhs;
    		case ast::comparison_enum::GTE: return lhs + " gte " + rhs;
    		case ast::comparison_enum::LTE: return lhs + " lte " + rhs;
    		default: assert(false);
    	}
    }
};

struct merge_nested_ifs_visitor {
	ast::program& program;

	merge_nested_ifs_visitor(ast::program& program) : program(program) {}

	// Merges an if statement with any if statements contained within. As an example
	//  if (condition 1) {
	//      ...
	//      if (condition 2) {}
	//      ...
	//  }
	//
	// becomes
	//
	//  if (condition 1) { ... }
	//  if (condition 1 && condition 2) { ... }
	//  if (condition 1) { ... }
	//
	// where common or redundantly generated if statements will be merged or removed by another pass
	auto merge_if(ast::continuous_if& n) -> vector<unique_ptr<ast::continuous_if>> {
		using namespace ast;

		auto new_ifs = vector<unique_ptr<continuous_if>>();

		// If statement with the same condition as the original if statement that will
		//  contain all the non-if statements from the original if body that do not need merging
		auto main_if = continuous_if::make(n.condition->clone(), make_unique<always_body>());

		for (auto& expr : n.body->exprs) {
			std::visit(overloaded {
				[&] (unique_ptr<continuous_if>& child) {
					// Create a new if statement merging with this child
					auto merged = continuous_if::make(
						logical::make(and_op::make(n.condition->clone(), child->condition->clone())),
						child->body->clone()
					);
					new_ifs.push_back(std::move(merged));
				},
				[&] (auto& child) {
					// Append the line to the main if statement body
					main_if->body->insert_expr(child->clone());
				}
			}, expr);
		}

		new_ifs.emplace_back(std::move(main_if));
		return new_ifs;
	}

	void operator()(ast::always_body& n) {
		auto new_exprs = vector<ast::expression>();
		for (auto& expr : n.exprs) {
			// Merge if statements, and leave all other statements alone
			std::visit(ast::overloaded {
				[&] (unique_ptr<ast::continuous_if>& child) {
					for (auto& if_stmt : merge_if(*child)) {
						ast::set_parent(&n, if_stmt);
						new_exprs.emplace_back(std::move(if_stmt));
					}
				},
				[&] (auto& child) {
					new_exprs.emplace_back(std::move(child));
				}
			}, expr);
		}

		n.exprs = std::move(new_exprs);
	}
};

struct remove_empty_ifs_visitor {
	ast::program& program;

	remove_empty_ifs_visitor(ast::program& program) : program(program) {}

	void operator()(ast::always_body& n) {
		for (auto it = n.exprs.begin(); it != n.exprs.end();) {
			if (std::holds_alternative<unique_ptr<ast::continuous_if>>(*it)) {
				auto& if_stmt = std::get<unique_ptr<ast::continuous_if>>(*it);

				if (if_stmt->body->exprs.size() == 0) {
					it = n.exprs.erase(it);
					continue;
				}
			}

			it++;
		}
	}
};

struct merge_common_ifs_visitor {
	// Merges if statements that have the same condition in the same always_body

	ast::program& program;
	bool changed;

	merge_common_ifs_visitor(ast::program& program) : program(program), changed(false) {}

	void operator()(ast::always_body& n) {
		auto pp = print_program(program);
		auto maude_inst = maude("lwg.maude");

		// Collect all if statements in a separate vector
		auto if_stmts = vector<unique_ptr<ast::continuous_if>>();
		for (auto it = n.exprs.begin(); it != n.exprs.end();) {
			if (std::holds_alternative<unique_ptr<ast::continuous_if>>(*it)) {
				if_stmts.emplace_back(
					std::move(std::get<unique_ptr<ast::continuous_if>>(*it)));
				it = n.exprs.erase(it);
			} else {
				it++;
			}
		}

		// Create disjoint sets of if statements that have equivalent conditions that can be merged
		// Map from if_stmt index to set index
		auto set_map = map<size_t, size_t>();
		// Map from set index to if stmt indices
		auto stmt_map = map<size_t, set<size_t>>();
		auto cur_set_index = 0;
		for (auto i = 0U; i < if_stmts.size(); i++) {
			for (auto j = i + 1; j < if_stmts.size(); j++) {
				// Check if the two are already transitively equivalent and skip if so
				if (set_map.find(i) != set_map.end() && set_map.find(j) != set_map.end() && set_map[i] == set_map[j]) {
					continue;
				}

				// Check if the two conditions are equivalent
				auto cond_1 = pp.get_output_for_node<ast::logical, maude_printer>(*if_stmts[i]->condition);
				auto cond_2 = pp.get_output_for_node<ast::logical, maude_printer>(*if_stmts[j]->condition);
				auto result = maude_inst.reduce(cond_1 + " == " + cond_2);

				if (!result) {
					std::cerr << "Unexpected internal failure of Maude on expression: " + cond_1 + " == " + cond_2 << std::endl;
				}
				if (!result || std::get<1>(result.value()) != "true") {
					continue;
				}

				// Create a new set if neither of them is in one already
				if (set_map.find(i) == set_map.end() && set_map.find(j) == set_map.end()) {
					set_map[i] = cur_set_index;
					set_map[j] = cur_set_index;
					stmt_map[cur_set_index].insert(i);
					stmt_map[cur_set_index].insert(j);
					cur_set_index++;
					continue;
				}

				// Find the index of the set that one is already in
				auto set_index = set_map.find(i) == set_map.end() ? set_map[j] : set_map[i];
				// Insert both into the set (one redundantly)
				set_map[i] = set_index;
				set_map[j] = set_index;
				stmt_map[set_index].insert(i);
				stmt_map[set_index].insert(j);
			}
		}

		auto merged_if_stmts = vector<unique_ptr<ast::continuous_if>>();
		for (auto& it : stmt_map) {
			auto& if_stmt_indices = it.second;

			// Copy the condition of the first if statement of the list into the new if statement
			auto& first_if = if_stmts[*if_stmt_indices.begin()];
			auto new_if_stmt = ast::continuous_if::make(std::move(first_if->condition), make_unique<ast::always_body>());

			// Move each if statement out of the vector if_stmts and append the body into the new if statement
			for (auto i : if_stmt_indices) {
				auto cur_if = std::move(if_stmts[i]);

				for (auto& expr : cur_if->body->exprs) {
					new_if_stmt->body->insert_expr(std::move(expr));
				}
			}

			merged_if_stmts.emplace_back(std::move(new_if_stmt));
		}

		if (merged_if_stmts.size() > 0) {
			changed = true;
		}

		// Insert the new if statements back into the original body
		// First, the if statements that were not merged
		for (auto& if_stmt : if_stmts) {
			// May be nullptr since we moved if statements out while merging
			if (if_stmt) {
				n.insert_expr(std::move(if_stmt));
			}
		}
		for (auto& if_stmt : merged_if_stmts) {
			n.insert_expr(std::move(if_stmt));
		}
	}
};

merge_ifs::merge_ifs(pass_manager& pm)
	: program(*pm.get_pass<parser>()->program)
{
	auto mniv = merge_nested_ifs_visitor(program);
	visit<ast::program, decltype(mniv)>()(program, mniv);

	auto reiv = remove_empty_ifs_visitor(program);
	visit<ast::program, decltype(reiv)>()(program, reiv);

	auto mciv = merge_common_ifs_visitor(program);
	do {
		mciv.changed = false;
		visit<ast::program, decltype(mciv)>()(program, mciv);
	} while (mciv.changed);
}
