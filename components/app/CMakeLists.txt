idf_component_register (
                        SRCS "display/display.cpp"
                             "screens/screens.cpp"
                             "server/server.cpp"
                        INCLUDE_DIRS "config/"
                                     "display/"
                                     "screens/"
                                     "server/"
                                     "utils/"
                        REQUIRES freertos esp_timer esp_wifi esp_http_server esp_event
                                 nvs_flash esp_netif drivers lib_motion_controller littlefs
)
