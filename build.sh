#!/bin/sh

# Initialize variables
GRN='\033[01;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[01;31m'
RST='\033[0m'
ORIGIN_DIR=$(pwd)
TOOLCHAIN=$ORIGIN_DIR/build-shit
IMAGE=$ORIGIN_DIR/out/arch/arm64/boot/Image
DEVICE=nabu
CONFIG="${DEVICE}_defconfig"
NPROC=$(($(nproc) + 1))
MAKE="-j$NPROC O=out CROSS_COMPILE=aarch64-elf- CROSS_COMPILE_ARM32=arm-eabi- HOSTCC=gcc HOSTCXX=aarch64-elf-g++ CC=aarch64-elf-gcc LD=ld.lld"
LOG_FILE="$ORIGIN_DIR/build-$(date '+%Y-%m-%d_%H-%M-%S').log"
DEFAULT_KV="R4.14"
LOG_PRINT=false
DO_CLEAN=false

# Parse arguments for log on terminal and or full clean build
for arg in "$@"; do
    case $arg in
        log|l)
            LOG_PRINT=true
            ;;
        clean|c)
            DO_CLEAN=true
            ;;
    esac
done

# Export environment variables
export KBUILD_BUILD_USER=RainZ
export KBUILD_BUILD_HOST=Cosmos
export ARCH=arm64
export USE_CCACHE=1
export CCACHE_SLOPPINESS="file_macro,locale,time_macros"
export CCACHE_NOHASHDIR="true"

# Initialize log file
echo "=== Build Log - $(date) ===" > "$LOG_FILE"

script_echo() {
    echo "  $1"
    echo "  $1" >> "$LOG_FILE"
}

log_command() {
    echo ">>> $1" >> "$LOG_FILE"
    if [ "$LOG_PRINT" = true ]; then
        eval "$1" 2>&1 | tee -a "$LOG_FILE"
    else
        eval "$1" >> "$LOG_FILE" 2>&1
    fi
}

exit_script() {
    echo "Build interrupted at $(date)" >> "$LOG_FILE"
    kill -INT $$
}

add_deps() {
    echo -e "${CYAN}"
    if [ ! -d "$TOOLCHAIN" ]; then
        script_echo "Creating build-shit folder"
        mkdir "$TOOLCHAIN"
    fi

    if [ ! -d "$TOOLCHAIN/gcc-arm64" ]; then
        script_echo "Downloading toolchain..."
        cd "$TOOLCHAIN" || exit_script
        script_echo "Cloning gcc-arm64 repository..."
        log_command "git clone https://github.com/KenHV/gcc-arm64.git --single-branch -b master --depth=1"
        script_echo "Cloning gcc-arm repository..."
        log_command "git clone https://github.com/KenHV/gcc-arm.git --single-branch -b master --depth=1"
        cd "$ORIGIN_DIR"
    fi

    verify_toolchain_install
}

verify_toolchain_install() {
    script_echo " "
    if [ -d "$TOOLCHAIN" ]; then
        script_echo "I: Toolchain found at default location"
        PATH="$TOOLCHAIN/gcc-arm64/bin:$TOOLCHAIN/gcc-arm/bin:$PATH"
        export PATH
    else
        script_echo "I: Toolchain not found"
        script_echo "   Downloading recommended toolchain at $TOOLCHAIN..."
        add_deps
    fi
}

build_kernel_image() {
    cleanup
    script_echo " "
    echo -e "${GRN}"
    printf "Write the Kernel version [%s]: " "$DEFAULT_KV"
    read KV
    if [ -z "$KV" ]; then
        KV=$DEFAULT_KV
    fi
    echo -e "${YELLOW}"
    script_echo "Building SnowFlake Kernel For $DEVICE"

    script_echo "Running make config..."
    log_command "make $MAKE LOCALVERSION=\"~SnowFlake\" $CONFIG"
    
    script_echo "Running make..."
    log_command "make $MAKE LOCALVERSION=\"~SnowFlake\""

    SUCCESS=$?
    echo -e "${RST}"

    if [ $SUCCESS -eq 0 ] && [ -f "$IMAGE" ]; then
        echo -e "${GRN}"
        script_echo "------------------------------------------------------------"
        script_echo "Compilation successful..."
        script_echo "Image can be found at out/arch/arm64/boot/Image"
        script_echo "------------------------------------------------------------"
        build_flashable_zip
    elif [ $SUCCESS -eq 130 ]; then
        echo -e "${RED}"
        script_echo "------------------------------------------------------------"
        script_echo "Build force stopped by the user."
        script_echo "------------------------------------------------------------"
        echo -e "${RST}"
    elif [ $SUCCESS -eq 1 ]; then
        echo -e "${RED}"
        script_echo "------------------------------------------------------------"
        script_echo "Compilation failed.."
        script_echo "------------------------------------------------------------"
        echo -e "${RST}"
        cleanup
    fi
}

build_flashable_zip() {
    script_echo " "
    script_echo "I: Building kernel image..."
    echo -e "${GRN}"
    script_echo "Copying kernel image and dtb files..."
    log_command "cp \"$ORIGIN_DIR\"/out/arch/arm64/boot/Image \"$ORIGIN_DIR\"/out/arch/arm64/boot/dtbo.img SnowFlake/"
    log_command "cp \"$ORIGIN_DIR\"/out/arch/arm64/boot/dtb.img SnowFlake/dtb"
    
    script_echo "Creating flashable zip..."
    cd "$ORIGIN_DIR"/SnowFlake/ || exit_script
    log_command "zip -r9 \"SnowFlake-R$KV-$DEVICE.zip\" META-INF version anykernel.sh tools Image dtb dtbo.img"
    
    script_echo "Cleaning up temporary files..."
    log_command "rm -rf Image dtb dtbo.img"
    cd "$ORIGIN_DIR"
}

cleanup() {
    script_echo "Cleaning up build artifacts..."
    log_command "rm -rf \"$ORIGIN_DIR\"/out/arch/arm64/boot/Image"
    log_command "rm -rf \"$ORIGIN_DIR\"/out/arch/arm64/boot/dtb*"
    log_command "rm -rf \"$ORIGIN_DIR\"/SnowFlake/Image"
    log_command "rm -rf \"$ORIGIN_DIR\"/SnowFlake/*.zip"
    log_command "rm -rf \"$ORIGIN_DIR\"/SnowFlake/dtb*"

    if [ "$DO_CLEAN" = true ]; then
        script_echo "Performing full clean-up..."
        log_command "make $MAKE clean"
        log_command "make $MAKE mrproper"
        log_command "rm -rf \"$ORIGIN_DIR\"/out/*"
    fi
}

echo -e "  ██████  ███▄    █  ▒█████   █     █░  █████▒██▓    ▄▄▄       ██ ▄█▀▓█████ "
echo -e "▒██    ▒  ██ ▀█   █ ▒██▒  ██▒▓█░ █ ░█░▓██   ▒▓██▒   ▒████▄     ██▄█▒ ▓█   ▀ "
echo -e "░ ▓██▄   ▓██  ▀█ ██▒▒██░  ██▒▒█░ █ ░█ ▒████ ░▒██░   ▒██  ▀█▄  ▓███▄░ ▒███   "
echo -e "  ▒   ██▒▓██▒  ▐▌██▒▒██   ██░░█░ █ ░█ ░▓█▒  ░▒██░   ░██▄▄▄▄██ ▓██ █▄ ▒▓█  ▄ "
echo -e "▒██████▒▒▒██░   ▓██░░ ████▓▒░░░██▒██▓ ░▒█░   ░██████▒▓█   ▓██▒▒██▒ █▄░▒████▒"
echo -e "▒ ▒▓▒ ▒ ░░ ▒░   ▒ ▒ ░ ▒░▒░▒░ ░ ▓░▒ ▒   ▒ ░   ░ ▒░▓  ░▒▒   ▓▒█░▒ ▒▒ ▓▒░░ ▒░ ░"
echo -e "░ ░▒  ░ ░░ ░░   ░ ▒░  ░ ▒ ▒░   ▒ ░ ░   ░     ░ ░ ▒  ░ ▒   ▒▒ ░░ ░▒ ▒░ ░ ░  ░"
echo -e "░  ░  ░     ░   ░ ░ ░ ░ ░ ▒    ░   ░   ░ ░     ░ ░    ░   ▒   ░ ░░ ░    ░   "
echo -e "      ░           ░     ░ ░      ░               ░  ░     ░  ░░  ░      ░  ░"
echo -e "                                                                            "
echo -e "${CYAN}                            Kernel Build Script"

add_deps
build_kernel_image