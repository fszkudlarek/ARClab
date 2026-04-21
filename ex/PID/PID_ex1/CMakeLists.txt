cmake_minimum_required(VERSION 3.22)

project(PID_ex1 C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

add_subdirectory(cmake/stm32cubemx)

add_executable(${PROJECT_NAME})

file(GLOB USER_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Src/*.c)
target_sources(${PROJECT_NAME} PRIVATE ${USER_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE Inc)

target_link_libraries(${PROJECT_NAME} PRIVATE stm32cubemx)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.bin
)
