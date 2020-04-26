#pragma once

#include "ast.h"

#include <map>
#include <string>
#include <typeinfo>
#include <functional>
#include <cassert>
#include <vector>

class pass {
public:
	virtual ~pass() {}
};

class pass_manager {
public:
	pass_manager(ast::program *program) : program(program) {}

	// Returns the pass in case of success, and nullptr in case of an error
	template <typename Pass, typename... Params>
	Pass *get_pass(Params... args) {
		size_t id = typeid(Pass).hash_code();

		if (passes.find(id) == passes.end()) {
			assert(pass_factories.find(id) != pass_factories.end() && "Pass not found");
			passes[id] = pass_factories[id](*this, *program, args...);
		}

		if (errors.find(id) == errors.end()) {
			return static_cast<Pass*>(passes[id].get());
		} else {
			return nullptr;
		}
	}

	template <typename Pass>
	void error(ast::node& n, std::string const& err) {
		errors[typeid(Pass).hash_code()].push_back(
			n.filename() + ":" + std::to_string(n.line()) + ":" + std::to_string(n.col()) + ": " + err);
	}

	template <typename Pass>
	auto get_errors() -> std::vector<std::string>& {
		return errors[typeid(Pass).hash_code()];
	}

private:
	std::map<size_t, std::vector<std::string>> errors;
	ast::program *program;
	std::map<size_t, std::unique_ptr<pass>> passes;

	static std::map<size_t, std::function<std::unique_ptr<pass>(pass_manager&, ast::program&)>> pass_factories;

	template <typename Pass>
	friend class register_pass;
};

template <typename Pass>
class register_pass {
public:
	register_pass() {
		size_t id = typeid(Pass).hash_code();

		pass_manager::pass_factories[id] = [=] (pass_manager& pm, ast::program& program) {
			auto pass_inst = std::make_unique<Pass>(pm, program);
			return std::unique_ptr<pass>(static_cast<pass*>(pass_inst.release()));
		};
	}
};
