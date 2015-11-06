RetroFreak open source component source release
---------------------------------------------

Current as of RetroFreak application v1.1

core-gameboy    = VBA-M port
core-gba        = VBA-M port (VBA-next fork)
core-genesis    = Genesis Plus GX port
core-nes        = FCEUltra port (FCEU-Next fork)
core-snes       = SNES9x port (SNES9x-Next fork)
core-snes2      = SNES9x port (based on v1.53)
core-pce        = Mednafen port
engine          = defines for the common interface required by plug-ins to the RetroFreak core

The source code for each of the projects residing in the folders listed above (with the exception of the
"engine" folder) is copyright the respective authors, who are identified in the corresponding source and
license files.

Build instructions
------------------

With Android NDK correctly installed, change to this folder in a shell, then: ndk-build NDK_PROJECT_PATH=./
