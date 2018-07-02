#!/bin/bash

if [ ! $BARRIER_BUILD_ENV ]; then

    printf "Modifying environment for Barrier build..."

    if command -v brew; then
        printf "Detected Homebrew"
        QT_PATH=$(brew --prefix qt)
        OPENSSL_PATH=$(brew --prefix openssl)

		export BARRIER_BUILD_BREW=1
        export CMAKE_PREFIX_PATH="$QT_PATH:$CMAKE_PREFIX_PATH"
        export LD_LIBRARY_PATH="$OPENSSL_PATH/lib:$LD_LIBRARY_PATH"
        export CPATH="$OPENSSL_PATH/include:$CPATH"
        export PKG_CONFIG_PATH="$OPENSSL_PATH/lib/pkgconfig:$PKG_CONFIG_PATH"

    elif command -v port; then
        printf "Detected Macports"

		if [ ! -d /opt/local/lib/cmake/Qt5 ]; then
            printf "Please install qt5-qtbase port"
        fi
		export BARRIER_BUILD_MACPORTS=1
		export CMAKE_PREFIX_PATH="/opt/local/lib/cmake/Qt5:$CMAKE_PREFIX_PATH"
		export LD_LIBRARY_PATH="/opt/local/lib:$LD_LIBRARY_PATH"
		export CPATH="/opt/local/include:$CPATH"
        export PKG_CONFIG_PATH="/opt/local/libexec/qt5/lib/pkgconfig:$PKG_CONFIG_PATH"
    else
        printf "Neither Homebrew nor Macports is installed. Can't get dependency paths"
        exit 1
    fi

    export BARRIER_BUILD_ENV=1

    printf "done\n"
fi
