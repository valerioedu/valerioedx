cmake_minimum_required(VERSION 3.16)

project(valerioedx ASM_NASM C)

set(ARCH "x64")
set(TARGET_TRIPLE "x86_64-linux-gnu")

set(CMAKE_C_COMPILER ${TARGET_TRIPLE}-gcc)
set(CMAKE_ASM_NASM_COMPILER nasm)

set(CMAKE_C_FLAGS "-m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -mno-red-zone")
set(CMAKE_ASM_NASM_FLAGS "-f elf32")

set(CMAKE_LINKER ${TARGET_TRIPLE}-ld)
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
    "${CMAKE_CURRENT_SOURCE_DIR}/cpu/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/firmware/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/cpu/*.asm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../mm/*.c"
)

add_executable(kernel.elf ${KERNEL_SOURCES})

set_target_properties(kernel.elf PROPERTIES
    LINK_FLAGS "-m elf_i386 -T ${LINKER_SCRIPT} -nostdlib"
    LINK_DEPENDS ${LINKER_SCRIPT}
)

add_custom_target(run
    COMMAND qemu-system-x86_64 -kernel kernel.elf
    DEPENDS kernel.elf
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running kernel in QEMU"
)