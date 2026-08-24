#!/bin/bash
set -e

# Root of your Retro68 build folder. Override with:
#   RETRO68_PATH=/path/to/Retro68-build bash build-fat.bash
# Defaults to the Retro68-build dir that sits next to this repo.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RETRO68_PATH="${RETRO68_PATH:-$SCRIPT_DIR/../Retro68-build}"

# check for nproc to count processors to build parallel
BUILD_PARALLEL=
if command -v nproc 2>&1 >/dev/null
then
  BUILD_PARALLEL="--parallel $(nproc)"
fi

################################################################################
echo "building PPC..."

rm -rf build-ppc
mkdir build-ppc

cmake -S . -B build-ppc -DCMAKE_TOOLCHAIN_FILE=$RETRO68_PATH/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake -DMBEDTLS_FATAL_WARNINGS=OFF
cmake --build build-ppc $BUILD_PARALLEL

################################################################################
echo "building m68k..."

rm -rf build-m68k
mkdir build-m68k

cmake -S . -B build-m68k -DCMAKE_TOOLCHAIN_FILE=$RETRO68_PATH/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake -DMBEDTLS_FATAL_WARNINGS=OFF
cmake --build build-m68k $BUILD_PARALLEL

################################################################################
echo "Rez-ing it all together..."

rm -rf build-fat
mkdir build-fat

$RETRO68_PATH/toolchain/bin/Rez \
$RETRO68_PATH/toolchain/m68k-apple-macos/RIncludes/RetroPPCAPPL.r \
-I$RETRO68_PATH/toolchain/m68k-apple-macos/RIncludes \
-DCFRAG_NAME="\"SevenTTY\"" \
--copy build-m68k/SevenTTY.code.bin \
-o build-fat/SevenTTY-fat.bin \
--cc build-fat/SevenTTY-fat.dsk --cc build-fat/SevenTTY-fat.APPL --cc build-fat/%SevenTTY-fat.ad \
-t APPL -c SSH7 \
--data build-ppc/SevenTTY.pef build-ppc/resources.r.rsrc.bin build-ppc/symbolfont.r.rsrc.bin

echo "done."
