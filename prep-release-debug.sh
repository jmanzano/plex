#!/bin/sh
pushd build/Debug/Plex.app/Contents/Resources/Plex
rm -rf addons/metadata.*
rm -rf addons/skin.mediastream/.git
rm -rf addons/skin.confluence
cd addons/skin.mediastream
../../../../../../../../tools/TexturePacker/TexturePacker -input media
rm media/*
rm -rf media-lite
mv Textures.xbt media/
rm -rf .git