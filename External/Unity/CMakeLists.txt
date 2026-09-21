set(SOURCES
        unity.c
)

set(HEADERS
        unity_internals.h
        unity.h
)

set(LIB_NAME Unity)

add_library(${LIB_NAME} STATIC ${SOURCES} ${HEADERS})
target_include_directories(${LIB_NAME} PUBLIC ./)