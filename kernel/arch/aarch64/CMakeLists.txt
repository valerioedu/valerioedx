cmake_minimum_required(VERSION 3.16)

if("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Darwin")
    set(CMAKE_SYSTEM_NAME Generic)
    set(CMAKE_OSX_SYSROOT "")
endif()

set(CMAKE_OSX_ARCHITECTURES "")

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

    set(OBJCOPY aarch64-linux-gnu-objcopy)
    if(EXISTS "/usr/share/qemu-efi-aarch64/AAVMF_CODE.fd")
        set(BIOS /usr/share/qemu-efi-aarch64/AAVMF_CODE.fd)
    elseif(EXISTS "/usr/share/AAVMF/AAVMF_CODE.fd")
        set(BIOS /usr/share/AAVMF/AAVMF_CODE.fd)
    else()
        set(BIOS /usr/share/qemu-efi-aarch64/QEMU_EFI.fd)
    endif()
#MacOS
else()
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
        set(CMAKE_C_COMPILER gcc)
        set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
        set(CMAKE_LINKER ld)
    else()
        set(CMAKE_C_COMPILER aarch64-elf-gcc)
        set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
        set(CMAKE_LINKER aarch64-elf-ld)
    endif()

    set(OBJCOPY aarch64-elf-objcopy)
    set(BIOS /opt/homebrew/share/qemu/edk2-aarch64-code.fd)
endif()

set(CMAKE_C_FLAGS "-march=armv8-a -ffreestanding -fno-pie -fno-builtin")
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
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/*.S
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../mm/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../fs/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../syscalls/*.c
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
    COMMAND qemu-system-aarch64 -M virt -m 2G -cpu cortex-a710  
            -bios ${BIOS}
            -drive file=../disk.img,if=none,format=raw,id=hd0
            -device ramfb 
            -device virtio-blk-device,drive=hd0
            -device virtio-keyboard-device
            -serial mon:stdio -dtb virt.dtb
            -device virtio-rng-device
            -device virtio-mouse-device
)

add_custom_target(debug
    COMMAND dtc -I dts -O dtb -o virt.dtb ../virt.dts
    COMMAND ${OBJCOPY} -O binary kernel.elf kernel.img
    COMMAND mcopy -i ../disk.img kernel.img ::/boot/kernel.img
    COMMAND qemu-system-aarch64 -M virt -m 2G -cpu cortex-a710
            -bios ${BIOS}
            -drive file=../disk.img,if=none,format=raw,id=hd0
            -device ramfb 
            -device virtio-blk-device,drive=hd0
            -device virtio-keyboard-device
            -serial mon:stdio -dtb virt.dtb -d int
            -device virtio-rng-device
            -device virtio-mouse-device
    DEPENDS kernel.elf
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running kernel in QEMU using interrupts debug"
)

add_custom_target(build
    COMMAND dtc -I dts -O dtb -o virt.dtb ../virt.dts
    COMMAND ${OBJCOPY} -O binary kernel.elf kernel.img
    COMMAND mcopy -i ../disk.img kernel.img ::/boot/kernel.img
    COMMAND qemu-system-aarch64 -M virt -m 2G -cpu cortex-a710 
            -bios ${BIOS}
            -drive file=../disk.img,if=none,format=raw,id=hd0
            -device ramfb 
            -device virtio-blk-device,drive=hd0
            -device virtio-keyboard-device
            -serial mon:stdio -dtb virt.dtb
            -device virtio-rng-device
            -device virtio-mouse-device
    DEPENDS kernel.elf
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running kernel in QEMU"
)