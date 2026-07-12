set(SOURCES
        flash_increment.c
)

set(HEADERS
        flash_increment.h
)

set(LIB_NAME UtilsFlashIncrement)

add_library(${LIB_NAME} STATIC ${SOURCES} ${HEADERS})

target_include_directories(${LIB_NAME} PUBLIC ./)