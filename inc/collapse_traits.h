#pragma once

#include "ast.h"
#include "pass_manager.h"

#include <string>

// Converts an AST containing multiple traits into an AST containing a single trait
// that applies to all units that leads to the same functionality
class collapse_traits : public pass {
public:
	collapse_traits(pass_manager& pm, ast::program& program);

private:
	// Renames all variables in all traits to be prefixed with the trait name
	void rename_variables();
	// Create new trait
	// Copy all old variables into new trait, and copy all logic into new trait
	// Transform for in with trait loops to for in loops with a trait check inside
	// Copy all logic into new trait

	pass_manager& pm;
	ast::program& program;
};
