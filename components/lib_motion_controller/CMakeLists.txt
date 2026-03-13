idf_component_register(
                       SRCS "controller/controller.cpp"
                            "private/parser/parser.cpp"

                       INCLUDE_DIRS "private/parser/"
                                    "controller/"
                                    "private/"

                       REQUIRES freertos
)
