idf_component_register(
                       SRCS "main.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES freertos driver esp_timer littlefs
                                app drivers lib_graphics lib_motion_controller
)
