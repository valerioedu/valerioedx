# Valerioedx

**Please know that the development is currently focusing just on Aarch64**

**Valerioedx** is a custom operating system kernel implementation designed to run on **x86_64** and **AArch64** (ARM64) architectures. This project demonstrates core OS concepts including memory management, virtual filesystems, device drivers, and userland process execution.

## Features

* **Multi-Architecture Support**:
    * **x64**: Runs on QEMU x86_64 virtual machines.
    * **AArch64**: Runs on QEMU ARM virtual machines (`virt` machine).
* **Kernel Core**:
    * Preemptive Multitasking and Scheduler.
    * Synchronization primitives (Spinlocks).
    * System Calls implementation (`exec`, `fork`, `open`, `write`, etc.).
    * ELF Binary Loading.
* **Memory Management**:
    * Physical Memory Manager (PMM).
    * Virtual Memory Manager (VMM).
    * Kernel Heap Allocator.
* **Filesystem**:
    * **VFS**: Virtual File System abstraction.
    * **FAT32**: Read/Write support for FAT32 partitions.
    * **DevFS**: Device filesystem mounted at `/dev`.
* **Drivers**:
    * **VirtIO**: Support for VirtIO Block devices and input.
    * **UART/PL011**: Serial communication.
    * **VGA**: Basic display support for x86.
    * **GIC**: Generic Interrupt Controller support for ARM.

## Prerequisites

The project includes an automated build script (`run.sh`) that attempts to install missing dependencies on supported Linux distributions (Debian/Ubuntu, Fedora, Arch, OpenSUSE) and macOS (Homebrew).

To build manually, you will need:
* **QEMU** (`qemu-system-x86_64` and `qemu-system-aarch64`)
* **CMake**
* **GCC** (Host GCC for x64; Cross-compiler `aarch64-elf-gcc` or `aarch64-linux-gnu-gcc` for ARM)
* **Device Tree Compiler** (`dtc`)
* **Dosfstools** (`mkfs.fat`)
* **Mtools** (`mcopy`, `mmd`)

## Building and Running

Use the provided `run.sh` wrapper script to compile and run the kernel in QEMU.

### Basic Usage

```bash
# Run the x86_64 kernel
./run.sh -machine x64

# Run the AArch64 kernel
./run.sh -machine virt
```