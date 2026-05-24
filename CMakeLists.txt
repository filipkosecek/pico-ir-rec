cmake_minimum_required(VERSION 3.13)

set(INC_DIR ${CMAKE_CURRENT_LIST_DIR}/inc)
set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/src)
set(SRC_DIR_PROT ${CMAKE_CURRENT_LIST_DIR}/src/protocols)

add_library(pico-ir-rec STATIC
        ${SRC_DIR}/ir_driver.c
        ${SRC_DIR_PROT}/nec.c
)

set_target_properties(pico-ir-rec PROPERTIES
        C_STANDARD 17
        C_EXTENSIONS ON
)

target_compile_options(pico-ir-rec PRIVATE
        -W
        -Wall
        -Wextra
        -Werror
)

# IR driver settings
if(NOT IR_DRIVER_PIO_BANK)
        set(IR_DRIVER_PIO_BANK pio0)
endif()

if(NOT IR_DRIVER_SM_INDEX)
        set(IR_DRIVER_SM_INDEX 0)
endif()

if(NOT IR_DRIVER_PIO_IRQ_LINE)
        set(IR_DRIVER_PIO_IRQ_LINE 0)
endif()

if(NOT IR_DRIVER_PIN)
        set(IR_DRIVER_PIN 0)
endif()

if(IR_DRIVER_INTERNAL_PULLUP)
        target_compile_definitions(pico-ir-rec PUBLIC IR_DRIVER_INTERNAL_PULLUP)
endif()

target_compile_definitions(pico-ir-rec PUBLIC
        IR_DRIVER_PIO_BANK=${IR_DRIVER_PIO_BANK}
        IR_DRIVER_SM_INDEX=${IR_DRIVER_SM_INDEX}
        IR_DRIVER_PIO_IRQ_LINE=${IR_DRIVER_PIO_IRQ_LINE}
        IR_DRIVER_PIN=${IR_DRIVER_PIN}
)

target_include_directories(pico-ir-rec PUBLIC
        ${INC_DIR}
)

target_link_libraries(pico-ir-rec
        pico_stdlib
        pico_multicore
        hardware_pio
        hardware_gpio
        hardware_irq
        hardware_sync
)

pico_generate_pio_header(pico-ir-rec
        ${SRC_DIR}/ir_sampler.pio
)
