#ifndef PARSER_HPP_
#define PARSER_HPP_


#include <expected>
#include <cstdint>


namespace gcode {

    enum class error_t {
        NONE = 0,
        INVALID_COMMAND,
        INVALID_PARAMETER,
        MISSING_PARAMETER,
        UNKNOWN_ERROR
    };

    struct parsed_line_t {
        bool has_g_cmd{};
        uint32_t g_cmd{};

        bool has_m_cmd{};
        uint32_t m_cmd{};

        float x_val{};
        float y_val{};

        uint32_t feed_rate{};
    };

    /**
     * @brief Parses a line of G-code and returns a parsed_line_t
     *        struct on success, or an error_t on failure.
     * 
     * @param line The line of G-code to parse. It reads parses until a
     *             newline character or null terminator is encountered.
     * 
     * @return `std::expected<parsed_line_t, error_t>` The result of parsing
     *         the line, containing either a parsed_line_t struct or an `error_t`
     */
    std::expected<parsed_line_t, error_t> parse_line(const char* line);

} // namespace gcode


#endif // PARSER_HPP_