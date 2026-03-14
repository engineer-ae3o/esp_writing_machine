#ifndef PARSER_HPP_
#define PARSER_HPP_


#include <expected>
#include <optional>
#include <cstdint>


namespace gcode::parser {

    enum class error_t {
        NONE = 0,
        INVALID_COMMAND,
        INVALID_SYNTAX,
        MISSING_PARAMETER,
    };

    enum class type_t : uint8_t {
        G0  = 0,
        G1  = 1,
        G20 = 20,
        G21 = 21,
        G28 = 28,
        G90 = 90,
        G91 = 91,
        M2  = 2,
        M3  = 3,
        M5  = 5
    };

    struct param_t {
        std::optional<float> x{std::nullopt};
        std::optional<float> y{std::nullopt};
        std::optional<uint32_t> feed_rate{std::nullopt};
    };

    struct line_t {
        // Gcode command
        std::optional<type_t> type{std::nullopt};
        // These are only valid if the type is either `G0` or `G1`
        std::optional<param_t> params{std::nullopt};
    };

    /**
     * @brief Parses a line of G-code and returns a line_t
     *        struct on success, or an error_t on failure.
     * 
     * @param line The line of G-code to parse. It reads parses until a
     *             newline character or null terminator is encountered.
     * 
     * @return `std::expected<line_t, error_t>` The result of parsing
     *         the line, containing either a parsed line or an error code
     */
    std::expected<line_t, error_t> parse_line(const char* line);

} // namespace gcode


#endif // PARSER_HPP_