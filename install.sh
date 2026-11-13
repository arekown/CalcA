#!/usr/bin/env bash
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# CalcA installer: builds, tests and installs CalcA (default: for the current user into ~/.local).
set -euo pipefail

APP_ID=io.github.arekown.CalcA
PREFIX="$HOME/.local"
ACTION=install
DEPS=0

usage() {
    cat <<'EOF'
Usage: ./install.sh [options]

  (no option)     build, test and install CalcA for the current user (~/.local)
  --deps          install the build dependencies first (dnf, apt or pacman; uses sudo)
  --system        install for all users into /usr/local (uses sudo)
  --prefix DIR    install into DIR instead
  --uninstall     remove CalcA from the prefix (combine with --system/--prefix if used)
  -h, --help      show this help
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --deps) DEPS=1 ;;
        --system) PREFIX=/usr/local ;;
        --prefix) shift; PREFIX="${1:?--prefix needs a directory}" ;;
        --uninstall) ACTION=uninstall ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
    shift
done

cd "$(dirname "$(readlink -f "$0")")"

# sudo only when the prefix is not writable for us
SUDO=""
target="$PREFIX"
while [ ! -e "$target" ]; do target="$(dirname "$target")"; done
[ -w "$target" ] || SUDO=sudo

refresh_menu() {
    command -v update-desktop-database >/dev/null && $SUDO update-desktop-database -q "$PREFIX/share/applications" 2>/dev/null || true
    # only refresh an existing icon cache; a new, unmaintained one would hide icons installed later
    if [ -f "$PREFIX/share/icons/hicolor/icon-theme.cache" ] && command -v gtk-update-icon-cache >/dev/null; then
        $SUDO gtk-update-icon-cache -q -t "$PREFIX/share/icons/hicolor" 2>/dev/null || true
    fi
    command -v kbuildsycoca6 >/dev/null && kbuildsycoca6 --noincremental >/dev/null 2>&1 || true
}

if [ "$ACTION" = uninstall ]; then
    $SUDO rm -f "$PREFIX/bin/calca" \
                "$PREFIX/share/applications/$APP_ID.desktop" \
                "$PREFIX/share/icons/hicolor/scalable/apps/$APP_ID.svg"
    refresh_menu
    echo "CalcA removed from $PREFIX."
    exit 0
fi

if [ "$DEPS" = 1 ]; then
    if command -v dnf >/dev/null; then
        sudo dnf install -y cmake ninja-build gcc-c++ pkgconf-pkg-config gmp-devel mpfr-devel \
            qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qttools-devel
    elif command -v apt-get >/dev/null; then
        sudo apt-get install -y cmake ninja-build g++ pkg-config libgmp-dev libmpfr-dev \
            qt6-base-dev qt6-declarative-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
            qml6-module-qtquick qml6-module-qtquick-layouts qml6-module-qtquick-shapes \
            qml6-module-qtquick-effects qml6-module-qtquick-window qml6-module-qtcore
    elif command -v pacman >/dev/null; then
        sudo pacman -S --needed cmake ninja gcc pkgconf gmp mpfr qt6-base qt6-declarative qt6-tools
    else
        echo "Unknown package manager - please install the dependencies listed in README.md." >&2
        exit 1
    fi
fi

GEN=()
command -v ninja >/dev/null && GEN=(-G Ninja)
[ -f build/CMakeCache.txt ] && GEN=() # keep the generator of an existing build directory

cmake -S . -B build "${GEN[@]}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build build --parallel
ctest --test-dir build --output-on-failure
$SUDO cmake --install build
refresh_menu

echo
echo "CalcA is installed in $PREFIX - open your application menu and type \"calc\"."
