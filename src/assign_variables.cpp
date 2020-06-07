#include "assign_variables.h"
#include "parser.h"

#include <vector>
#include <algorithm>
#include <iostream>
#include <cassert>

using std::string;
using std::tuple;
using std::vector;
using std::map;
using std::find_if;

auto const fields = vector<string> {
	"animSpeed",
	"bodyPower",
	"bounceDistMax",
	"bounceDistMin",
	"bouncePower",
	"cargoUse",
	"cost",
	"healthbarOffset",
	"healthbarWidth",
	"height",
	"lifesteal",
	"limit",
	"oscillationAmplitude",
	"percDmg",
	"power",
	"projectileLen",
	"projectileStartHeight",
	"selectionOffsetY",
	"size",
	"startHp",
	"startMana",
	"supply",
	"supplyProvided",
	"tabPriority",
	"vision",
	"visionHeightBonus"
};

auto log2(long x) -> long {
	long retval = 0;
	while (x) {
		retval++;
		x >>= 1;
	}
	return retval - 1;
}

assign_variables::assign_variables(pass_manager& pm) {
	auto& program = *pm.get_pass<parser>()->program;

	assert(program.traits.size() == 1);
	auto& main = *program.get_trait("main");

	// Create list of unassigned fields that we can take from
	auto unassigned = map<string, bitrange>();
	for (auto& field : fields) {
		unassigned[field] = bitrange(0, ast::ty_int::num_bits - 1);
	}

	// Assign fields to each variable
	auto num_assigned = 0;
	for (auto& decl : main.props->variable_declarations) {
		auto& type = decl->type;

		long offset;
		size_t required_bits;

		switch (type->type) {
			case ast::type_enum::BOOL:
				required_bits = 1;
				offset = 0;
				break;
			case ast::type_enum::INT:
				required_bits = log2(type->max - type->min + 1);
				offset = type->min;
				break;
			case ast::type_enum::FLOAT:
				required_bits = ast::ty_int::num_bits;
				offset = 0;
				break;
			default:
				assert(false);
		}

		// Find a field with enough bits available, and preferably one that is unused so far
		auto match = unassigned.end();
		for (auto it = unassigned.begin(); it != unassigned.end(); it++) {
			auto& [_, range] = *it;

			if (range.msb - range.lsb + 1 >= required_bits) {
				match = it;
			}

			// We are completely happy if the field is unused, and continue searching if not
			if (range.lsb == 0) {
				break;
			}
		}

		// Error out if there is no field available
		if (match == unassigned.end()) {
			auto num_variables = main.props->variable_declarations.size();
			pm.error<assign_variables>(program, "Too many variables! Failed to assign " +
				std::to_string(num_variables - num_assigned) + " variables of " + std::to_string(num_variables) +
				" total (some variables are auto-generated)");
			break;
		}

		// Take <required_bits> bits from the bottom of the field
		auto& [field, range] = *match;
		assignments[decl->name] = {field, bitrange(range.lsb, range.lsb + required_bits - 1, offset)};
		range.lsb += required_bits;

		num_assigned++;
	}

	for (auto& pair : assignments) {
		auto& [variable, assign] = pair;
		auto& [field, range] = assign;
		DEBUG(std::cout << "Assigned " << variable << " to " << field << "[" << range.lsb << ":" << range.msb
			<< "] with offset " << range.offset << std::endl);
	}
}

auto assign_variables::get_assignment(string variable) -> assignment {
	assert(assignments.find(variable) != assignments.end());
	return assignments[variable];
}