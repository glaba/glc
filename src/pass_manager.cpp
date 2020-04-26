#include "pass_manager.h"

std::map<size_t, std::function<std::unique_ptr<pass>(pass_manager&, ast::program&)>> pass_manager::pass_factories;
