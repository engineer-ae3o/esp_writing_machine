#ifndef CONTROLLER_HPP_
#define CONTROLLER_HPP_


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "parser.hpp"
#include "config.hpp"
#include "types.hpp"

#include <cstdint>
#include <atomic>
#include <cstdio>
#include <array>


namespace gcode::controller {

    class controller_t {
    private:
        types::config_t m_config{};
        types::session_t m_session{};

        std::atomic<bool> m_is_initialized{};
        std::atomic<bool> m_session_active{};
        std::atomic<bool> m_shutdown_requested{};

        QueueHandle_t m_event_queue{};

        // Is modified by the planner task and can be read by another thread
        std::atomic<types::state_t> m_state{types::state_t::SLEEPING};
        
    public:
        // Remove the default constructor
        controller_t() = delete;

        // Will never run as it's a single static instance, but good practice
        ~controller_t();

        // Non copyable
        controller_t(const controller_t&) = delete;
        controller_t& operator=(const controller_t&) = delete;

        // Non moveable
        controller_t(controller_t&&) = delete;
        controller_t& operator=(controller_t&&) = delete;

        // Useable functions
        [[nodiscard]] static controller_t& get_instance(const types::config_t& config);
        void init(const types::config_t& config);
        void create_session(const types::session_t& session);
        void send_event(types::event_t event);
        [[nodiscard]] types::state_t get_state();
        void shutdown();

        // Utility functions for conversion
        [[nodiscard]] static constexpr const char* event_to_string(types::event_t event);
        [[nodiscard]] static constexpr const char* controller_state_to_string(types::state_t state);

    private:
        controller_t(const types::config_t&);
        void cleanup();
        static void planner_task(void* arg);
        types::error_t get_error_from_parser_error(parser::error_t error);
    };

} // namespace gcode::controller


#endif // CONTROLLER_HPP_