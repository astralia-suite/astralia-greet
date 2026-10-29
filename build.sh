#!/usr/bin/env bash

# Build script for astralia-greet
# Use:
# <empty>: build and run the unit tests
# setup: install dependencies
# build: build only
# test: build and run the unit tests
# install: build, install system-wide, and enable the greeter as the display manager
# run: build, render a preview image and open the greeter in a window
# uninstall: disable the greeter and remove the system-wide install
# ASTRALIA_GREET_BUILD_JOBS=N: parallel compile jobs (default: all cores)

set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")"

BIN=astralia-greet
JOBS="${ASTRALIA_GREET_BUILD_JOBS:-$(nproc)}"

cmd_setup() {
    sudo pacman -Syu --needed base-devel meson ninja pkgconf libdrm cairo pango fontconfig libxkbcommon libxkbcommon-x11 libinput systemd systemd-libs libjpeg-turbo libpng libxcb libxcursor seatd pam xorg-server xorg-xauth
}

cmd_build() {
    if [[ -d build && "$(cat build/.source-dir 2>/dev/null)" != "$PWD" ]]; then
        rm -rf build
    fi
    if [[ ! -f build/build.ninja ]]; then
        meson setup build
    fi
    printf '%s\n' "$PWD" > build/.source-dir
    meson compile -C build -j "$JOBS"
}

cmd_test() { cmd_build; meson test -C build --print-errorlogs; }
cmd_default() { cmd_test; }
cmd_run() {
    cmd_build
    "./build/$BIN" --preview preview.png
    echo "preview: $PWD/preview.png"
    "./build/$BIN" --window
}
cmd_install() {
    cmd_build
    sudo meson install -C build --no-rebuild
    sudo systemd-sysusers
    sudo systemctl daemon-reload
    local current
    current="$(systemctl show -P Id display-manager.service 2>/dev/null || true)"
    if [[ -n "$current" && "$current" != display-manager.service && "$current" != astralia-greet.service ]]; then
        sudo systemctl disable --now "$current"
    fi
    sudo systemctl enable --force astralia-greet.service
    sudo systemctl restart astralia-greet.service
}
cmd_uninstall() {
    sudo systemctl disable --now astralia-greet.service
    sudo ninja -C build uninstall
    sudo systemctl daemon-reload
    echo "no display manager is enabled: enable one, e.g. sudo systemctl enable sddm.service"
}

main() {
    local cmd="${1:-default}"
    case "$cmd" in
    default | setup | build | test | install | run | uninstall) shift || true; "cmd_$cmd" "$@" ;;
    *) echo "unknown command: $cmd" >&2; exit 2 ;;
    esac
}

main "$@"
