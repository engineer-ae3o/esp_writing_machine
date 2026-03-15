#ifndef PLANNER_HPP_
#define PLANNER_HPP_


#include "parser.hpp"


namespace gcode::planner {

    void plan_motion(const parser::line_t& line);
    
} // namespace gcode::planner


#endif // PLANNER_HPP_