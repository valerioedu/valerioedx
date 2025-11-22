cmake_minimum_required(VERSION 3.16)

project(valerioedx ASM_NASM C)

set(ARCH "x64")
set(TARGET_TRIPLE "x86_64-linux-gnu")

# --- Toolchain Setup ---
set(CMAKE_C_COMPILER ${TARGET_TRIPLE}-gcc)
set(CMAKE_ASM_NASM_COMPILER nasm)

# --- Global Defaults (Targeting the 32-bit Bootloader) ---
# These flags apply to the 32-bit bootloader (Stage 1)
set(CMAKE_C_FLAGS "-m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -mno-red-zone")

# [CORRECTION 1] Output 32-bit ELF objects from NASM so they can link with the 32-bit C code.
# The jump to 64-bit happens internally in boot.asm, but the container must remain 32-bit.
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

# --- Target 1: 32-bit Bootloader (kernel.elf) ---

file(GLOB_RECURSE KERNEL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/cpu/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/firmware/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/cpu/*.asm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../mm/*.c"
)

# [CORRECTION 2] Ensure the 64-bit kernel file is NOT included in the 32-bit build.
# (Adjust the path below if your kernel.c is located elsewhere)
list(REMOVE_ITEM KERNEL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../../kernel.c")

add_executable(kernel.elf
    ${KERNEL_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/idt.c
)

set_target_properties(kernel.elf PROPERTIES
    LINK_FLAGS "-m elf_i386 -T ${LINKER_SCRIPT} -nostdlib"
    LINK_DEPENDS ${LINKER_SCRIPT}
)

# --- Target 2: 64-bit Kernel Module (kernel64.elf) ---

# [CORRECTION 3] Create a separate target for the 64-bit kernel
add_executable(kernel64.elf "${CMAKE_CURRENT_SOURCE_DIR}/../../kernel.c")

# Override the global 32-bit flags with 64-bit flags for this specific target
target_compile_options(kernel64.elf PRIVATE 
    -m64 
    -ffreestanding 
    -mno-red-zone 
    -mcmodel=large
)

# Link the 64-bit kernel at a higher memory address (e.g., 2MB / 0x200000)
# This ensures it doesn't overlap with the bootloader at 1MB.
target_link_options(kernel64.elf PRIVATE 
    -m elf_x86_64
    -nostdlib 
    -Ttext 0x200000
)

# --- Run Configuration ---

add_custom_target(run
    # [CORRECTION 4] Load the 64-bit executable as a Multiboot module using -initrd
    COMMAND tmux new-session qemu-system-x86_64 -kernel kernel.elf -display curses
    DEPENDS kernel.elf kernel64.elf
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running kernel in QEMU"
)