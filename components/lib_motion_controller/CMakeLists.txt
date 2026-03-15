idf_component_register(
                       SRCS "private/src/controller.cpp"
                            "private/src/planner.cpp"
                            "private/src/parser.cpp"

                       INCLUDE_DIRS "private/inc/"
                                    "inc/"

                       REQUIRES freertos
)
