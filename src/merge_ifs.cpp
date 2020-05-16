#include "merge_ifs.h"
#include "parser.h"
#include "visitor.h"

#include <vector>
#include <memory>

using std::vector;
using std::unique_ptr;
using std::make_unique;

struct merge_ifs_visitor {
	pass_manager& pm;
	ast::program& program;

	merge_ifs_visitor(pass_manager& pm, ast::program& program) : pm(pm), program(program) {}

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

merge_ifs::merge_ifs(pass_manager& pm)
	: pm(pm), program(*pm.get_pass<parser>()->program)
{
	auto miv = merge_ifs_visitor(pm, program);
	visit<ast::program, decltype(miv)>()(program, miv);
}
