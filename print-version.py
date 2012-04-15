#!/usr/bin/env python
import os, sys
from Foundation import NSMutableDictionary

# Read the version.
plist = NSMutableDictionary.dictionaryWithContentsOfFile_("build/Release/Plex.app/Contents/Info.plist")
version = plist['CFBundleVersion']
print version
