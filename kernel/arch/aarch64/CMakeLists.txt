cmake_minimum_required(VERSION 3.16)

project(valerioedx ASM C)

set(ARCH "AArch64")

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
        set(CMAKE_C_COMPILER gcc)
        set(CMAKE_ASM_COMPILER as)
        set(CMAKE_LINKER ld)
    else()
        set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
        set(CMAKE_ASM_COMPILER aarch64-linux-gnu-as)
        set(CMAKE_LINKER aarch64-linux-gnu-ld)
    endif()
#MacOS
else()
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
        set(CMAKE_C_COMPILER gcc)
        set(CMAKE_ASM_COMPILER as)
        set(CMAKE_LINKER ld)
    else()
        set(CMAKE_C_COMPILER arm-none-eabi-gcc)
        set(CMAKE_ASM_COMPILER arm-none-eabi-as)
        set(CMAKE_LINKER arm-none-eabi-ld)
    endif()
endif()

set(CMAKE_C_FLAGS "-march=armv8-a -mno-outline-atomics -ffreestanding -mgeneral-regs-only -fno-pie -fno-builtin")
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
)

add_executable(kernel.elf ${KERNEL_SOURCES})

set_target_properties(kernel.elf PROPERTIES
    LINK_FLAGS "-T ${LINKER_SCRIPT} -nostdlib"
    LINK_DEPENDS ${LINKER_SCRIPT}
)

add_custom_target(run
    COMMAND qemu-system-aarch64 -M virt -cpu cortex-a53 -kernel kernel.elf -device ramfb -serial mon:stdio -d int
    DEPENDS kernel.elf
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running kernel in QEMU"
)