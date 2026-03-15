#include "unity.h"
#include "parser.hpp"


TEST_CASE("m_cmd_1", "[parser]")
{
    constexpr const char* line = "M3";
    auto ret = gcode::parser::parse_line(line);
}

TEST_CASE("g_cmd_2", "[parser]")
{
    constexpr const char* line = "G0 X11.3 Y43.2";
    auto ret = gcode::parser::parse_line(line);
}

TEST_CASE("g_cmd_3", "[parser]")
{
    constexpr const char* line = "G1 X10.5 Y23.2 F400";
    auto ret = gcode::parser::parse_line(line);
}

TEST_CASE("g_cmd_4", "[parser]")
{
    constexpr const char* line = "G20 X11.3 Y43.2";
    auto ret = gcode::parser::parse_line(line);
}

TEST_CASE("g_cmd_5", "[parser]")
{
    constexpr const char* line = "G1 Z5 F400";
    auto ret = gcode::parser::parse_line(line);
}

TEST_CASE("m_cmd_6", "[parser]")
{
    constexpr const char* line = "M2";
    auto ret = gcode::parser::parse_line(line);
}

TEST_CASE("comment_7", "[parser]")
{
    constexpr const char* line = " ; G1 Z5 F400";
    auto ret = gcode::parser::parse_line(line);
}

TEST_CASE("g_cmd_8", "[parser]")
{
    constexpr const char* line = "G1 Z5 F400 ; ";
    auto ret = gcode::parser::parse_line(line);
}

TEST_CASE("m_cmd_9", "[parser]")
{
    constexpr const char* line = "     M3 ; Should be ignored";
    auto ret = gcode::parser::parse_line(line);
}
