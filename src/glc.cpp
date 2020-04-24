#include "cli.h"
#include "parser.h"
#include "pass_manager.h"

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

    pass_manager pm(program.get());
    
}
