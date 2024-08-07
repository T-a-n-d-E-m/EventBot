#!/bin/bash

# Build script for EventBot.
# Needs headers and libs for:
#     MariaDB
#     Curl
#     DPP++
#     libfmt

# No, this does not need a Makefile or some other convoluted build system!

if [[ $1 == 'release' ]]; then
	BUILD_MODE="-DRELEASE -O3"
else
	BUILD_MODE="-DDEBUG -g -O3"
fi

# Opts for Howard Hinnant's date/tz library.
LIB_DATE_OPTS="-DINSTALL=/tmp -DHAS_REMOTE_API=1" # -DAUTO_DOWNLOAD=1

# Libraries to link with
LIBS="$(mariadb_config --include --libs) -ldpp -lfmt -lcurl -lpoppler-cpp -lpthread"

# -Wno-volatile for mongoose
# -Wno-unused_function for stbi_resize
time g++ -std=c++20 $BUILD_MODE -DEVENTBOT -Wall -Werror -Wpedantic -Wno-volatile -Wno-unused-function -Wno-maybe-uninitialized -fno-rtti $LIB_DATE_OPTS -I./src/date ./src/tz.cpp ./src/eventbot.cpp $LIBS -o eventbot

# If compiling elsewhere...
if [ "$HOSTNAME" != harvest-sigma ]; then
	strip eventbot
	scp -o PubkeyAuthentication=no eventbot tandem@harvest-sigma.bnr.la:~/dev/EventBot/
fi
