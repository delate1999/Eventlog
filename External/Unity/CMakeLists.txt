set(SOURCES
        unity.c
)

set(HEADERS
        unity.h
)

add_library(Unity STATIC ${SOURCES} ${HEADERS})
target_include_directories(Unity PUBLIC
    Unity
)