# Sourced by the macOS build/run scripts. The port runs x86-64 PS4 code in-process, so on Apple
# Silicon the whole process is x86_64 under Rosetta 2: every library it loads comes from the
# x86_64 prefix built by scripts/macos_deps.sh, never from /opt/homebrew (arm64).
# MACOS_DEPS overrides the default prefix (out/macos-deps/prefix).
macos_port_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
export MACOS_DEPS=${MACOS_DEPS:-$macos_port_dir/out/macos-deps/prefix}
# Rosetta AVX/AVX2 (macOS 15+) runs regardless, but CPUID only reports it with this set.
export ROSETTA_ADVERTISE_AVX=1
export MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-15.0}
# Only the x86_64 prefix: pkg-config must not fall back to Homebrew's arm64 .pc files.
export PKG_CONFIG_LIBDIR="$MACOS_DEPS/lib/pkgconfig:$MACOS_DEPS/share/pkgconfig"
unset PKG_CONFIG_PATH
# Vulkan loader -> MoltenVK (the ICD manifest installed by scripts/macos_deps.sh).
export VK_DRIVER_FILES="$MACOS_DEPS/share/vulkan/icd.d/MoltenVK_icd.json"
export DYLD_FALLBACK_LIBRARY_PATH="$MACOS_DEPS/lib${DYLD_FALLBACK_LIBRARY_PATH:+:$DYLD_FALLBACK_LIBRARY_PATH}"
