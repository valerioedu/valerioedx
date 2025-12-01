cmake_minimum_required(VERSION 3.16)

project(valerioedx ASM C)

set(ARCH "AArch64")

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
        set(CMAKE_C_COMPILER gcc)
        set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
        set(CMAKE_LINKER ld)
    else()
        set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
        set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
        set(CMAKE_LINKER aarch64-linux-gnu-ld)
    endif()
#MacOS
else()
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
        set(CMAKE_C_COMPILER gcc)
        set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
        set(CMAKE_LINKER ld)
    else()
        set(CMAKE_C_COMPILER arm-none-eabi-gcc)
        set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
        set(CMAKE_LINKER arm-none-eabi-ld)
    endif()
endif()

set(CMAKE_C_FLAGS "-march=armv8-a -mno-outline-atomics -ffreestanding -fno-pie -fno-builtin")
set(CMAKE_ASM_FLAGS "-march=armv8-a")

set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_C_COMPILER> <LINK_FLAGS> <OBJECTS> -o <TARGET>")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../shell/lib
)

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

file(GLOB_RECURSE KERNEL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/firmware/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/*.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mm/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../threading/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../threading/*.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../../kernel.c
)

add_executable(kernel.elf ${KERNEL_SOURCES})

option(VALERIOEDX_DEBUG "Enable DEBUG compile-time flag" OFF)

if(VALERIOEDX_DEBUG)
    target_compile_definitions(kernel.elf PRIVATE DEBUG=1)
endif()

target_compile_definitions(kernel.elf PRIVATE ARM=1)

set_target_properties(kernel.elf PROPERTIES
    LINK_FLAGS "-T ${LINKER_SCRIPT} -nostdlib"
    LINK_DEPENDS ${LINKER_SCRIPT}
)

add_custom_target(run
    COMMAND dtc -I dts -O dtb -o virt.dtb ../virt.dts
    COMMAND qemu-system-aarch64 -M virt -m 2G -cpu cortex-a53 -kernel kernel.elf -device ramfb -serial mon:stdio -dtb virt.dtb
    DEPENDS kernel.elf
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running kernel in QEMU"
)

add_custom_target(debug
    COMMAND dtc -I dts -O dtb -o virt.dtb ../virt.dts
    COMMAND qemu-system-aarch64 -M virt -m 2G -cpu cortex-a53 -kernel kernel.elf -device ramfb -serial mon:stdio -dtb virt.dtb -d int
    DEPENDS kernel.elf
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running kernel in QEMU using interrupts debug"
)