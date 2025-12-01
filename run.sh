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
        -h|--help)
            echo "Usage: ./run.sh -machine [MACHINE] [OPTIONS]"
            echo ""
            echo "Machines available:"
            echo "    x64,     QEMU x86_64 virtual machine"
            echo "    virt,    QEMU Arm virtual machine"
            echo ""
            echo "Options:"
            echo "    --no-clean    Skip cleaning build directory"
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
        if $DEBUG; then
            cmake -DVALERIOEDX_DEBUG=ON ..
            make debug
        else
            cmake -DVALERIOEDX_DEBUG=OFF ..
            make run
        fi
        cd ../
        rm -rf build
        ;;
esac