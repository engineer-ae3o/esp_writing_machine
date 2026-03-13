idf_component_register (
                        SRCS "buttons/buttons.cpp"
                             "encoder/encoder.cpp"
                             "ili9341/ili9341.c"
                             "st7920/st7920.cpp"
                             "servo/servo.cpp"
                        INCLUDE_DIRS "ili9341/"
                                     "stepper/"
                                     "stepper/"
                                     "encoder/"
                                     "st7920/"
                                     "servo/"
                        REQUIRES freertos driver esp_timer
)
