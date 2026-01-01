#!/bin/bash

if [ $# -eq 0 ]; then
    echo "ERROR: No arguments"
    echo "Usage: ./run.sh -machine [MACHINE]"
    echo ""
    echo "Machines available:"
    echo "    x64,     QEMU x86_64 virtual machine"
    echo "    virt,    QEMU Arm virtual machine"
    exit 1
fi

MACHINE=""
CLEAN=true
DEBUG=false
NEW=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -machine|--machine)
            MACHINE="$2"
            if [[ -z "$MACHINE" ]]; then
                echo "ERROR: No machine specified after $1"
                exit 1
            fi

            if [[ "$MACHINE" == "raspi" ]]; then
                if [[ -n "$3" && ( "$3" == "virt" || "$3" == "board" ) ]]; then
                    VIRT="$3"
                    shift 3
                else
                    VIRT="virt"
                    shift 2
                fi

                case $VIRT in
                    virt)
                        echo "Building for QEMU's virtual machine"
                        ;;
                    board)
                        echo "Building for the hardware board"
                        ;;
                    *)
                        echo "Error: Third argument should be either board/virt"
                        echo "    board: real raspberry pi 3 board"
                        echo "    virt:  QEMU's virtual machine"
                        echo ""
                        echo "No third argument will build valerioedx as virt"
                        exit 1
                        ;;
                esac
            else
                shift 2
            fi
            ;;
        --no-clean)
            CLEAN=false
            shift
            ;;
        -new|--new-disk)
            NEW=true
            shift
            ;;
        -h|--help)
            echo "Usage: ./run.sh -machine [MACHINE] [OPTIONS]"
            echo ""
            echo "Machines available:"
            echo "    x64,     QEMU x86_64 virtual machine"
            echo "    virt,    QEMU Arm virtual machine"
            echo ""
            echo "Options:"
            echo "    --no-clean    Skip cleaning build directory"
            echo "    -new          Force create a new disk image (wipes data)"
            echo "    -d, --debug   Enable debug mode"
            echo "    -h, --help    Show this help message"
            exit 0
            ;;
        -d|--debug)
            DEBUG=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

if [ -z "$MACHINE" ]; then
    echo "ERROR: Machine not specified"
    echo "Use ./run.sh -machine [MACHINE]"
    exit 1
fi

check_and_install() {
    local cmd=$1
    local pkg=$2
    local install_cmd=$3
    local alt_cmd=$4

    if ! command -v "$cmd" &> /dev/null; then
        if [ -n "$alt_cmd" ] && command -v "$alt_cmd" &> /dev/null; then
            return 0
        fi

        echo "$cmd could not be found."
        read -p "Do you want to install it? (y/n) " choice
        case "$choice" in 
          y|Y ) 
            echo "Installing $pkg..."
            eval "$install_cmd"
            ;;
          n|N ) 
            echo "Skipping installation of $pkg. Build might fail."
            ;;
          * ) 
            echo "Invalid input. Skipping."
            ;;
        esac
    fi
}

if [[ "$MACHINE" == "virt" ]]; then
    OS="$(uname -s)"
    case "${OS}" in
        Darwin*)
            # macOS
            if ! command -v brew &> /dev/null; then
                echo "Homebrew is not installed. Please install Homebrew first."
                exit 1
            fi
            check_and_install "dtc" "dtc" "brew install dtc"
            check_and_install "cmake" "cmake" "brew install cmake"
            check_and_install "dosfstools" "dosfstools" "brew install dosfstools"
            check_and_install "aarch64-elf-gcc" "aarch64-elf-gcc" "brew install aarch64-elf-gcc"
            check_and_install "qemu-system-aarch64" "qemu" "brew install qemu"
            ;;
        Linux*)
            PKG_MANAGER=""
            INSTALL_CMD=""
            HOST_ARCH="$(uname -m)"
            
            if command -v apt-get &> /dev/null; then
                PKG_MANAGER="apt"
                INSTALL_CMD="sudo apt-get update && sudo apt-get install -y"
                DTC_PKG="device-tree-compiler"
                CMAKE_PKG="cmake"
                GCC_CROSS_PKG="gcc-aarch64-linux-gnu"
                GCC_NATIVE_PKG="gcc"
                QEMU_PKG="qemu-system-arm" 
            elif command -v dnf &> /dev/null; then
                PKG_MANAGER="dnf"
                INSTALL_CMD="sudo dnf install -y"
                DTC_PKG="dtc"
                CMAKE_PKG="cmake"
                GCC_CROSS_PKG="gcc-aarch64-linux-gnu"
                GCC_NATIVE_PKG="gcc"
                QEMU_PKG="qemu-system-aarch64"
            elif command -v pacman &> /dev/null; then
                PKG_MANAGER="pacman"
                INSTALL_CMD="sudo pacman -S --noconfirm"
                DTC_PKG="dtc"
                CMAKE_PKG="cmake"
                GCC_CROSS_PKG="aarch64-linux-gnu-gcc"
                GCC_NATIVE_PKG="gcc"
                QEMU_PKG="qemu-system-aarch64"
            elif command -v zypper &> /dev/null; then
                PKG_MANAGER="zypper"
                INSTALL_CMD="sudo zypper install -y"
                DTC_PKG="dtc"
                CMAKE_PKG="cmake"
                GCC_CROSS_PKG="cross-aarch64-gcc13"
                GCC_NATIVE_PKG="gcc"
                QEMU_PKG="qemu-arm"
            elif command -v yum &> /dev/null; then
                PKG_MANAGER="yum"
                INSTALL_CMD="sudo yum install -y"
                DTC_PKG="dtc"
                CMAKE_PKG="cmake"
                GCC_CROSS_PKG="gcc-aarch64-linux-gnu"
                GCC_NATIVE_PKG="gcc"
                QEMU_PKG="qemu-system-aarch64"
            fi

            if [ -n "$PKG_MANAGER" ]; then
                check_and_install "dtc" "$DTC_PKG" "$INSTALL_CMD $DTC_PKG"
                check_and_install "cmake" "$CMAKE_PKG" "$INSTALL_CMD $CMAKE_PKG"
                
                if [[ "$HOST_ARCH" == "aarch64" || "$HOST_ARCH" == "arm64" ]]; then
                    check_and_install "gcc" "$GCC_NATIVE_PKG" "$INSTALL_CMD $GCC_NATIVE_PKG"
                else
                    check_and_install "aarch64-linux-gnu-gcc" "$GCC_CROSS_PKG" "$INSTALL_CMD $GCC_CROSS_PKG"
                fi

                check_and_install "qemu-system-aarch64" "$QEMU_PKG" "$INSTALL_CMD $QEMU_PKG"
            else
                echo "Warning: Could not detect a supported package manager (apt, dnf, pacman, zypper, yum)."
                echo "Please ensure dtc, cmake, gcc (or cross-compiler), and qemu-system-aarch64 are installed."
            fi
            ;;
        *)
            echo "Warning: Unknown OS ${OS}, skipping dependency checks."
            ;;
    esac
fi

case $MACHINE in
    x64|virt)
        echo "Building for machine: $MACHINE"
        ;;
    *)
        echo "ERROR: Unknown machine '$MACHINE'"
        echo "Available machines: x64, raspi, oranpi, virt"
        exit 1
        ;;
esac

case $MACHINE in
    x64)
        cd kernel/arch/x64
        rm -rf build
        mkdir build && cd build
        cmake ..
        make run
        cd ../
        rm -rf build
        ;;
    virt)
        cd kernel/arch/aarch64
        rm -rf build
        mkdir build && cd build

        if [ "$NEW" = true ] || [ ! -f ../disk.img ]; then
            cd ..
            dd if=/dev/zero of=disk.img bs=1M count=128 status=none
            /usr/sbin/mkfs.fat -F 32 -I disk.img
            echo "Hello from the Host OS!" > TEST.TXT
            mcopy -i disk.img TEST.TXT ::TEST.TXT
            mmd -i disk.img ::/bin
            mmd -i disk.img ::/sbin
            mmd -i disk.img ::/home
            mmd -i disk.img ::/usr
            mmd -i disk.img ::/usr/bin
            mmd -i disk.img ::/usr/sbin
            mmd -i disk.img ::/usr/include

            #To implement in the cmake file
            aarch64-linux-gnu-gcc -c ../../../user/init.c -o init.o -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -Wall -Wextra -march=armv8-a
            aarch64-linux-gnu-ld init.o -Ttext=0x400000 -o init.elf
            mcopy -i disk.img init.elf ::/bin/init.elf
            cd build
        fi

        if $DEBUG; then
            cmake -DVALERIOEDX_DEBUG=ON ..
            make debug
        else
            cmake -DVALERIOEDX_DEBUG=OFF ..
            make run

        fi
        cd ../
        rm -rf build
        if [ -f init.elf ]; then
            rm init.o
            rm init.elf
        fi
        ;;
esac