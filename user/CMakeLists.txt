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
        set(CMAKE_AR ar)
        set(CMAKE_RANLIB ranlib)
    else()
        set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
        set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
        set(CMAKE_LINKER aarch64-linux-gnu-ld)
        set(CMAKE_AR aarch64-linux-gnu-ar)
        set(CMAKE_RANLIB aarch64-linux-gnu-ranlib)
    endif()
#MacOS
else()
    set(CMAKE_C_COMPILER aarch64-elf-gcc)
    set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
    set(CMAKE_LINKER aarch64-elf-ld)
    set(CMAKE_AR aarch64-elf-ar)
    set(CMAKE_RANLIB aarch64-elf-ranlib)
endif()

set(CMAKE_C_FLAGS "-march=armv8-a -ffreestanding -fno-pie -fno-builtin -O0")
set(CMAKE_ASM_FLAGS "-march=armv8-a")

set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_C_COMPILER> <LINK_FLAGS> <OBJECTS> <LINK_LIBRARIES> -o <TARGET>")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

file(GLOB_RECURSE LIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/crt0.S
)

file(GLOB BUILTIN_COMMANDS
    ${CMAKE_CURRENT_SOURCE_DIR}/programs/sh/commands/*.c
)

add_library(libc STATIC ${LIB_SOURCES})

add_executable(init.elf 
    ${CMAKE_CURRENT_SOURCE_DIR}/init.c
)

target_link_libraries(init.elf libc)

macro(add_user_util name)
    add_executable(${name}.elf
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/${name}.c
    )
    
    target_link_libraries(${name}.elf libc)
    
    set_target_properties(${name}.elf PROPERTIES
        LINK_FLAGS "-Ttext=0x400000 -nostdlib"
    )
    list(APPEND USER_COMMANDS ${name}.elf)
endmacro()

macro(add_user_util_sbin name)
    add_executable(${name}.elf
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/${name}.c
    )
    
    target_link_libraries(${name}.elf libc)
    
    set_target_properties(${name}.elf PROPERTIES
        LINK_FLAGS "-Ttext=0x400000 -nostdlib"
    )
    list(APPEND USER_COMMANDS_SBIN ${name}.elf)
endmacro()

macro(build_shell name)
    add_executable(${name}.elf
        ${CMAKE_CURRENT_SOURCE_DIR}/programs/sh/sh.c
        ${BUILTIN_COMMANDS}
    )
    
    target_link_libraries(${name}.elf libc)
    
    set_target_properties(${name}.elf PROPERTIES
        LINK_FLAGS "-Ttext=0x400000 -nostdlib"
    )
endmacro()

macro(build_program name)
    add_executable(${name}.elf
        ${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}/${name}.c
    )
    
    target_link_libraries(${name}.elf libc)
    
    set_target_properties(${name}.elf PROPERTIES
        LINK_FLAGS "-Ttext=0x400000 -nostdlib"
    )
endmacro()

add_user_util(cat)
add_user_util(ls)
add_user_util(mkdir)
add_user_util(echo)
add_user_util(rmdir)
add_user_util(touch)
add_user_util_sbin(reboot)
add_user_util_sbin(halt)
add_user_util(printenv)
add_user_util(clear)
add_user_util_sbin(shutdown)
add_user_util(rm)
build_shell(sh)
build_program(vbasic)

option(VALERIOEDX_DEBUG "Enable DEBUG compile-time flag" OFF)

set_target_properties(init.elf PROPERTIES
    LINK_FLAGS "-Ttext=0x400000 -nostdlib"
)

add_custom_target(copy
    DEPENDS ${USER_COMMANDS}
    COMMAND ${CMAKE_COMMAND} -E echo "vale::0:0:root:/:/bin/sh" > ${CMAKE_BINARY_DIR}/passwd
    COMMAND mcopy -i ../../kernel/arch/aarch64/disk.img ${CMAKE_BINARY_DIR}/passwd ::/etc/passwd
)

file(GLOB LIB_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/*.h
)

foreach(h ${LIB_HEADERS})
    get_filename_component(hname ${h} NAME)
    add_custom_command(TARGET copy POST_BUILD
        COMMAND mcopy -i ../../kernel/arch/aarch64/disk.img ${h} ::/usr/include/${hname}
    )
endforeach()

foreach(cmd ${USER_COMMANDS})
    get_filename_component(cmdname ${cmd} NAME_WE)
    add_custom_command(TARGET copy POST_BUILD
        COMMAND mcopy -i ../../kernel/arch/aarch64/disk.img $<TARGET_FILE:${cmd}> ::/bin/${cmdname}
    )
endforeach()

foreach(cmd ${USER_COMMANDS_SBIN})
    get_filename_component(cmdname ${cmd} NAME_WE)
    add_custom_command(TARGET copy POST_BUILD
        COMMAND mcopy -i ../../kernel/arch/aarch64/disk.img $<TARGET_FILE:${cmd}> ::/sbin/${cmdname}
    )
endforeach()

add_custom_command(TARGET copy POST_BUILD
    COMMAND mcopy -i ../../kernel/arch/aarch64/disk.img $<TARGET_FILE:sh.elf> ::/bin/sh
    COMMAND mcopy -i ../../kernel/arch/aarch64/disk.img $<TARGET_FILE:vbasic.elf> ::/bin/vbasic
    COMMAND mcopy -i ../../kernel/arch/aarch64/disk.img $<TARGET_FILE:init.elf> ::/bin/init
)