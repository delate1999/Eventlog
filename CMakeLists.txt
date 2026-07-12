cmake_minimum_required(VERSION 3.10.0)
project(Eventlog VERSION 0.1.0 LANGUAGES C)

add_executable(Eventlog main.c)

add_subdirectory(Eventlog)
add_subdirectory(FlashIncrement)

target_link_libraries(Eventlog PRIVATE 
    UtilsEventlog
)
