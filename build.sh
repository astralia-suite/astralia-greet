#!/usr/bin/env bash

set -euo pipefail
cd "$(dirname "$0")"

cmd_build() {
	if [ -d build ] && [ "$(cat build/.source-dir 2>/dev/null)" != "$PWD" ]; then
		rm -rf build
	fi
	if [ -d build ]; then
		meson setup --prefix=/usr --reconfigure build
	else
		meson setup --prefix=/usr build
	fi
	printf '%s\n' "$PWD" > build/.source-dir
	ninja -C build -j "${ASTRALIA_GREET_BUILD_JOBS:-4}"
}

cmd_default() { cmd_build; meson test -C build --print-errorlogs; }
cmd_test() {
	cmd_build
	./build/astralia-greet --preview preview.png
	echo "preview: $PWD/preview.png"
	./build/astralia-greet --window
}
cmd_install() {
	cmd_build
	sudo ninja -C build install
	sudo systemd-sysusers
	sudo systemctl daemon-reload
	local current
	current="$(systemctl show -P Id display-manager.service 2>/dev/null || true)"
	if [ -n "$current" ] && [ "$current" != display-manager.service ] && [ "$current" != astralia-greet.service ]; then
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
		default|test|install|uninstall) "cmd_$cmd" ;;
		*) echo "unknown command: $cmd" >&2; exit 2 ;;
	esac
}

main "$@"
