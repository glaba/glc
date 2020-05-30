#include "simplify_transition_ifs.h"
#include "parser.h"
#include "visitor.h"

#include <vector>
#include <string>
#include <memory>
#include <variant>

using std::vector;
using std::unique_ptr;

static auto unique_id_counter = 0;

struct simplify_transition_ifs_visitor {
	ast::program& program;

	simplify_transition_ifs_visitor(ast::program& program) : program(program) {}

	auto simplify_transition_if(ast::transition_if& n) -> vector<ast::expression> {
		using namespace ast;

		auto new_exprs = vector<ast::expression>();

		// Create a new bool variable to contain the previous value of the condition
		auto prev_val = "prev~" + std::to_string(unique_id_counter++);
		auto prev_val_decl = variable_decl::make(variable_type::make(type_enum::BOOL, 0, 0), prev_val);
		find_parent<trait>(n)->props->add_decl(std::move(prev_val_decl));

		// Add an assignment statement prev~# = condition causing prev~# to follow behind by one tick
		auto follower = assignment::make(
			field::make(this_unit(), member_op_enum::CUSTOM, prev_val),
			n.condition->clone());
		new_exprs.emplace_back(std::move(follower));

		// Create a new continuous_if with condition: (condition and (not prev))
		auto new_condition = logical::make(and_op::make(
			std::move(n.condition),
			logical::make(negated::make(
				logical::make(field::make(this_unit(), member_op_enum::CUSTOM, prev_val))))));

		auto new_if = continuous_if::make(std::move(new_condition), std::move(n.body));
		new_exprs.emplace_back(std::move(new_if));

		return new_exprs;
	}

	void operator()(ast::always_body& n) {
		auto new_exprs = vector<ast::expression>();
		for (auto& expr : n.exprs) {
			// Transform only transition ifs and do nothing otherwise
			std::visit(ast::overloaded {
				[&] (unique_ptr<ast::transition_if>& child) {
					for (auto& new_stmt : simplify_transition_if(*child)) {
						std::visit([&] (auto& new_stmt) { ast::set_parent(&n, new_stmt); }, new_stmt);
						new_exprs.emplace_back(std::move(new_stmt));
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

simplify_transition_ifs::simplify_transition_ifs(pass_manager& pm)
	: program(*pm.get_pass<parser>()->program)
{
	auto stiv = simplify_transition_ifs_visitor(program);
	visit<ast::program, decltype(stiv)>()(program, stiv);
}
