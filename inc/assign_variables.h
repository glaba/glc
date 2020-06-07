#pragma once

#include "ast.h"
#include "pass_manager.h"

#include <string>
#include <tuple>
#include <map>

// Assigns variables of the main trait to unit fields that the generated modifiers will reference
class assign_variables : public pass {
public:
	assign_variables(pass_manager& pm);

	// Inclusive range of bits used, and the offset to the actual value represented
	// For example, if the bits are 101, and the offset is -2, the actual value represented is 3, not 5
	struct bitrange {
		bitrange() {}
		bitrange(size_t lsb, size_t msb, long offset = 0) : lsb(lsb), msb(msb), offset(offset) {}
		size_t lsb;
		size_t msb;
		long offset;
	};
	using assignment = std::tuple<std::string, bitrange>;

	auto get_assignment(std::string variable) -> assignment;

private:
	std::map<std::string, assignment> assignments;
};
