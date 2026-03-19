#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "planner.hpp"
#include "config.hpp"


namespace gcode::planner {

    static controller::config_t s_config{};
    static bool s_is_absolute_coordinates{true};
    static bool s_is_unit_mm{true};

    struct position_t {
        float x{}, y{};
    };

    // Origin
    static constexpr position_t HOME_AXES = {
        .x = 0.0f, .y = 0.0f
    };

    // To keep track of where we are during any and all motion
    static position_t current{};


    // Forward declaration
    static inline void pen_up();
    static inline void pen_down();
    static void step_motors(const parser::param_t& param);
    static void move_to(const position_t& final, const position_t& initial);

    // Public API
    void init(const controller::config_t& config) {
        s_config = config;
        current = {};
    }

    motion_return_t plan_motion(const parser::line_t& line) {

        // If no type, that means a pure comment line
        if (!line.type) return motion_return_t::SUCCESS;

        switch (line.type.value()) {
            case parser::type_t::G0:
                step_motors(line.params.value());
                return motion_return_t::SUCCESS;

            case parser::type_t::G1:
                step_motors(line.params.value());
                return motion_return_t::SUCCESS;

            case parser::type_t::G20:
                s_is_unit_mm = false;
                return motion_return_t::SUCCESS;

            case parser::type_t::G21:
                s_is_unit_mm = true;
                return motion_return_t::SUCCESS;

            case parser::type_t::G28:
                pen_up();
                move_to(HOME_AXES, current);
                return motion_return_t::SUCCESS;

            case parser::type_t::G90:
                s_is_absolute_coordinates = true;
                return motion_return_t::SUCCESS;

            case parser::type_t::G91:
                s_is_absolute_coordinates = false;
                return motion_return_t::SUCCESS;

            case parser::type_t::M2:
                reset();
                return motion_return_t::END;
                
            case parser::type_t::M3:
                pen_down();
                return motion_return_t::SUCCESS;

            case parser::type_t::M5:
                pen_up();
                return motion_return_t::SUCCESS;

            default:
                return motion_return_t::ERROR;
        }
    }
    
    void reset() {
        pen_up();
        move_to(HOME_AXES, current);
        current = {};
    }

    void teardown() {
        s_config = {};
        current = {};
    }

    // Static helpers
    static inline void pen_up() {
        s_config.servo_set_angle(config::SERVO_PEN_UP_ANGLE);
        vTaskDelay(pdMS_TO_TICKS(config::PEN_DWELL_TIME_MS));
    }

    static inline void pen_down() {
        s_config.servo_set_angle(config::SERVO_PEN_DOWN_ANGLE);
        vTaskDelay(pdMS_TO_TICKS(config::PEN_DWELL_TIME_MS));
    }
    
    static void step_motors(const parser::param_t& param) {
        // TODO: Step motors
    }
    
    static void move_to(const position_t& final, const position_t& initial) {
        // TODO: Implement movement
    }

} // namespace gcode::planner
