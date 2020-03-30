#pragma once

#include <string>

class parser {
public:
	parser(std::string input_file)
		: input_file(input_file) {}

	void run();

private:
	std::string input_file;
};
