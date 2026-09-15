cmake_minimum_required(VERSION 3.10.0)
project(Eventlog_example VERSION 0.1.0 LANGUAGES C)

add_executable(Eventlog_example main.c)

add_subdirectory(Eventlog)
add_subdirectory(FlashIncrement)

target_link_libraries(Eventlog_example PRIVATE 
    UtilsEventlog
)
