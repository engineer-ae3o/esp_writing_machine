#include "parser.hpp"

#include <cstring>
#include <cctype>


namespace gcode::parser {
    
    std::expected<line_t, error_t> parse_line(const char* line) {

        line_t parsed_line{};
        bool is_cmd{};
        
        while ((*line != '\n') && (*line != '\0')) {
            
            // Skip comments that may appear at any point
            if (*line == ';') return parsed_line;
            
            // If the current character is a space or line endings or whatever, skip it
            if ((*line == ' ') || (*line == '\r') || (*line == '\t')) {
                line++;
                continue;
            }

            if (!std::isalpha(*line)) {
                line++;
                continue;
            }

            char letter = std::toupper(*line++);

            char* end{};
            float val = strtof(line, &end);
            if (line == end) return std::unexpected(error_t::MISSING_PARAMETER);

            if (!is_cmd) {
                is_cmd = true;

                switch (letter) {
                    case 'G':
                        switch (static_cast<int>(val)) {
                            case 0: 
                                parsed_line.type = type_t::G0;
                                break;
                            case 1: 
                                parsed_line.type = type_t::G1;
                                break;
                            case 20:
                                parsed_line.type = type_t::G20;
                                return parsed_line;
                            case 21:
                                parsed_line.type = type_t::G21;
                                return parsed_line;
                            case 28:
                                parsed_line.type = type_t::G28;
                                return parsed_line;
                            case 90:
                                parsed_line.type = type_t::G90;
                                return parsed_line;
                            case 91:
                                parsed_line.type = type_t::G91;
                                return parsed_line;
                            default:
                                return std::unexpected(error_t::INVALID_SYNTAX);
                        }
                        break;

                    case 'M':
                        switch (static_cast<int>(val)) {
                            case 2:
                                parsed_line.type = type_t::M2;
                                return parsed_line;
                            case 3:
                                parsed_line.type = type_t::M3;
                                return parsed_line;
                            case 5:
                                parsed_line.type = type_t::M5;
                                return parsed_line;
                        }

                        return parsed_line;
                }
            }
            
            switch (letter) {
                case 'F':
                    parsed_line.params.has_f = true;

                    break;
                
                case 'X':
                    parsed_line.params.has_x = true;

                    break;
                
                case 'Y':
                    parsed_line.params.has_y = true;

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
