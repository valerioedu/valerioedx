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
endif()

set(CMAKE_C_FLAGS "-march=armv8-a -ffreestanding -fno-pie -fno-builtin -O0")
set(CMAKE_ASM_FLAGS "-march=armv8-a")

set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_C_COMPILER> <LINK_FLAGS> <OBJECTS> -o <TARGET>")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

file(GLOB_RECURSE LIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

file(GLOB BUILTIN_COMMANDS
    ${CMAKE_CURRENT_SOURCE_DIR}/commands/pwd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/commands/cd.c
)

add_executable(init.elf 
    ${CMAKE_CURRENT_SOURCE_DIR}/init.c
    ${LIB_SOURCES}
    ${BUILTIN_COMMANDS}
)

macro(add_user_command name)
    add_executable(${name}.elf
        ${CMAKE_CURRENT_SOURCE_DIR}/commands/${name}.c
        ${LIB_SOURCES}
    )
    set_target_properties(${name}.elf PROPERTIES
        LINK_FLAGS "-Ttext=0x400000 -nostdlib"
    )
    list(APPEND USER_COMMANDS ${name}.elf)
endmacro()

add_user_command(cat)
add_user_command(ls)
add_user_command(mkdir)
add_user_command(echo)

option(VALERIOEDX_DEBUG "Enable DEBUG compile-time flag" OFF)

set_target_properties(init.elf PROPERTIES
    LINK_FLAGS "-Ttext=0x400000 -nostdlib"
)

add_custom_target(copy
    DEPENDS init.elf cat.elf ls.elf mkdir.elf echo.elf
    COMMAND mcopy -i ../../kernel/arch/aarch64/disk.img ../lib/stdint.h ::/usr/include/stdint.h
    COMMAND mcopy -i ../../kernel/arch/aarch64/disk.img ../lib/stddef.h ::/usr/include/stddef.h
    COMMAND mcopy -i ../../kernel/arch/aarch64/disk.img ../lib/string.h ::/usr/include/string.h
    COMMAND mcopy -i ../../kernel/arch/aarch64/disk.img ../lib/unistd.h ::/usr/include/unistd.h
    COMMAND mcopy -i ../../kernel/arch/aarch64/disk.img init.elf ::/bin/init.elf
    COMMAND mcopy -i ../../kernel/arch/aarch64/disk.img cat.elf ::/bin/cat
    COMMAND mcopy -i ../../kernel/arch/aarch64/disk.img ls.elf ::/bin/ls
    COMMAND mcopy -i ../../kernel/arch/aarch64/disk.img mkdir.elf ::/bin/mkdir
    COMMAND mcopy -i ../../kernel/arch/aarch64/disk.img echo.elf ::/bin/echo
)