#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "planner.hpp"
#include "config.hpp"


namespace gcode::planner {

    static types::config_t s_config{};
    static bool s_is_absolute_coordinates{true};
    static bool s_is_unit_mm{true};

    struct position_t {
        float x{}, y{};

        const bool operator==(const position_t& other) const {
            return (x == other.x) && (y == other.y);
        }
        
        position_t& operator+(const position_t& other) {
            x += other.x;
            y += other.y;
            return *this;
        }
    };

    // Origin
    static constexpr position_t HOME_AXES = {
        .x = 0.0f, .y = 0.0f
    };

    // To keep track of where we are during any and all motion
    static position_t s_current{};


    // Forward declaration
    static inline void pen_up();
    static inline void pen_down();
    static void step_motors(const parser::param_t& param);
    static void move_to(const position_t& final, const position_t& initial, uint32_t feed_rate = config::DEFAULT_FEED_RATE);

    // Public API
    void init(const types::config_t& config) {
        s_config = config;
        s_current = {};
    }

    ret_t motion(const parser::line_t& line) {

        // If no type, that means a pure comment line
        if (!line.type) return ret_t::SUCCESS;

        switch (line.type.value()) {
            case parser::type_t::G0:
                step_motors(line.params.value());
                return ret_t::SUCCESS;

            case parser::type_t::G1:
                step_motors(line.params.value());
                return ret_t::SUCCESS;

            case parser::type_t::G20:
                s_is_unit_mm = false;
                return ret_t::SUCCESS;

            case parser::type_t::G21:
                s_is_unit_mm = true;
                return ret_t::SUCCESS;

            case parser::type_t::G28:
                pen_up();
                move_to(HOME_AXES, s_current);
                return ret_t::SUCCESS;

            case parser::type_t::G90:
                s_is_absolute_coordinates = true;
                return ret_t::SUCCESS;

            case parser::type_t::G91:
                s_is_absolute_coordinates = false;
                return ret_t::SUCCESS;

            case parser::type_t::M2:
                reset();
                return ret_t::END;
                
            case parser::type_t::M3:
                pen_down();
                return ret_t::SUCCESS;

            case parser::type_t::M5:
                pen_up();
                return ret_t::SUCCESS;

            default:
                return ret_t::ERROR;
        }
    }
    
    void reset() {
        pen_up();
        move_to(HOME_AXES, s_current);
        s_current = {};
    }

    void teardown() {
        reset();
        s_config = {};
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
    
    static void step_motors(const parser::param_t& coord) {
        
        // Where we are starting our motion from
        const position_t& old = s_is_absolute_coordinates ? HOME_AXES : s_current;

        // Where we want to end up
        position_t final = {
            .x = coord.x.value_or(0.0f),
            .y = coord.y.value_or(0.0f)
        };

        move_to(final, old, coord.feed_rate.value_or(config::DEFAULT_FEED_RATE));

        s_current = final + old;
    }
    
    static void move_to(const position_t& final, const position_t& initial, uint32_t feed_rate = config::DEFAULT_FEED_RATE) {
        // No movement. We are already there
        if (final == initial) return;


    }

} // namespace gcode::planner
