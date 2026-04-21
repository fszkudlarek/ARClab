cmake_minimum_required(VERSION 3.22)

add_library(stm32cubemx INTERFACE)

target_compile_definitions(stm32cubemx INTERFACE
    USE_HAL_DRIVER
    STM32L476xx
    $<$<CONFIG:Debug>:DEBUG>
)

target_include_directories(stm32cubemx INTERFACE
    ${CMAKE_SOURCE_DIR}/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32L4xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32L4xx_HAL_Driver/Inc/Legacy
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32L4xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
)

file(GLOB HAL_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/Drivers/STM32L4xx_HAL_Driver/Src/*.c
)

target_sources(stm32cubemx INTERFACE
    ${HAL_SOURCES}
    ${CMAKE_SOURCE_DIR}/startup/startup_stm32l476xx.s
)

# FreeRTOS (only if the project ships it)
if(EXISTS ${CMAKE_SOURCE_DIR}/FreeRTOS)
    target_include_directories(stm32cubemx INTERFACE
        ${CMAKE_SOURCE_DIR}/FreeRTOS/include
        ${CMAKE_SOURCE_DIR}/FreeRTOS/portable/GCC/ARM_CM4F
    )
    target_sources(stm32cubemx INTERFACE
        ${CMAKE_SOURCE_DIR}/FreeRTOS/croutine.c
        ${CMAKE_SOURCE_DIR}/FreeRTOS/event_groups.c
        ${CMAKE_SOURCE_DIR}/FreeRTOS/list.c
        ${CMAKE_SOURCE_DIR}/FreeRTOS/queue.c
        ${CMAKE_SOURCE_DIR}/FreeRTOS/tasks.c
        ${CMAKE_SOURCE_DIR}/FreeRTOS/timers.c
        ${CMAKE_SOURCE_DIR}/FreeRTOS/portable/GCC/ARM_CM4F/port.c
        ${CMAKE_SOURCE_DIR}/FreeRTOS/portable/MemMang/heap_4.c
    )
endif()
