# SRWWL
A simple library for writing a software renderer using wayland windowing. A fork based on code at https://github.com/willth7/wayland-client-example.

## Compiling:
Must be compiled with GCC. **NO OTHER COMPILERS ARE SUPPORTED**. This is because of nested functions in qoidecoder.c.
Must be compiled with valid copies of xdg-decoration-unstable-v1-protocol.c and xdg-shell-protocol.c as well as their respective headers.
See https://www.youtube.com/watch?v=iIVIu7YRdY0&t=2137s for instructions on how to generate these files.

## Usage:
Honestly everything is fairly obvious from libSRWWL.h but just note that the buffer must be exactly the correct size and contain 4 uint8_t's per pixel in BGRA row-major format.
Compiled binaries are available in releases.

## Dependencies:
Must have wayland-devel tools installed and link with wayland-client.
 -   Ubuntu/Debian: `sudo apt install libwayland-dev wayland-protocols`
 -   Fedora: `sudo dnf install wayland-devel wayland-protocols`

## Licence:
It should be obvious but Apache 2.0
