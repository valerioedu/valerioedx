cmake_minimum_required(VERSION 3.16)
project(valerioedx C ASM_NASM)

# ---- Toolchain / arch ----
set(ARCH "x86")   # or leave empty to use default gcc/ld

# C compiler & NASM
if(TARGET_TRIPLE)
    set(CMAKE_C_COMPILER ${TARGET_TRIPLE}-gcc)
    set(CMAKE_LINKER     ${TARGET_TRIPLE}-ld)
else()
    # fall back to system tools
    set(CMAKE_C_COMPILER gcc)
    set(CMAKE_LINKER     ld)
endif()

set(CMAKE_ASM_NASM_COMPILER nasm)

# 32-bit freestanding
set(CMAKE_C_FLAGS "-m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib -mno-red-zone")
set(CMAKE_ASM_NASM_FLAGS "-f elf32")

# Use our linker instead of default linker step
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

# 1. Define the output object file path
set(BOOT_OBJ "${CMAKE_CURRENT_BINARY_DIR}/boot.o")

# 2. Add a custom command to compile boot.asm manually
add_custom_command(
    OUTPUT ${BOOT_OBJ}
    COMMAND nasm ${CMAKE_CURRENT_SOURCE_DIR}/cpu/boot.asm -f elf32 -o ${BOOT_OBJ}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cpu/boot.asm
    COMMENT "Compiling boot.asm manually to 32-bit ELF"
)

# 3. Update KERNEL_SOURCES to exclude the raw .asm and include the compiled .o
file(GLOB_RECURSE KERNEL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/cpu/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/firmware/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../mm/*.c"
)

# 4. Add the manually compiled object to the executable
add_executable(kernel.elf ${KERNEL_SOURCES} ${BOOT_OBJ})

set_target_properties(kernel.elf PROPERTIES
    LINK_FLAGS "-m elf_i386 -T ${LINKER_SCRIPT} -nostdlib"
    LINK_DEPENDS ${LINKER_SCRIPT}
)

# ---- ISO + QEMU run target ----
# assumes grub.cfg is in this directory
set(GRUB_CFG ${CMAKE_CURRENT_SOURCE_DIR}/grub.cfg)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/valerioedx.iso
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/iso/boot/grub
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:kernel.elf> ${CMAKE_BINARY_DIR}/iso/boot/kernel.elf
    COMMAND ${CMAKE_COMMAND} -E copy ${GRUB_CFG} ${CMAKE_BINARY_DIR}/iso/boot/grub/grub.cfg
    COMMAND grub-mkrescue -o ${CMAKE_BINARY_DIR}/valerioedx.iso ${CMAKE_BINARY_DIR}/iso
    DEPENDS kernel.elf ${GRUB_CFG}
    VERBATIM
)

add_custom_target(iso ALL
    DEPENDS ${CMAKE_BINARY_DIR}/valerioedx.iso
)

add_custom_target(run
    COMMAND qemu-system-i386 -cdrom ${CMAKE_BINARY_DIR}/valerioedx.iso -no-reboot -d int,cpu_reset -no-shutdown
    DEPENDS iso
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)