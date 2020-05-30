#include "maude.h"

#include <cstdlib>

using std::string;
using std::function;
using std::tuple;
using std::make_tuple;
using std::optional;

auto maude::reduce(string expr) -> optional<tuple<string, string>> {
	bool got_result = false;
	string sort;
	string result;

	run_command("echo \"red " + expr + " .\" | ./maude 2>&1 " + module,
		[&] (string line) {
			// Parse a string with format result <sort>: <expression>
			if (line.substr(0, 6) != "result") {
				return;
			}

			auto colon_pos = line.find_first_of(':');
			if (colon_pos == string::npos) {
				return;
			}

			got_result = true;
			sort = line.substr(7, colon_pos);
			result = line.substr(colon_pos + 2);
		});

	if (got_result) {
		return make_tuple(sort, result);
	} else {
		return std::nullopt;
	}
}

auto maude::run_command(string command, function<void(string)> callback) -> bool {
    char buffer[1024];
    FILE *stream = popen(command.c_str(), "r");

    if (!stream) {
    	return false;
    }

    string cur_line = "";
    while (!feof(stream)) {
        size_t bytes_read = fread(static_cast<void*>(buffer), 1, 1024, stream);
        if (bytes_read != 1024 && (!feof(stream) || bytes_read == 0)) {
        	return false;
        }

        cur_line += string(buffer, bytes_read);

        while (true) {
            int newline_pos = -1;
            for (size_t i = 0; i < cur_line.length(); i++) {
                if (cur_line.at(i) == '\n') {
                    newline_pos = i;
                    break;
                }
            }

            // Check if we encountered a \n, which means we should call the callback
            if (newline_pos < 0) {
            	break;
            }

            callback(cur_line.substr(0, newline_pos));
            cur_line = cur_line.substr(newline_pos + 1);
        }
    }
    return pclose(stream) == 0;
}