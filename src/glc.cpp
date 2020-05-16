#include "cli.h"
#include "parser.h"
#include "pass_manager.h"
#include "semantic_checker.h"
#include "collapse_traits.h"
#include "print_program.h"
#include "merge_ifs.h"

#include <string>
#include <iostream>
#include <memory>
#include <vector>

#define TTY_RESET "\033[0m"
#define TTY_RED "\033[1m\033[31m"
#define TTY_GREEN "\033[1m\033[32m"
#define TTY_CYAN "\033[1m\033[36m"

using std::string;
using std::unique_ptr;
using std::vector;
using none = std::monostate;

int main(int argc, char **argv) {
    // Arguments for command glc ...
    string input_file;
    string output_file;

    cli_parser.add_argument("input_file", "The LWG file to be compiled", &input_file);
    cli_parser.add_option("o", "output_file", "The output JSON map file to be generated", &output_file, string("map.json"));

    // Run CLI parser and exit on failure
    if (!cli_parser.parse("glc", argc, argv)) {
        return 1;
    }

    pass_manager pm;

    try {
        pm.run_pass<parser>(input_file);
        pm.run_pass<semantic_checker>();
        pm.run_pass<print_program>();
        std::cout << TTY_CYAN << "original input" << TTY_RESET << std::endl;
        std::cout << pm.get_pass<print_program>()->get_output() << std::endl;

        pm.run_pass<collapse_traits>();
        pm.run_pass<semantic_checker>();
        pm.run_pass<print_program>();
        std::cout << TTY_CYAN << "collapse_traits" << TTY_RESET << std::endl;
        std::cout << pm.get_pass<print_program>()->get_output() << std::endl;

        pm.run_pass<merge_ifs>();
        pm.run_pass<semantic_checker>();
        pm.run_pass<print_program>();
        std::cout << TTY_CYAN << "merge_ifs" << TTY_RESET << std::endl;
        std::cout << pm.get_pass<print_program>()->get_output() << std::endl;

        std::cout << TTY_GREEN << "Compilation succeeded" << TTY_RESET << std::endl;
    } catch (vector<string>& errors) {
        for (auto& error : errors) {
            std::cout << error << std::endl;
        }
        std::cout << TTY_RED << "Compilation failed due to at least " << errors.size() <<
            (errors.size() == 1 ? " error" : " errors") << TTY_RESET << std::endl;
        return 1;
    }

    return 0;
}
