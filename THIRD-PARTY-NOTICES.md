# Third-party notices and credits

wheelpad builds on the PS4 homebrew toolchain. The compiled `wheelpad.prx`
includes code from the projects below; their license terms travel with this
file.

## GoldHEN Plugin SDK — MIT

Source: https://github.com/GoldHEN/GoldHEN_Plugins_SDK

The build compiles the SDK's `source/Detour.c` and `source/GoldHEN.c` and
uses its headers (`Common.h`, `Detour.h`, `HDE64.h`, `Syscall.h`,
`Table64.h`, `Utilities.h`, `Patcher.h`). The x64 detour that hooks
`scePadReadState` is SDK code.

The SDK carries these credits in its sources: OSM
(https://github.com/OSM-Made), jocover (https://github.com/jocover),
bucanero (https://github.com/bucanero), OpenOrbis Team
(https://github.com/OpenOrbis), SiSTRo (https://github.com/SiSTR0).

License:

    MIT License

    Copyright (c) 2022 GoldHEN

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

## OpenOrbis PS4 Toolchain

Source: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain

Provides the Orbis headers (`orbis/Pad.h`, `orbis/Usbd.h`,
`orbis/libkernel.h` and the `_types` headers) and the link stubs the build
uses. Build-time dependency; the plugin's NID imports resolve through it.

## OSM-Made / PS4-Notify — reference for the load notification

Source: https://github.com/OSM-Made/PS4-Notify

The load banner fills `OrbisNotificationRequest` and calls
`sceKernelSendNotificationRequest` following the recipe this project
published (also used by the GoldHEN Plugins Repository).

## crucifix86 / xbox_controller_plugin — reference design

Source: https://github.com/crucifix86/xbox_controller_plugin

Read as the working reference for `sceUsbd` device discovery and pad
injection from a GoldHEN plugin. No code copied.
