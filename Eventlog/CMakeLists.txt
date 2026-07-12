set(SOURCES
        eventlog.c
)

set(HEADERS
        eventlog.h
        endianness.h
)

set(LIB_NAME UtilsEventlog)

add_library(${LIB_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${LIB_NAME} PUBLIC ./)

target_link_libraries(${LIB_NAME} PUBLIC
        UtilsFlashIncrement
)
