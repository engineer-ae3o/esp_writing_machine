idf_component_register (
                        SRCS "menu/menu.cpp"
                             "screens/screens.cpp"
                             "server/server.cpp"
                             "utils/utils.cpp"
                        INCLUDE_DIRS "config/"
                                     "menu/"
                                     "screens/"
                                     "server/"
                                     "utils/"
                        REQUIRES freertos esp_timer esp_wifi esp_http_server esp_event nvs_flash
                                 esp_netif drivers lib_graphics lib_motion_controller littlefs
)
