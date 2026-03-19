#ifndef PLANNER_HPP_
#define PLANNER_HPP_


#include "parser.hpp"
#include "types.hpp"


namespace gcode::planner {

    enum class ret_t : uint8_t {
        SUCCESS = 0,
        END,
        ERROR
    };

    void init(const types::config_t& config);
    [[nodiscard]] ret_t motion(const parser::line_t& line);
    void teardown();
    void reset();
    
} // namespace gcode::planner


#endif // PLANNER_HPP_