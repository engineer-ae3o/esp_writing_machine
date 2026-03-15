# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

list(APPEND EXTRA_COMPONENT_DIRS "components/lib_motion_controller/test")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(writing_machine)

# Has already been flashed. No need to reflash
# littlefs_create_partition_image(storage ./data FLASH_IN_PROJECT)
