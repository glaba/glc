#include "cli.h"
#include "parser.h"
#include "pass_manager.h"
#include "collapse_traits.h"

#include <string>
#include <iostream>
#include <memory>

using std::string;
using std::unique_ptr;
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

    parser p(input_file);
    unique_ptr<ast::program> program = p.run();

    if (!program) {
        std::cout << "Failed to parse program" << std::endl;
        return 1;
    }

    pass_manager pm(program.get());
    pm.get_pass<collapse_traits>();
    for (auto& error : pm.get_errors<collapse_traits>()) {
        std::cout << error << std::endl;
    }
}
