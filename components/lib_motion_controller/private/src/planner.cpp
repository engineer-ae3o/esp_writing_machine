#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "planner.hpp"
#include "config.hpp"
#include "utils.hpp"


namespace gcode::planner {

    static types::config_t s_config{};
    static bool s_is_absolute_coordinates{true};
    static bool s_is_unit_mm{true};

    struct position_t {
        float x{}, y{};

        const bool operator==(const position_t& other) const {
            return (x == other.x) && (y == other.y);
        }
        
        position_t operator+(const position_t& other) {
            return { x + other.x, y + other.y };
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
        
        // Where we are starting our motion relative from
        const position_t& old = s_is_absolute_coordinates ? HOME_AXES : s_current;

        // Where we want to end up
        const position_t final = {
            .x = coord.x.value_or(0.0f) + old.x,
            .y = coord.y.value_or(0.0f) + old.y
        };

        move_to(final, old, coord.feed_rate.value_or(config::DEFAULT_FEED_RATE));
        s_current = final;
    }
    
    static void move_to(const position_t& final, const position_t& initial, uint32_t feed_rate = config::DEFAULT_FEED_RATE) {
        // No movement. We are already there
        if (final == initial) return;

        // Delta for both axes. These are either in milimetres or inches
        const float dx = final.x - initial.x;
        const float dy = final.y - initial.y;

        // Get direction for both axes
        const types::direction_t x_dir = (dx >= 0) ? types::direction_t::FORWARD : types::direction_t::BACKWARD;
        const types::direction_t y_dir = (dy >= 0) ? types::direction_t::FORWARD : types::direction_t::BACKWARD;
        
        // Convert to milimetres based on the `s_is_unit_mm` flag
        const float dx_mm = s_is_unit_mm ? dx : dx * 25.4f;
        const float dy_mm = s_is_unit_mm ? dy : dy * 25.4f;
        
        // Number of steps. Loss in precision is ineveitable. Also, make positive
        const uint32_t dx_steps = static_cast<uint32_t>(std::abs(dx_mm) * config::STEPS_PER_MM);
        const uint32_t dy_steps = static_cast<uint32_t>(std::abs(dy_mm) * config::STEPS_PER_MM);

        // Get speed `(steps/sec)` from feedrate `(mm/min)` parameter
        const uint32_t speed = static_cast<uint32_t>(feed_rate * (config::STEPS_PER_MM / 60.0f));

        if (s_config.send_steps_sync) {
            // God have mercy
            s_config.send_steps_sync.value()(dx_steps, x_dir, speed, dy_steps, y_dir, speed);
            s_current = final;
            return;
        }

        // Brasenham's line interpolation algorithm implementation if
        // no function is present to synchronously move both motors
        const uint32_t dominant_axis_steps = (dx_steps >= dy_steps) ? dx_steps : dy_steps;
        const bool is_x_dominant = dominant_axis_steps == dx_steps;
        const uint32_t recessive_axis_steps = is_x_dominant ? dy_steps : dx_steps;
        
        int32_t error = dominant_axis_steps - recessive_axis_steps;
        uint32_t steps_remaining = dominant_axis_steps;

        uint32_t dominant_steps = 0;

        while (steps_remaining--) {
            // Increment dominant step counter
            dominant_steps++;

            error -= recessive_axis_steps;
            if (error < 0) {
                // Step dominant axis
                is_x_dominant ? s_config.send_steps_x(dominant_steps, x_dir, speed)
                              : s_config.send_steps_y(dominant_steps, y_dir, speed);

                // Step recessive axis
                !is_x_dominant ? s_config.send_steps_x(1, x_dir, speed)
                               : s_config.send_steps_y(1, y_dir, speed);

                error += dominant_axis_steps;
                dominant_steps = 0;
            }
        }

        // Step dominant axis if there are any leftover steps on the dominant axis
        if (dominant_steps > 0) {
            is_x_dominant ? s_config.send_steps_x(dominant_steps, x_dir, speed)
                          : s_config.send_steps_y(dominant_steps, y_dir, speed);
        }

        // Update our position
        s_current = final;
    }

} // namespace gcode::planner
