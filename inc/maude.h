#pragma once

#include <string>
#include <functional>
#include <tuple>
#include <optional>

class maude {
public:
	maude(std::string module) : module(module) {}

	// Reduces the provided expression and returns a tuple containing the sort of the result and the result itself
	// Returns an empty option on error
	auto reduce(std::string expr) -> std::optional<std::tuple<std::string, std::string>>;

private:
	static auto run_command(std::string command, std::function<void(std::string)> callback) -> bool;

	std::string module;
};
