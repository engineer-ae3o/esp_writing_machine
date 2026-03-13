#include "parser.hpp"

#include <cstring>


namespace gcode {
    
    std::expected<parsed_line_t, error_t> parse_line(const char* line) {

        parsed_line_t parsed_line{};

        // Skip comments
        if (*line == ';') return parsed_line;

        // TODO: Implement parsing of a line here, using for each token in the line
        while ((*line != '\n') && (*line != '\0')) {
            
            // If the current character is a space, skip it
            if (*line == ' ') {
                line++;
                continue;
            }
            
            // Skip comments that may appear at any point
            if (*line == ';') return parsed_line;

            switch (*line) {
            case 'G':
                parsed_line.has_g = true;
                
                break;

            case 'M':

                break;
            
            case 'X':

                break;
            
            case 'Y':

                break;

            case 'Z':

                break;

            default:
                line++;
                continue;
            }
            
            line++;
        }
        
        return parsed_line;
    }

} // namespace gcode
