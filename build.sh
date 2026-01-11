#!/bin/sh

CFLAGS='-Wall -Wextra -Wswitch-enum -ggdb -Werror=vla'
SOURCE=main.c
TARGET=kc

build() {
	set -o xtrace
	cc $CFLAGS -o "$TARGET" "$SOURCE"
}

build_run() {
	build && ./"$TARGET" "$@"
}

case "$1" in
	'run')   shift && build_run "$@" ;;
	'build') build ;;
	*)       build ;;
esac
