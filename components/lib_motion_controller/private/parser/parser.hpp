#ifndef PARSER_HPP_
#define PARSER_HPP_


#include <expected>
#include <cstdint>


namespace gcode::parser {

    enum class error_t {
        NONE = 0,
        INVALID_COMMAND,
        INVALID_SYNTAX,
        MISSING_PARAMETER,
        UNKNOWN_ERROR
    };

    enum class type_t : int8_t {
        UNKNOWN = -1,
        G0      = 0,
        G1      = 0,
        G20     = 20,
        G21     = 21,
        G28     = 28,
        G90     = 90,
        G91     = 91,
        M2      = 2,
        M3      = 3,
        M5      = 5
    };

    struct param_t {
        float x{}, y{}, f{};
        bool has_x{}, has_y{}, has_f{};
    };

    struct line_t {
        // Gcode command
        type_t type{};
        // These are only valid if the type is either `G0` or `G1`
        param_t params{};
    };

    /**
     * @brief Parses a line of G-code and returns a line_t
     *        struct on success, or an error_t on failure.
     * 
     * @param line The line of G-code to parse. It reads parses until a
     *             newline character or null terminator is encountered.
     * 
     * @return `std::expected<line_t, error_t>` The result of parsing
     *         the line, containing either a line_t struct or an `error_t`
     */
    std::expected<line_t, error_t> parse_line(const char* line);

} // namespace gcode


#endif // PARSER_HPP_