#!/usr/bin/env bash
# xemu Steam Deck Setup — EmuDeck Integration
#
# Installs the custom xemu binary and creates a wrapper script that
# EmuDeck/Steam ROM Manager can use instead of the Flatpak version.
#
# The existing EmuDeck xemu config (~/.var/app/app.xemu.xemu/) is reused,
# so BIOS files, HDD image, and game library settings are preserved.
#
# Usage (on the Steam Deck):
#   ./setup-steamdeck.sh [path/to/xemu-steamdeck]
#
# Then in EmuDeck, point xemu's launch command to:
#   ~/Applications/xemu-steamdeck-launch.sh

set -e

BINARY_SRC="${1:-dist/xemu-steamdeck}"
INSTALL_DIR="$HOME/Applications/xemu-steamdeck"
WRAPPER="$HOME/Applications/xemu-steamdeck-launch.sh"

# EmuDeck Flatpak config location (preserved — not replaced)
EMUDECK_CONFIG_DIR="$HOME/.var/app/app.xemu.xemu/data/xemu/xemu"
EMUDECK_CONFIG="$EMUDECK_CONFIG_DIR/xemu.toml"

if [ ! -f "$BINARY_SRC" ]; then
    echo "ERROR: Binary not found at '$BINARY_SRC'"
    echo "Build it first with: ./build-steamdeck.sh"
    exit 1
fi

echo "=== xemu Steam Deck Setup ==="
echo ""

# Install binary
mkdir -p "$INSTALL_DIR"
cp "$BINARY_SRC" "$INSTALL_DIR/xemu"
chmod +x "$INSTALL_DIR/xemu"
echo "[1/3] Binary installed: $INSTALL_DIR/xemu"

# Create EmuDeck config dir if it doesn't exist (first install)
mkdir -p "$EMUDECK_CONFIG_DIR"

# Create wrapper script
cat > "$WRAPPER" << 'WRAPPER_EOF'
#!/usr/bin/env bash
# xemu Steam Deck wrapper — launches custom build with EmuDeck config
#
# This script is used by EmuDeck/Steam ROM Manager as the xemu executable.
# It reuses the existing EmuDeck xemu config so BIOS paths and settings
# set via the EmuDeck GUI remain intact.

# Tell xemu it's running on Steam Deck (enables platform optimizations)
export SteamDeck=1

CONFIG="$HOME/.var/app/app.xemu.xemu/data/xemu/xemu/xemu.toml"
BINARY="$HOME/Applications/xemu-steamdeck/xemu"

if [ ! -f "$BINARY" ]; then
    echo "ERROR: xemu-steamdeck binary not found at $BINARY" >&2
    echo "Re-run setup-steamdeck.sh to reinstall." >&2
    exit 1
fi

exec "$BINARY" -enable-kvm -config_path "$CONFIG" "$@"
WRAPPER_EOF

chmod +x "$WRAPPER"
echo "[2/3] Wrapper script created: $WRAPPER"

echo "[3/3] Setup complete."
echo ""
echo "=========================================="
echo "  How to use with EmuDeck"
echo "=========================================="
echo ""
echo "Option A — Custom emulator (recommended):"
echo "  1. Open EmuDeck → Manage Emulators → Xbox"
echo "  2. Set the launch command to:"
echo "       $WRAPPER"
echo "  3. Run Steam ROM Manager to update shortcuts"
echo ""
echo "Option B — Direct Steam shortcut:"
echo "  Add a game to Steam with:"
echo "    Target:     $WRAPPER"
echo "    Arguments:  -dvd_path \"/path/to/your/game.iso\""
echo ""
echo "Your existing EmuDeck xemu config is at:"
echo "  $EMUDECK_CONFIG"
echo "  (BIOS paths and settings are preserved)"
echo ""
echo "Gamescope + FSR launch example:"
echo "  gamescope -w 1280 -h 800 -r 60 --fsr-upscaling -- \\"
echo "    $WRAPPER -dvd_path \"/path/to/scda.iso\""
