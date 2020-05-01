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

	// Creates a new trait with field(s) "trait_bitfield#" indicating which original trait is applicable
	// Copies all the old variables and logic into the new trait, transforming trait
	//  checks in for_in loops to if statements that check trait_bitfield
	// Removes the old traits on completion
	void create_collapsed_trait();

	pass_manager& pm;
	ast::program& program;
};
