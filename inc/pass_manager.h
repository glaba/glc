#pragma once

#include "ast.h"

#include <map>
#include <string>
#include <cxxabi.h>
#include <typeinfo>
#include <functional>
#include <cassert>

class pass {
public:
	virtual ~pass() {}
};

class pass_manager {
public:
	pass_manager(ast::program *program) : program(program) {}

	template <typename Pass>
	Pass *get_pass() {
		std::string id = abi::__cxa_demangle(typeid(Pass).name(), 0, 0, 0);

		if (passes.find(id) == passes.end()) {
			assert(pass_factories.find(id) != pass_factories.end() && "Pass not found");
			passes[id] = pass_factories[id](*this, *program);
		}

		return passes[id].get();
	}

private:
	ast::program *program;
	std::map<std::string, std::unique_ptr<pass>> passes;

	static std::map<std::string, std::function<std::unique_ptr<pass>(pass_manager&, ast::program&)>> pass_factories;

	template <typename Pass>
	friend class register_pass;
};

template <typename Pass>
class register_pass {
	register_pass() {
		std::string id = abi::__cxa_demangle(typeid(Pass).name(), 0, 0, 0);

		pass_manager::pass_factories[id] = [=] (pass_manager& pm, ast::program& program) {
			auto pass_inst = std::make_unique<Pass>(pm, program);
			return std::unique_ptr<pass>(static_cast<pass*>(pass_inst.release()));
		};
	}
};
