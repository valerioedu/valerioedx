cmake_minimum_required(VERSION 3.16)

project(valerioedx ASM C)

set(ARCH "AArch64")

# FOR ARM NATIVE MACHINES
set(CMAKE_C_COMPILER gcc)
set(CMAKE_ASM_COMPILER as)
set(CMAKE_LINKER ld)

set(CMAKE_C_FLAGS "-march=armv8-a -ffreestanding -fno-builtin -O2")
set(CMAKE_ASM_FLAGS "-march=armv8-a")

set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_LINKER} <LINK_FLAGS> <OBJECTS> -o <TARGET>")

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
    LINK_FLAGS "-T ${LINKER_SCRIPT}"
    LINK_DEPENDS ${LINKER_SCRIPT}
)

add_custom_target(run
    COMMAND qemu-system-aarch64 -M virt -cpu cortex-a53 -kernel kernel.elf -device ramfb
    DEPENDS kernel.elf
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running kernel in QEMU"
)