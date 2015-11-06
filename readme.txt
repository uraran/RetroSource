RetroN 5 open source component source release
---------------------------------------------

Current as of RetroN 5 application v2.2 (build 1789)

core-gameboy    = VBA-M port
core-gba        = VBA-M port (VBA-next fork)
core-genesis    = Genesis Plus GX (forked before v1.6.0 when the code was still GPL licensed, with various components updated/replaced with suitably licensed versions)
core-nes        = FCEUltra port (FCEU-Next fork)
core-snes       = SNES9x  port (SNES9x-Next fork)
engine          = defines for the common interface required by plug-ins to the RetroN 5 core

The source code for each of the projects residing in the folders listed above (with the exception of the
"engine" folder) is copyright the respective authors, who are identified in the corresponding source and
license files.

Build instructions
------------------

With Android NDK correctly installed, change to this folder in a shell, then: ndk-build NDK_PROJECT_PATH=./
