# macOS Wine Bridge

This binary allows other binaries running under Wine to communicate to Discord running on the host machine. This binary is designed to work only on 64 bit macOS host systems.

## Installation
Note: These instructions assume the usage of a [Wineskin](http://wineskin.urgesoftware.com/tiki-index.php) wrapper.
1. Download the `bridge.exe` binary from this repository
2. Move the binary into the Wineskin `drive_c` folder
3. Invoke the binary before the main application with a batch script:
    ```bat
    start C:\<path>\bridge.exe
    start C:\<path>\<application>.exe
    ```
4. Change the Wineskin executable path to the batch script:
    ```
    "C:\<path>\execute.bat" 
    ```

## Compiling (macOS)
To compile this project, you will need to install `x86_64-w64-mingw32-gcc` (The MinGW GCC compiler).  
You can install this with [MacPorts](https://ports.macports.org/port/x86_64-w64-mingw32-gcc/):  
```bat
sudo port install x86_64-w64-mingw32-gcc
```

Then, use the Makefile to compile:
```bat
make
```
You can also manually compile using the command found in the Makefile:
```bat
x86_64-w64-mingw32-gcc -masm=intel -mwindows -o bridge.exe -O2 -Wall main.c
```

This process will give you a `bridge.exe` that you can call inside the Wine command line.  
XIV for Mac users can replace the embedded bridge found at `/Applications/XIV for Mac.app/Contents/Resources/discord_bridge.exe` with this one.

## Technical information

This fork is based on three other projects:
* https://github.com/0e4ef622/wine-discord-ipc-bridge
* https://github.com/koukuno/wine-discord-ipc-bridge
* https://github.com/goeo-/discord-rpc/blob/xnu-under-wine/src/connection_win.cpp

For general information on how the bridge operates see the mentioned projects. The rest of this section will discuss the specifics for bridging on macOS.

The `discord-rpc` project created by `goeo-` is unfortunately outdated for two main reasons. The first is that Discord has deprecated the `discord-rpc.dll` method of performing communication in favour of an integrated game sdk file. This makes it difficult to inject code that communicates with the host system rather than with Windows.

However, much of the code responsible for communicating with Linux and macOS is still valid (see branches `linux-under-wine` and `xnu-under-wine`). In fact, 0e4ef622's implementation directly splices the system call invocations for Linux into the bridging binary. For reference, Wine does not intercept system calls directly but instead replaces Windows API calls. This means that PE binaries can make system calls directly to the host system and it is this method that allows bridging to become possible.

The system calls used by the `xnu-under-wine` however cannot be used on modern versions of macOS as 32 bit code execution has been disabled. This includes 32 bit system calls which the handlers have not been specifically removed but remain disabled. These handlers can be enabled again by setting the boot argument: `nvram boot-args="no32exec=0"` but this option is impractical for most users as it requires booting into Recovery Mode. This branch uses 32 bit system calls which means that they cause a segmentation fault upon execution.

Fortunately it does not seem immediately difficult to change these into 64 bit system calls. Add the value `0x2000000` to the call vector register (`eax`) and then change the invocation code from `int 0x80` to `syscall` (https://filippo.io/making-system-calls-from-assembly-in-mac-os-x/). The new system call method also requires the arguments to be placed in registers according to the System V ABI. After these modifications are made, the executable can be recompiled.

However, after doing so the binary crashes on execution. Upon further inspection (by running the binary through `wine64` directly; note that Gcenx provides a [brew tap](https://github.com/Gcenx/homebrew-wine) that has a formula for running 32 bit applications on Catalina) it is discovered that it crashes on the `syscall` instruction. This is because the compiler used for the original bridges generates a 32 bit executable that causes Wine to request the code be located in a 32 bit segment. This prevents the `syscall` instruction from being executed under restrictions by the Intel processor and hence issues a fault.

Thankfully the fix is simple. Switch the compiler from `i686-w64-mingw32-gcc` to `x86_64-w64-mingw32-gcc` and the toolchain will generate a 64 bit executable. Note that the registers in the `syscall` instruction have been changed to their long mode form. Finally, note that the calling conventions for the system call wrappers have been changed using an `attribute`. These conventions can be found in GCC's manual and are compatible with mingw's.
