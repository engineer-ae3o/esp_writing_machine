idf_component_register(
                       SRCS "test_controller.cpp"
                            "test_parser.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES unity lib_motion_controller
)
