#pragma once

#include "ast.h"

#include <string>
#include <memory>

class parser {
public:
	parser(std::string input_file)
		: input_file(input_file) {}

	// Should only be called once at a time
	auto run() -> std::unique_ptr<ast::program>;

private:
	std::string input_file;
};
