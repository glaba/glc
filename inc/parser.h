#pragma once

#include "ast.h"
#include "pass_manager.h"

#include <string>
#include <memory>

class parser : public pass {
public:
	parser(pass_manager& pm, std::string input_file);

	std::unique_ptr<ast::program> program;
};
