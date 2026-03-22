#!/usr/bin/env bash
# xemu Build Script — Steam Deck (AMD Zen 2 + RDNA2)
#
# Builds a custom xemu binary optimized for Steam Deck hardware with:
#   - AMD Zen 2 CPU tuning (-march=znver2)
#   - KVM hardware virtualization for maximum emulation performance
#   - Steam Deck platform detection and SCDA game patches included
#
# Usage:
#   ./build-steamdeck.sh
#   ./build-steamdeck.sh -j8          # custom job count
#
# Output: dist/xemu-steamdeck
# Then run: ./setup-steamdeck.sh      # to install for EmuDeck

set -e
set -o pipefail
set -o physical

project_source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

# Steam Deck APU: AMD Van Gogh, Zen 2 CPU cores + RDNA 2 GPU
export CFLAGS="${CFLAGS} -O2 -march=znver2 -mtune=znver2"

job_count=12
for arg in "$@"; do
    case "$arg" in
        -j*) job_count="${arg:2}" ;;
    esac
done
if [ -z "$job_count" ] && command -v nproc >/dev/null; then
    job_count=$(nproc)
fi

package_linux() {
    rm -rf dist
    mkdir -p dist
    cp build/qemu-system-i386 dist/xemu-steamdeck
    chmod +x dist/xemu-steamdeck
    echo ""
    echo "Build complete: $(pwd)/dist/xemu-steamdeck"
    echo "Next step: copy to Steam Deck and run ./setup-steamdeck.sh"
}

configure="${project_source_dir}/configure"

set -x

"${configure}" \
    --extra-cflags="-DXBOX=1 -Wno-error=redundant-decls ${CFLAGS}" \
    --extra-ldflags="" \
    --target-list=i386-softmmu \
    --enable-kvm \
    --disable-werror \
    -Db_lto=true \
    "$@"

time make -j"${job_count}" qemu-system-i386 2>&1 | tee build-steamdeck.log

package_linux
