idf_component_register(
                       SRCS "controller/controller.cpp"
                            "parser/parser.cpp"
                       INCLUDE_DIRS .
                                    "controller/"
                                    "parser/"
                       REQUIRES freertos
)
