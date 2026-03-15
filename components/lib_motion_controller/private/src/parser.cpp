#include "parser.hpp"

#include <cstring>
#include <cctype>


namespace gcode::parser {
    
    std::expected<line_t, error_t> parse_line(const char* line) {

        line_t parsed_line{};
        bool is_cmd{};

        // Needed to check for `Z0` and `Z5` parameters since they
        // have the same meanings as the `M3` and `M5` commands
        std::optional<uint32_t> z{std::nullopt};
        
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

            char letter = toupper(*line++);

            char* end{};
            float val = strtof(line, &end);
            if (line == end) return std::unexpected(error_t::MISSING_PARAMETER);
            // Advance pointer to next character after the last digit of the extracted float
            line = end;

            if (!is_cmd) {
                // There can only be one command per valid line of Gcode
                is_cmd = true;

                switch (letter) {
                    case 'G':
                        switch (static_cast<uint32_t>(val)) {
                            case 0:
                                parsed_line.type = type_t::G0;
                                // Create the params object
                                parsed_line.params.emplace();
                                break;
                            case 1:
                                parsed_line.type = type_t::G1;
                                // Create the params object
                                parsed_line.params.emplace();
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
                                return std::unexpected(error_t::INVALID_COMMAND);
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
                            default:
                                return std::unexpected(error_t::INVALID_COMMAND);
                        }
                        
                    default:
                        return std::unexpected(error_t::INVALID_COMMAND);
                }
                continue;
            }
            
            switch (letter) {
                case 'F':
                    parsed_line.params->feed_rate = static_cast<uint32_t>(val);
                    break;

                case 'X':
                    parsed_line.params->x = val;
                    break;

                case 'Y':
                    parsed_line.params->y = val;
                    break;

                case 'Z':
                    z = static_cast<uint32_t>(val);
                    break;

                default:
                    return std::unexpected(error_t::INVALID_SYNTAX);
            }
        }

        // Check if there is a Z parameter
        if (z) {
            if (z == 0UL) {
                // If the Z parameter is 0, that means a pen down motion,
                // which is the same as the `M3`commmand
                parsed_line.type = type_t::M3;
            } else if (z == 5UL) {
                // If the Z parameter is 5, that means a pen up motion,
                // which is the same as the `M5`commmand
                parsed_line.type = type_t::M5;
            } else {
                // If there is a Z parameter but it's not a 0 or 5, return an error
                return std::unexpected(error_t::INVALID_SYNTAX);
            }
        }

        // Make sure that `G0` and `G1` both have valid parameters after them
        // The feedrate is optional (lol), but x and y are not. A valid gcode
        // line with `G0` and `G1` has x and y, not only one
        if (!parsed_line.params->x || !parsed_line.params->y) return std::unexpected(error_t::MISSING_PARAMETER);
        
        return parsed_line;
    }

} // namespace gcode
