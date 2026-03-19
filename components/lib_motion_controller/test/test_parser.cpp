#include "unity.h"
#include "parser.hpp"


// Shorthand aliases
using type_t  = gcode::parser::type_t;
using error_t = gcode::parser::error_t;

static constexpr float FLOAT_DELTA = 0.001f;

void setUp(void)    {}
void tearDown(void) {}


// VALID M COMMANDS

TEST_CASE("m_cmd_M2", "[parser][m_cmd]")
{
    auto ret = gcode::parser::parse_line("M2");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::M2), static_cast<uint8_t>(ret->type.value()));
    TEST_ASSERT_FALSE(ret->params.has_value());
}

TEST_CASE("m_cmd_M3", "[parser][m_cmd]")
{
    auto ret = gcode::parser::parse_line("M3");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::M3), static_cast<uint8_t>(ret->type.value()));
    TEST_ASSERT_FALSE(ret->params.has_value());
}

TEST_CASE("m_cmd_M5", "[parser][m_cmd]")
{
    auto ret = gcode::parser::parse_line("M5");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::M5), static_cast<uint8_t>(ret->type.value()));
    TEST_ASSERT_FALSE(ret->params.has_value());
}

TEST_CASE("m_cmd_M3_lowercase", "[parser][m_cmd]")
{
    auto ret = gcode::parser::parse_line("m3");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::M3), static_cast<uint8_t>(ret->type.value()));
}

TEST_CASE("m_cmd_M3_leading_whitespace", "[parser][m_cmd]")
{
    auto ret = gcode::parser::parse_line("     M3");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::M3), static_cast<uint8_t>(ret->type.value()));
}

TEST_CASE("m_cmd_M3_inline_comment", "[parser][m_cmd]")
{
    auto ret = gcode::parser::parse_line("M3 ; pen down");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::M3), static_cast<uint8_t>(ret->type.value()));
}

TEST_CASE("m_cmd_M3_trailing_whitespace", "[parser][m_cmd]")
{
    auto ret = gcode::parser::parse_line("M3   ");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::M3), static_cast<uint8_t>(ret->type.value()));
}


// VALID G COMMANDS (no param variants)

TEST_CASE("g_cmd_G20", "[parser][g_cmd]")
{
    auto ret = gcode::parser::parse_line("G20");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::G20), static_cast<uint8_t>(ret->type.value()));
    TEST_ASSERT_FALSE(ret->params.has_value());
}

TEST_CASE("g_cmd_G21", "[parser][g_cmd]")
{
    auto ret = gcode::parser::parse_line("G21");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::G21), static_cast<uint8_t>(ret->type.value()));
    TEST_ASSERT_FALSE(ret->params.has_value());
}

TEST_CASE("g_cmd_G28", "[parser][g_cmd]")
{
    auto ret = gcode::parser::parse_line("G28");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::G28), static_cast<uint8_t>(ret->type.value()));
    TEST_ASSERT_FALSE(ret->params.has_value());
}

TEST_CASE("g_cmd_G90", "[parser][g_cmd]")
{
    auto ret = gcode::parser::parse_line("G90");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::G90), static_cast<uint8_t>(ret->type.value()));
    TEST_ASSERT_FALSE(ret->params.has_value());
}

TEST_CASE("g_cmd_G91", "[parser][g_cmd]")
{
    auto ret = gcode::parser::parse_line("G91");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::G91), static_cast<uint8_t>(ret->type.value()));
    TEST_ASSERT_FALSE(ret->params.has_value());
}

// No param commands should silently drop any trailing params
TEST_CASE("g_cmd_G20_with_trailing_params_dropped", "[parser][g_cmd]")
{
    auto ret = gcode::parser::parse_line("G20 X11.3 Y43.2");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::G20), static_cast<uint8_t>(ret->type.value()));
    TEST_ASSERT_FALSE(ret->params.has_value());
}


// VALID G0 / G1

TEST_CASE("g_cmd_G0_XY", "[parser][g_cmd]")
{
    auto ret = gcode::parser::parse_line("G0 X11.3 Y43.2");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::G0), static_cast<uint8_t>(ret->type.value()));
    TEST_ASSERT(ret->params.has_value());
    TEST_ASSERT(ret->params->x.has_value());
    TEST_ASSERT(ret->params->y.has_value());
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_DELTA, 11.3f, ret->params->x.value());
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_DELTA, 43.2f, ret->params->y.value());
}

TEST_CASE("g_cmd_G0_X_only", "[parser][g_cmd]")
{
    auto ret = gcode::parser::parse_line("G0 X5.0");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::G0), static_cast<uint8_t>(ret->type.value()));
    TEST_ASSERT(ret->params->x.has_value());
    TEST_ASSERT_FALSE(ret->params->y.has_value());
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_DELTA, 5.0f, ret->params->x.value());
}

TEST_CASE("g_cmd_G0_Y_only", "[parser][g_cmd]")
{
    auto ret = gcode::parser::parse_line("G0 Y99.9");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::G0), static_cast<uint8_t>(ret->type.value()));
    TEST_ASSERT_FALSE(ret->params->x.has_value());
    TEST_ASSERT(ret->params->y.has_value());
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_DELTA, 99.9f, ret->params->y.value());
}

TEST_CASE("g_cmd_G0_no_params", "[parser][g_cmd]")
{
    // G0 with no coordinates is valid: all params remain nullopt
    auto ret = gcode::parser::parse_line("G0");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::G0), static_cast<uint8_t>(ret->type.value()));
    TEST_ASSERT(ret->params.has_value());
    TEST_ASSERT_FALSE(ret->params->x.has_value());
    TEST_ASSERT_FALSE(ret->params->y.has_value());
    TEST_ASSERT_FALSE(ret->params->feed_rate.has_value());
}

TEST_CASE("g_cmd_G1_XYF", "[parser][g_cmd]")
{
    auto ret = gcode::parser::parse_line("G1 X10.5 Y23.2 F400");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::G1), static_cast<uint8_t>(ret->type.value()));
    TEST_ASSERT(ret->params.has_value());
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_DELTA, 10.5f, ret->params->x.value());
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_DELTA, 23.2f, ret->params->y.value());
    TEST_ASSERT_EQUAL_UINT32(400, ret->params->feed_rate.value());
}

TEST_CASE("g_cmd_G1_XY_no_feed", "[parser][g_cmd]")
{
    auto ret = gcode::parser::parse_line("G1 X1.0 Y2.0");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::G1), static_cast<uint8_t>(ret->type.value()));
    TEST_ASSERT_FALSE(ret->params->feed_rate.has_value());
}

TEST_CASE("g_cmd_G0_negative_coords", "[parser][g_cmd]")
{
    auto ret = gcode::parser::parse_line("G0 X-10.5 Y-3.2");
    TEST_ASSERT(ret);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_DELTA, -10.5f, ret->params->x.value());
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_DELTA, -3.2f,  ret->params->y.value());
}

TEST_CASE("g_cmd_G0_zero_coords", "[parser][g_cmd]")
{
    auto ret = gcode::parser::parse_line("G0 X0.0 Y0.0");
    TEST_ASSERT(ret);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_DELTA, 0.0f, ret->params->x.value());
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_DELTA, 0.0f, ret->params->y.value());
}

TEST_CASE("g_cmd_G0_lowercase", "[parser][g_cmd]")
{
    auto ret = gcode::parser::parse_line("g0 x5.0 y10.0");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::G0), static_cast<uint8_t>(ret->type.value()));
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_DELTA, 5.0f,  ret->params->x.value());
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_DELTA, 10.0f, ret->params->y.value());
}

TEST_CASE("g_cmd_G1_params_reordered", "[parser][g_cmd]")
{
    // F before X and Y: should work
    auto ret = gcode::parser::parse_line("G1 F300 Y5.0 X2.5");
    TEST_ASSERT(ret);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_DELTA, 2.5f, ret->params->x.value());
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_DELTA, 5.0f, ret->params->y.value());
    TEST_ASSERT_EQUAL_UINT32(300, ret->params->feed_rate.value());
}

TEST_CASE("g_cmd_G1_inline_comment", "[parser][g_cmd]")
{
    auto ret = gcode::parser::parse_line("G1 X5.0 Y10.0 ; move to position");
    TEST_ASSERT(ret);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_DELTA, 5.0f,  ret->params->x.value());
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_DELTA, 10.0f, ret->params->y.value());
}

TEST_CASE("g_cmd_G1_tab_separated", "[parser][g_cmd]")
{
    auto ret = gcode::parser::parse_line("G1\tX5.0\tY10.0");
    TEST_ASSERT(ret);
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_DELTA, 5.0f,  ret->params->x.value());
    TEST_ASSERT_FLOAT_WITHIN(FLOAT_DELTA, 10.0f, ret->params->y.value());
}


// Z PARAMETER (treated as M3/M5)

TEST_CASE("z_param_Z0_alone", "[parser][z_param]")
{
    auto ret = gcode::parser::parse_line("Z0");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::M3), static_cast<uint8_t>(ret->type.value()));
}

TEST_CASE("z_param_Z5_alone", "[parser][z_param]")
{
    auto ret = gcode::parser::parse_line("Z5");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::M5), static_cast<uint8_t>(ret->type.value()));
}

TEST_CASE("z_param_Z0_float", "[parser][z_param]")
{
    // Z0.0 should also be treated as Z0
    auto ret = gcode::parser::parse_line("Z0.0");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::M3), static_cast<uint8_t>(ret->type.value()));
}

TEST_CASE("z_param_Z5_float", "[parser][z_param]")
{
    auto ret = gcode::parser::parse_line("Z5.0");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::M5), static_cast<uint8_t>(ret->type.value()));
}

TEST_CASE("z_param_on_G1_overrides_type", "[parser][z_param]")
{
    // Z overrides the type — G1 X Y Z0 → M3, XY irrelevant
    auto ret = gcode::parser::parse_line("G1 X10 Y20 Z0");
    TEST_ASSERT(ret);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(type_t::M3), static_cast<uint8_t>(ret->type.value()));
}


// COMMENT / BLANK LINES

TEST_CASE("comment_pure", "[parser][comment]")
{
    auto ret = gcode::parser::parse_line("; this is a comment");
    TEST_ASSERT(ret);
    TEST_ASSERT_FALSE(ret->type.has_value());
    TEST_ASSERT_FALSE(ret->params.has_value());
}

TEST_CASE("comment_leading_whitespace", "[parser][comment]")
{
    auto ret = gcode::parser::parse_line("   ; G1 X5 Y10");
    TEST_ASSERT(ret);
    TEST_ASSERT_FALSE(ret->type.has_value());
}

TEST_CASE("comment_empty_string", "[parser][comment]")
{
    auto ret = gcode::parser::parse_line("");
    TEST_ASSERT(ret);
    TEST_ASSERT_FALSE(ret->type.has_value());
    TEST_ASSERT_FALSE(ret->params.has_value());
}

TEST_CASE("comment_whitespace_only", "[parser][comment]")
{
    auto ret = gcode::parser::parse_line("     ");
    TEST_ASSERT(ret);
    TEST_ASSERT_FALSE(ret->type.has_value());
}

TEST_CASE("comment_newline_only", "[parser][comment]")
{
    auto ret = gcode::parser::parse_line("\n");
    TEST_ASSERT(ret);
    TEST_ASSERT_FALSE(ret->type.has_value());
}

TEST_CASE("comment_crlf", "[parser][comment]")
{
    auto ret = gcode::parser::parse_line("\r\n");
    TEST_ASSERT(ret);
    TEST_ASSERT_FALSE(ret->type.has_value());
}


// ERROR: INVALID COMMAND

TEST_CASE("err_invalid_G_command", "[parser][error]")
{
    auto ret = gcode::parser::parse_line("G99");
    TEST_ASSERT_FALSE(ret);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(error_t::INVALID_COMMAND), static_cast<int>(ret.error()));
}

TEST_CASE("err_invalid_M_command", "[parser][error]")
{
    auto ret = gcode::parser::parse_line("M99");
    TEST_ASSERT_FALSE(ret);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(error_t::INVALID_COMMAND), static_cast<int>(ret.error()));
}

TEST_CASE("err_invalid_M4", "[parser][error]")
{
    // M4 is not in the command set
    auto ret = gcode::parser::parse_line("M4");
    TEST_ASSERT_FALSE(ret);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(error_t::INVALID_COMMAND), static_cast<int>(ret.error()));
}

TEST_CASE("err_unknown_command_letter", "[parser][error]")
{
    // T is not a valid command letter
    auto ret = gcode::parser::parse_line("T1");
    TEST_ASSERT_FALSE(ret);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(error_t::INVALID_COMMAND), static_cast<int>(ret.error()));
}

TEST_CASE("err_unknown_command_letter_S", "[parser][error]")
{
    auto ret = gcode::parser::parse_line("S100");
    TEST_ASSERT_FALSE(ret);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(error_t::INVALID_COMMAND), static_cast<int>(ret.error()));
}

// ERROR: INVALID SYNTAX

TEST_CASE("err_unknown_param_letter_on_G0", "[parser][error]")
{
    // A is not a valid parameter
    auto ret = gcode::parser::parse_line("G0 X5.0 A10.0");
    TEST_ASSERT_FALSE(ret);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(error_t::INVALID_SYNTAX), static_cast<int>(ret.error()));
}

TEST_CASE("err_unknown_param_letter_on_G1", "[parser][error]")
{
    auto ret = gcode::parser::parse_line("G1 X5.0 Y3.0 E100");
    TEST_ASSERT_FALSE(ret);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(error_t::INVALID_SYNTAX), static_cast<int>(ret.error()));
}

TEST_CASE("err_Z_invalid_value", "[parser][error]")
{
    // Z3 is neither Z0 nor Z5
    auto ret = gcode::parser::parse_line("Z3");
    TEST_ASSERT_FALSE(ret);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(error_t::INVALID_SYNTAX), static_cast<int>(ret.error()));
}

TEST_CASE("err_Z_negative_value", "[parser][error]")
{
    auto ret = gcode::parser::parse_line("Z-1");
    TEST_ASSERT_FALSE(ret);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(error_t::INVALID_SYNTAX), static_cast<int>(ret.error()));
}


// ERROR: MISSING PARAMETER (letter with no following number)

TEST_CASE("err_G_no_number", "[parser][error]")
{
    // 'G' with no number after it
    auto ret = gcode::parser::parse_line("G X5.0");
    TEST_ASSERT_FALSE(ret);
}

TEST_CASE("err_X_no_number", "[parser][error]")
{
    auto ret = gcode::parser::parse_line("G0 X Y10.0");
    TEST_ASSERT_FALSE(ret);
}

TEST_CASE("err_Y_no_number", "[parser][error]")
{
    auto ret = gcode::parser::parse_line("G0 X5.0 Y");
    TEST_ASSERT_FALSE(ret);
}

TEST_CASE("err_F_no_number", "[parser][error]")
{
    auto ret = gcode::parser::parse_line("G1 X5.0 Y3.0 F");
    TEST_ASSERT_FALSE(ret);
}

TEST_CASE("err_X_nonnumeric", "[parser][error]")
{
    auto ret = gcode::parser::parse_line("G0 Xabc Y10.0");
    TEST_ASSERT_FALSE(ret);
}

TEST_CASE("err_Y_nonnumeric", "[parser][error]")
{
    auto ret = gcode::parser::parse_line("G0 X5.0 Ywhat");
    TEST_ASSERT_FALSE(ret);
}

TEST_CASE("err_M_no_number", "[parser][error]")
{
    auto ret = gcode::parser::parse_line("M");
    TEST_ASSERT_FALSE(ret);
}
