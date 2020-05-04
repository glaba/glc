#pragma once

#include "ast.h"
#include "pass_manager.h"

class semantic_checker : public pass {
public:
	semantic_checker(pass_manager& pm);

private:
	pass_manager& pm;
	ast::program& program;
};