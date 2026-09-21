add_executable(${PROJECT_NAME}
    tests.c
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    UtilsEventlog
    Unity
)