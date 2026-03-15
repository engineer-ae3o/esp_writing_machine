#ifndef PLANNER_HPP_
#define PLANNER_HPP_


#include "controller.hpp"
#include "parser.hpp"


namespace gcode::planner {

    enum class motion_return_t : uint8_t {
        SUCCESS = 0,
        END,
        ERROR
    };

    void init(const controller::config_t& config);
    motion_return_t plan_motion(const parser::line_t& line);
    void teardown();
    
} // namespace gcode::planner


#endif // PLANNER_HPP_