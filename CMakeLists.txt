cmake_minimum_required(VERSION 3.10)

project(kvaser-canutils
    VERSION 0.0.1
    DESCRIPTION "KVASER SDK based CAN utility"
    LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "This project is designed for Windows only")
endif()

if(NOT CMAKE_BUILD_TYPE)
	#set(CMAKE_BUILD_TYPE Release)
	set(CMAKE_BUILD_TYPE Debug)
endif()

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
#set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
#set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

set(KVASER_ROOT_DIR "C:/Program Files (x86)/Kvaser/Canlib"
    CACHE PATH "KVASER SDK root directory")

find_path(KVASER_INCLUDE_DIR
    NAMES canlib.h
    PATHS
        ${KVASER_ROOT_DIR}/INC
        ${KVASER_ROOT_DIR}/Inc
        "C:/Program Files/Kvaser/Canlib/INC"
        "C:/Program Files/Kvaser/Canlib/Inc"
    DOC "KVASER SDK include directory"
)

find_library(KVASER_CANLIB
    NAMES canlib32.lib canlib.lib
    PATHS
        ${KVASER_ROOT_DIR}/Lib/x64
        ${KVASER_ROOT_DIR}/lib/x64
        "C:/Program Files/Kvaser/Canlib/Lib/x64"
        "C:/Program Files/Kvaser/Canlib/lib/x64"
    DOC "KVASER SDK library"
)

if(NOT KVASER_INCLUDE_DIR)
    message(FATAL_ERROR "KVASER SDK include directory not found. Please set KVASER_ROOT_DIR correctly.")
endif()

if(NOT KVASER_CANLIB)
    message(FATAL_ERROR "KVASER SDK library not found. Please set KVASER_ROOT_DIR correctly.")
endif()

message(STATUS "KVASER Include: ${KVASER_INCLUDE_DIR}")
message(STATUS "KVASER Library: ${KVASER_CANLIB}")

set(SOURCES
	kv.c
	lib.c
	kvaser.c
	cansend.c
	canplay.c
	candump.c
	linux/lib.c
)

set(HEADERS
    lib.h
	linux/can.h
	linux/lib.h
)


set(EXE_NAME kv)

add_executable(${EXE_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${EXE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${KVASER_INCLUDE_DIR}
)

target_link_libraries(${EXE_NAME} PRIVATE
	${KVASER_CANLIB}
)

if(WIN32)
	target_link_libraries(${EXE_NAME} PRIVATE
        kernel32
        user32
    )
endif()



## test.exe
#set(EXE_NAME test)
#
#set(SOURCES
#	test.c
#	lib.c
#	kvaser.c
#	linux/lib.c
#)
#
#set(HEADERS
#    lib.h
#	linux/can.h
#	linux/lib.h
#)
#add_executable(${EXE_NAME} ${SOURCES} ${HEADERS})
#
#target_include_directories(${EXE_NAME} PRIVATE
#    ${CMAKE_CURRENT_SOURCE_DIR}
#    ${KVASER_INCLUDE_DIR}
#)
#
#target_link_libraries(${EXE_NAME} PRIVATE
#	${KVASER_CANLIB}
#)
#
#if(WIN32)
#	target_link_libraries(${EXE_NAME} PRIVATE
#        kernel32
#        user32
#    )
#endif()
