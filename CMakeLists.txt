cmake_minimum_required(VERSION 3.10.0)

#set(BUILD_TYPE "EXAMPLE")
set(BUILD_TYPE "TESTS")

if(BUILD_TYPE STREQUAL "EXAMPLE")
    project(Eventlog_example VERSION 0.1.0 LANGUAGES C)
    add_executable(${PROJECT_NAME} main.c)
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        UtilsEventlog
    )
elseif(BUILD_TYPE STREQUAL "TESTS")
    project(Eventlog_tests LANGUAGES C)
    add_subdirectory(Tests)
endif()

add_subdirectory(Eventlog)
add_subdirectory(FlashIncrement)
add_subdirectory(External)
