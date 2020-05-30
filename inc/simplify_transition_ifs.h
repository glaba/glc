#pragma once

#include "ast.h"
#include "pass_manager.h"

// Convert transition ifs into equivalent statements made of continuous ifs
class simplify_transition_ifs : public pass {
public:
	simplify_transition_ifs(pass_manager& pm);

private:
	ast::program& program;
};