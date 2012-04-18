#!/bin/sh
rm -rf build/Release/Plex.app
xcodebuild -project Plex.xcodeproj
./prep-release.sh