#include "parser.hpp"

#include <cstring>


namespace gcode {

    static error_t parse_token(const char* cmd) {

        // TODO: Implement parsing of tokens here

        return error_t::NONE;
    }

    std::expected<parsed_line_t, error_t> parse_line(const char* line) {

        parsed_line_t parsed_line{};

        // Skip comments
        if (*line == ';') return parsed_line;

        // TODO: Implement parsing of a line here, using parse_token for each token in the line
        while ((*line) && (*line != '\n') && (*line != '\0')) {

            // If the current character is a space, skip it
            if (*line == ' ') {
                line++;
                continue;
            }

            switch *line {
            case 'G':

                break;

            case 'M':

                break;

            default:
                line++;
                continue;
            }
            
            // Skip comments that may appear at any point
            if (*line == ';') return parsed_line;

            line++;
        }
        
        return parsed_line;
    }

} // namespace gcode
