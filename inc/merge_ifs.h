#pragma once

#include "ast.h"
#include "pass_manager.h"

// Merges if statements such that the body of an if statement never directly contains another if statement
class merge_ifs : public pass {
public:
	merge_ifs(pass_manager& pm);

private:
	ast::program& program;
};