idf_component_register (
                        SRCS "graphics/graphics.c"
                             "fonts/fonts.c"
                        INCLUDE_DIRS "graphics/"
                                     "fonts/"
                        REQUIRES freertos
)
