#pragma once

#include "ast.h"

#include <map>
#include <string>
#include <typeinfo>
#include <functional>
#include <cassert>
#include <vector>
#include <iostream>

class pass {
public:
	virtual ~pass() {}
};

class pass_manager {
public:
	// Returns the pass in case of success, and nullptr in case of an error
	template <typename Pass, typename... Params>
	Pass *run_pass(Params... args) {
		size_t id = typeid(Pass).hash_code();

		passes[id] = std::unique_ptr<pass>(std::make_unique<Pass>(*this, args...).release());

		if (errors.find(id) == errors.end()) {
			return static_cast<Pass*>(passes[id].get());
		} else {
		    throw get_errors<Pass>();
		}
	}

	template <typename Pass>
	Pass *get_pass() {
		size_t id = typeid(Pass).hash_code();
		assert(passes.find(id) != passes.end());
		return static_cast<Pass*>(passes[id].get());
	}

	template <typename Pass>
	void error(ast::node& n, std::string const& err) {
		errors[typeid(Pass).hash_code()].push_back(
			n.filename() + ":" + std::to_string(n.line()) + ":" + std::to_string(n.col()) + ": " + err);
	}

	template <typename Pass>
	void error(std::string const& err) {
		errors[typeid(Pass).hash_code()].push_back(err);
	}

	template <typename Pass>
	auto get_errors() -> std::vector<std::string>& {
		return errors[typeid(Pass).hash_code()];
	}

private:
	std::map<size_t, std::vector<std::string>> errors;
	std::map<size_t, std::unique_ptr<pass>> passes;
};
