#!/usr/bin/env bash
# macOS: builds the port's libraries as x86_64 into out/macos-deps/prefix (macos-env.sh).
# The game's eboot is x86-64 code that runs inside our process, so on Apple Silicon the process
# (and every library it loads) is x86_64 and runs under Rosetta 2. Homebrew there is arm64, so
# only build tools (cmake, ninja, pkgconf, nasm, glslang) and header-only Boost come from it.
# Usage: scripts/macos_deps.sh [--clean]   (each library is built once; --clean starts over)
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
source ./macos-env.sh
root=$PWD/out/macos-deps
src=$root/src build=$root/build prefix=$MACOS_DEPS
if [[ ${1:-} == --clean ]]; then rm -rf "$root"; fi
mkdir -p "$src" "$build" "$prefix"
for tool in git curl cmake ninja pkg-config nasm glslang; do
    command -v "$tool" >/dev/null || { echo "Need $tool: brew install cmake ninja pkgconf nasm glslang boost" >&2; exit 1; }
done
jobs=$(sysctl -n hw.ncpu)
min="-mmacosx-version-min=$MACOSX_DEPLOYMENT_TARGET"

# fetch <name> <git url> <tag> [git clone flags]: shallow clone of a release tag, once.
fetch() {
    local name=$1 url=$2 tag=$3; shift 3
    [[ -d $src/$name ]] || git -c advice.detachedHead=false clone -q --depth 1 --branch "$tag" "$@" "$url" "$src/$name"
}
# step <name> <command...>: runs once (stamp file), log in out/macos-deps/build/<name>.log.
# Under `if !` bash ignores set -e inside the command, so the build functions chain with &&.
step() {
    local name=$1; shift
    if [[ -f $build/$name.done ]]; then echo "== $name: already built"; return; fi
    echo "== $name"
    if ! "$@" > "$build/$name.log" 2>&1; then
        tail -30 "$build/$name.log" >&2
        echo "$name failed (full log: $build/$name.log)" >&2; exit 1
    fi
    touch "$build/$name.done"
}
# cmake_install <name> <source dir> [cmake args]: x86_64 build, installed into the prefix.
# CMAKE_IGNORE_PREFIX_PATH keeps Homebrew's arm64 libraries out of every find_package.
cmake_install() {
    local name=$1 dir=$2; shift 2
    cmake -S "$dir" -B "$build/$name" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET" \
        -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_PREFIX_PATH="$prefix" \
        -DCMAKE_IGNORE_PREFIX_PATH="/opt/homebrew;/usr/local" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 "$@" &&
    cmake --build "$build/$name" -j "$jobs" &&
    cmake --install "$build/$name"
}

# Vulkan: headers from the GPU submodule, the loader at the same version, MoltenVK (Vulkan on Metal).
step vulkan-headers cmake_install vulkan-headers gpu/third_party/vulkan-headers
fetch vulkan-loader https://github.com/KhronosGroup/Vulkan-Loader.git v1.4.354
step vulkan-loader cmake_install vulkan-loader "$src/vulkan-loader" -DBUILD_TESTS=OFF \
    -DVULKAN_HEADERS_INSTALL_DIR="$prefix"
moltenvk() {
    # Release build (universal: x86_64 + arm64); building it needs the full Xcode.
    local dylib json
    curl -fL --retry 3 -o "$src/MoltenVK-macos.tar" \
        https://github.com/KhronosGroup/MoltenVK/releases/download/v1.4.2/MoltenVK-macos.tar &&
    rm -rf "$src/moltenvk" && mkdir -p "$src/moltenvk" "$prefix/share/vulkan/icd.d" &&
    tar -xf "$src/MoltenVK-macos.tar" -C "$src/moltenvk" &&
    dylib=$(find "$src/moltenvk" -path '*dynamic*' -name libMoltenVK.dylib -print -quit) &&
    json=$(find "$src/moltenvk" -name MoltenVK_icd.json -print -quit) &&
    lipo -info "$dylib" | grep -q x86_64 &&
    cp "$dylib" "$prefix/lib/" &&
    # The manifest points at the dylib relative to itself.
    sed 's#"library_path"[^,]*#"library_path": "../../../lib/libMoltenVK.dylib"#' "$json" \
        > "$prefix/share/vulkan/icd.d/MoltenVK_icd.json"
}
step moltenvk moltenvk

fetch sdl3 https://github.com/libsdl-org/SDL.git release-3.4.18
step sdl3 cmake_install sdl3 "$src/sdl3" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF \
    -DSDL_EXAMPLES=OFF
# Zydis' installed CMake package requires Zycore installed too: the copy its submodule pins.
fetch zydis https://github.com/zyantific/zydis.git v4.1.1 --recursive --shallow-submodules
step zycore cmake_install zycore "$src/zydis/dependencies/zycore" -DZYCORE_BUILD_SHARED_LIB=OFF
step zydis cmake_install zydis "$src/zydis" -DZYDIS_BUILD_TOOLS=OFF -DZYDIS_BUILD_EXAMPLES=OFF \
    -DZYDIS_BUILD_SHARED_LIB=OFF
fetch xxhash https://github.com/Cyan4973/xxHash.git v0.8.4
step xxhash cmake_install xxhash "$src/xxhash/build/cmake" -DXXHASH_BUILD_XXHSUM=OFF -DBUILD_SHARED_LIBS=OFF
fetch fmt https://github.com/fmtlib/fmt.git 12.2.0
step fmt cmake_install fmt "$src/fmt" -DFMT_TEST=OFF -DFMT_DOC=OFF
fetch robin-map https://github.com/Tessil/robin-map.git v1.4.1
step robin-map cmake_install robin-map "$src/robin-map"
fetch vma https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git v3.4.0
step vma cmake_install vma "$src/vma"
fetch freetype https://github.com/freetype/freetype.git VER-2-14-3
# Built-in zlib: the SDK's has no .pc file for the x86_64-only pkg-config path.
step freetype cmake_install freetype "$src/freetype" -DBUILD_SHARED_LIBS=OFF -DFT_DISABLE_BZIP2=ON \
    -DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON \
    -DFT_DISABLE_ZLIB=ON

# FFmpeg has its own configure. --disable-autodetect: nothing from Homebrew sneaks in;
# zlib (PNG decoder) comes from the macOS SDK, which is universal.
fetch ffmpeg https://git.ffmpeg.org/ffmpeg.git n8.0.3
ffmpeg() {
    mkdir -p "$build/ffmpeg" && cd "$build/ffmpeg" &&
    "$src/ffmpeg/configure" --prefix="$prefix" --enable-cross-compile --target-os=darwin --arch=x86_64 \
        --cc=clang --cxx=clang++ --extra-cflags="-arch x86_64 $min" --extra-ldflags="-arch x86_64 $min" \
        --x86asmexe=nasm --enable-shared --disable-static --disable-programs --disable-doc \
        --disable-network --disable-avdevice --disable-avfilter --disable-autodetect --enable-zlib &&
    make -j "$jobs" &&
    make install
}
step ffmpeg ffmpeg

echo "Dependencies in $prefix:"
for lib in "$prefix"/lib/*.dylib; do [[ -L $lib ]] || echo "  $(basename "$lib"): $(lipo -archs "$lib")"; done
