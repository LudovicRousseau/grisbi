#!/bin/bash

# fail on first error
set -e

# fail on undefined variable
set -u

# debug
set -x

if [ -z "$JHBUILD_LIBDIR" ]
then
	echo "Use: jhbuild shell"
	exit
fi

BUILD_DIR="snapshot"
rm -rf "$BUILD_DIR"

meson setup "$BUILD_DIR" --prefix="$PREFIX" "$@"

meson compile -C "$BUILD_DIR"
meson install -C "$BUILD_DIR"

rm -f MacOS/Grisbi-*.dmg
rm -rf MacOS/dist

# CPU architecture
ARCH=$(uname -m)

# extract version from config.h
GRISBI_VERSION=$(grep VERSION $BUILD_DIR/config.h | cut -f 3 -d ' ' | tr -d '"')

sed -e "s/VERSION/$GRISBI_VERSION/" MacOS/Info.plist.in > MacOS/Info.plist

GRIBSI_BUNDLE_PATH=. gtk-mac-bundler MacOS/Grisbi.bundle

./MacOS/manual_add.sh

# comment the line bellow to avoid code signing
SIGN_ID="E2CY6P89NY"

if [ "$SIGN_ID" ]
then
    codesign --sign "$SIGN_ID" --timestamp MacOS/dist/Grisbi.app/Contents/MacOS/Grisbi
    codesign --sign "$SIGN_ID" --timestamp MacOS/dist/Grisbi.app/Contents/Resources/lib/*.dylib
    codesign --sign "$SIGN_ID" --timestamp MacOS/dist/Grisbi.app/Contents/Resources/lib/*/*/*/*.so
    codesign --sign "$SIGN_ID" --timestamp MacOS/dist/Grisbi.app/Contents/Resources/lib/*/*/*/*/*.so

    # verify the code signature
    codesign --verify --display --verbose --verbose MacOS/dist/Grisbi.app/Contents/MacOS/Grisbi
fi

CUSTOM_ARGS=""
if [ "$(uname -m)" = "arm64" ]
then
    echo "On ARM CPU"
    CUSTOM_ARGS+="--add-file CtrlClickOpenMe.command CtrlClickOpenMe.command 325 100"
fi

(
cd MacOS
touch .this-is-the-create-dmg-repo
./create-dmg \
	--volname Grisbi \
	--volicon Grisbi.icns \
	--window-size 640 400 \
	--background background.png \
	--icon-size 96 \
	--app-drop-link 500 250 \
	--icon "Grisbi.app" 150 250 \
	$CUSTOM_ARGS \
	Grisbi-"$GRISBI_VERSION"-"$ARCH".dmg \
	dist
)

rm -r "$BUILD_DIR"
