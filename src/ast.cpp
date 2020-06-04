#include "ast.h"
#include "visitor.h"

#include <iostream>
#include <algorithm>

namespace ast {
	auto ty_int::make(long min, long max) -> unique_ptr<ty_int> {
		auto result = make_unique<ty_int>();
		result->min = min;
		result->max = max;
		return std::move(result);
	}

	auto ty_int::clone() -> unique_ptr<ty_int> {
		return make(min, max);
	}

	auto variable_type::make_value(type_enum type, long min, long max) -> variable_type {
		auto result = variable_type();
		result.type = type;
		result.min = min;
		result.max = max;
		return result;
	}

	auto variable_type::make(type_enum type, long min, long max) -> unique_ptr<variable_type> {
		auto result = make_unique<variable_type>();
		result->type = type;
		result->min = min;
		result->max = max;
		return std::move(result);
	}

	auto variable_type::clone() -> unique_ptr<variable_type> {
		return make(type, min, max);
	}

	auto variable_type::is_arithmetic() -> bool {
		return type == type_enum::INT || type == type_enum::FLOAT;
	}

	auto variable_type::is_logical() -> bool {
		return type == type_enum::BOOL;
	}

	auto variable_decl::make(unique_ptr<variable_type>&& type, string name) -> unique_ptr<variable_decl> {
		auto result = make_unique<variable_decl>();
		result->type = std::move(type);
		result->name = name;
		set_parent(result, result->type);
		return std::move(result);
	}

	auto variable_decl::clone() -> unique_ptr<variable_decl> {
		return make(type->clone(), name);
	}

	auto properties::make(vector<unique_ptr<variable_decl>>&& decls) -> unique_ptr<properties> {
		auto result = make_unique<properties>();
		result->variable_declarations = std::move(decls);
		for (auto& decl : result->variable_declarations) {
			set_parent(result, decl);
		}
		return std::move(result);
	}

	auto properties::clone() -> unique_ptr<properties> {
		auto result = make_unique<properties>();
		for (auto& decl : variable_declarations) {
			result->add_decl(decl->clone());
		}
		return std::move(result);
	}

	void properties::add_decl(unique_ptr<variable_decl>&& decl) {
		set_parent(this, decl);
		variable_declarations.emplace_back(std::move(decl));
	}

	auto field::make(unit_object unit, member_op_enum member_op, string field_name, bool is_rate) -> unique_ptr<field> {
		auto result = make_unique<field>();
		result->unit = unit;
		result->member_op = member_op;
		result->field_name = field_name;
		result->is_rate = is_rate;
		return std::move(result);
	}

	auto field::clone() -> unique_ptr<field> {
		return make(unit, member_op, field_name, is_rate);
	}

	auto field::get_type() -> variable_type* {
		switch (member_op) {
			case member_op_enum::BUILTIN: {
				auto& builtins = get_builtin_fields();
				if (builtins.find(field_name) == builtins.end()) {
					return nullptr;
				} else {
					return &builtins[field_name];
				}
				break;
			}
			case member_op_enum::CUSTOM: {
				auto unit_trait = get_trait();
				if (!unit_trait) {
					return nullptr;
				} else {
					return unit_trait->get_property(field_name)->type.get();
				}
				break;
			}
			case member_op_enum::LANGUAGE: {
				// TODO: check for language properties
				assert(false);
				break;
			}
			default: assert(false);
		}
	}

	auto field::get_loop_from_identifier() -> for_in* {
		auto& identifier = std::get<identifier_unit>(unit).identifier;
		return find_parent<for_in>(*this, [&] (for_in& loop) { return loop.variable == identifier; });
	}

	auto field::get_trait() -> trait* {
		if (!std::holds_alternative<identifier_unit>(unit)) {
			return find_parent<trait>(*this);
		}

		auto& p = *find_parent<program>(*this);
		auto loop = get_loop_from_identifier();
		auto& trait_candidates = loop->traits;

		auto result = static_cast<trait*>(nullptr);
		auto decl_visitor = [&] (variable_decl& decl) {
			if (decl.name == field_name) {
				// Find the trait corresponding to this declaration
				auto trait_node = find_parent<trait>(decl);
				assert(trait_node != nullptr);
				if (std::find(trait_candidates.begin(), trait_candidates.end(), trait_node->name) != trait_candidates.end()) {
					result = trait_node;
				}
			}
		};
		visit<program, decltype(decl_visitor)>()(p, decl_visitor);
		return result;
	}

	auto field::get_builtin_fields() -> map<string, variable_type>& {
		static auto result = map<string, variable_type> {
			{"hp", variable_type::make_value(type_enum::INT, 1, 99999999)},
			{"mana", variable_type::make_value(type_enum::INT, 0, 99999999)},
			{"hpRegenerationRate", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"manaRegenerationRate", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"armor", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"weaponCooldown", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"weaponDelay", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"dmg", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"armorPenetration", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"dmgCap", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"range", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"minRange", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"aoeRadius", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"attackPrio", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"imageScale", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"repairRate", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"repairCost", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"projectileSpeed", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"circleSize", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"circleOffset", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"drawOffsetY", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"acceleration", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"angularVelocity", variable_type::make_value(type_enum::FLOAT, 0, 0)},
			{"goldReward", variable_type::make_value(type_enum::INT, 0, 999999)},
			{"controllable", variable_type::make_value(type_enum::BOOL, 0, 0)},
			{"hasDetection", variable_type::make_value(type_enum::BOOL, 0, 0)},
			{"noShow", variable_type::make_value(type_enum::BOOL, 0, 0)},
			{"isInvisible", variable_type::make_value(type_enum::BOOL, 0, 0)}
		};
		return result;
	}

	auto arithmetic_value::make(variant<unique_ptr<field>, long, double>&& value) -> unique_ptr<arithmetic_value> {
		auto result = make_unique<arithmetic_value>();
		result->value = std::move(value);
		if (std::holds_alternative<unique_ptr<field>>(result->value)) {
			set_parent(result, std::get<unique_ptr<field>>(result->value));
		}
		return std::move(result);
	}

	auto arithmetic_value::clone() -> unique_ptr<arithmetic_value> {
		if (std::holds_alternative<unique_ptr<field>>(value)) {
			return make(std::get<unique_ptr<field>>(value)->clone());
		} else {
			auto result = make_unique<arithmetic_value>();
			std::visit(overloaded {
				[&] (unique_ptr<field>& _) {},
				[&] (auto& v) { result->value = v; }
			}, value);
			return std::move(result);
		}
	}

	auto arithmetic::make(arithmetic::arithmetic_expr&& expr) -> unique_ptr<arithmetic> {
		auto result = make_unique<arithmetic>();
		result->expr = std::move(expr);
		std::visit([&] (auto& node) { set_parent(result, node); }, result->expr);
		return std::move(result);
	}

	auto arithmetic::clone() -> unique_ptr<arithmetic> {
		auto result = make_unique<arithmetic>();
		std::visit([&] (auto& node) {
			auto copy = node->clone();
			set_parent(result, copy);
			result->expr = std::move(copy);
		}, expr);
		return std::move(result);
	}

	auto comparison::make(unique_ptr<arithmetic>&& lhs, unique_ptr<arithmetic>&& rhs,
		comparison_enum comparison_type) -> unique_ptr<comparison>
	{
		auto result = make_unique<comparison>();
		result->lhs = std::move(lhs);
		result->rhs = std::move(rhs);
		result->comparison_type = comparison_type;
		set_parent(result, result->lhs, result->rhs);
		return std::move(result);
	}

	auto comparison::clone() -> unique_ptr<comparison> {
		return make(lhs->clone(), rhs->clone(), comparison_type);
	}

	auto negated::make(unique_ptr<logical>&& expr) -> unique_ptr<negated> {
		auto result = make_unique<negated>();
		result->expr = std::move(expr);
		set_parent(result, result->expr);
		return std::move(result);
	}

	auto negated::clone() -> unique_ptr<negated> {
		return make(expr->clone());
	}

	auto logical::make(logical::logical_expr&& expr) -> unique_ptr<logical> {
		auto result = make_unique<logical>();
		result->expr = std::move(expr);
		std::visit([&] (auto& node) { set_parent(result, node); }, result->expr);
		return std::move(result);
	}

	auto logical::clone() -> unique_ptr<logical> {
		auto result = make_unique<logical>();
		std::visit([&] (auto& node) {
			auto copy = node->clone();
			set_parent(result, copy);
			result->expr = std::move(copy);
		}, expr);
		return std::move(result);
	}

	auto assignment::make(unique_ptr<field>&& lhs, assignment_enum assignment_type, rhs_t&& rhs) -> unique_ptr<assignment> {
		auto result = make_unique<assignment>();
		result->lhs = std::move(lhs);
		result->assignment_type = assignment_type;
		result->rhs = std::move(rhs);
		set_parent(result, result->lhs);
		std::visit([&] (auto& node) { set_parent(result, node); }, result->rhs);
		return std::move(result);
	}

	auto assignment::clone() -> unique_ptr<assignment> {
		auto result = unique_ptr<assignment>();
		std::visit([&] (auto& node) { result = make(lhs->clone(), assignment_type, node->clone()); }, rhs);
		return std::move(result);
	}

	auto continuous_if::make(unique_ptr<logical>&& condition, unique_ptr<always_body>&& body) -> unique_ptr<continuous_if> {
		auto result = make_unique<continuous_if>();
		result->condition = std::move(condition);
		result->body = std::move(body);
		set_parent(result, result->condition, result->body);
		return std::move(result);
	}

	auto continuous_if::clone() -> unique_ptr<continuous_if> {
		return make(condition->clone(), body->clone());
	}

	auto transition_if::make(unique_ptr<logical>&& condition, unique_ptr<always_body>&& body) -> unique_ptr<transition_if> {
		auto result = make_unique<transition_if>();
		result->condition = std::move(condition);
		result->body = std::move(body);
		set_parent(result, result->condition, result->body);
		return std::move(result);
	}

	auto transition_if::clone() -> unique_ptr<transition_if> {
		return make(condition->clone(), body->clone());
	}

	auto for_in::make(string variable, double range, unit_object range_unit, vector<string>& traits, unique_ptr<always_body>&& body) -> unique_ptr<for_in> {
		auto result = make_unique<for_in>();
		result->variable = variable;
		result->range = range;
		result->range_unit = range_unit;
		result->traits = traits;
		result->body = std::move(body);
		set_parent(result, result->body);
		return std::move(result);
	}

	auto for_in::clone() -> unique_ptr<for_in> {
		return make(variable, range, range_unit, traits, body->clone());
	}

	void for_in::replace_body(unique_ptr<always_body>&& new_body) {
		set_parent(this, new_body);
		body = std::move(new_body);
	}

	auto for_in::get_loop_from_identifier() -> for_in* {
		auto identifier = std::get<identifier_unit>(range_unit).identifier;
		return find_parent<for_in>(*this->parent(), [&] (for_in& loop) { return loop.variable == identifier; });
	}

	auto always_body::make(vector<expression>&& exprs) -> unique_ptr<always_body> {
		auto result = make_unique<always_body>();
		result->exprs = std::move(exprs);
		for (auto& expr : result->exprs) {
			std::visit([&] (auto& node) { set_parent(result, node); }, expr);
		}
		return std::move(result);
	}

	auto always_body::clone() -> unique_ptr<always_body> {
		auto result = make_unique<always_body>();
		for (auto& expr : exprs) {
			std::visit([&] (auto& node) {
				auto copy = node->clone();
				set_parent(result, copy);
				result->exprs.push_back(std::move(copy));
			}, expr);
		}
		return std::move(result);
	}

	void always_body::insert_expr(expression&& expr) {
		std::visit([&] (auto& node) { set_parent(this, node); }, expr);
		exprs.emplace_back(std::move(expr));
	}

	auto trait::make(string name, unique_ptr<properties>&& props, unique_ptr<always_body>&& body) -> unique_ptr<trait> {
		auto result = make_unique<trait>();
		result->name = name;
		result->props = std::move(props);
		result->body = std::move(body);
		set_parent(result, result->props, result->body);
		return std::move(result);
	}

	auto trait::clone() -> unique_ptr<trait> {
		return make(name, props->clone(), body->clone());
	}

	auto trait::get_property(string name) -> variable_decl* {
		for (auto& prop : props->variable_declarations) {
			if (prop->name == name) {
				return prop.get();
			}
		}
		return nullptr;
	}

	auto trait_initializer::make(string name, map<string, literal_value> initial_values) -> unique_ptr<trait_initializer> {
		auto result = make_unique<trait_initializer>();
		result->name = name;
		result->initial_values = initial_values;
		return std::move(result);
	}

	auto trait_initializer::clone() -> unique_ptr<trait_initializer> {
		return make(name, initial_values);
	}

	auto unit_traits::make(string name, vector<unique_ptr<trait_initializer>>&& traits) -> unique_ptr<unit_traits> {
		auto result = make_unique<unit_traits>();
		result->name = name;
		result->traits = std::move(traits);
		for (auto& trait : result->traits) {
			set_parent(result, trait);
		}
		return std::move(result);
	}

	auto unit_traits::clone() -> unique_ptr<unit_traits> {
		auto result = make_unique<unit_traits>();
		result->name = name;
		for (auto& trait : traits) {
			result->traits.push_back(trait->clone());
		}
		return std::move(result);
	}

	void unit_traits::insert_initializer(unique_ptr<trait_initializer>&& trait) {
		set_parent(this, trait);
		traits.emplace_back(std::move(trait));
	}

	auto program::make(vector<unique_ptr<trait>>&& traits, vector<unique_ptr<unit_traits>>&& all_unit_traits) -> unique_ptr<program> {
		auto result = make_unique<program>();
		result->traits = std::move(traits);
		result->all_unit_traits = std::move(all_unit_traits);
		for (auto& trait : result->traits) {
			set_parent(result, trait);
		}
		for (auto& single_unit_traits : result->all_unit_traits) {
			set_parent(result, single_unit_traits);
		}
		return std::move(result);
	}

	auto program::clone() -> unique_ptr<program> {
		auto result = make_unique<program>();
		for (auto& trait : traits) {
			auto copy = trait->clone();
			set_parent(result, copy);
			result->traits.push_back(std::move(copy));
		}
		for (auto& single_unit_traits : all_unit_traits) {
			auto copy = single_unit_traits->clone();
			set_parent(result, copy);
			result->all_unit_traits.push_back(std::move(copy));
		}
		return std::move(result);
	}

	void program::insert_trait(unique_ptr<trait>&& trait) {
		set_parent(this, trait);
		traits.emplace_back(std::move(trait));
	}

	auto program::get_trait(string name) -> trait* {
		for (auto& trait : traits) {
			if (trait->name == name) {
				return trait.get();
			}
		}
		return nullptr;
	}
}