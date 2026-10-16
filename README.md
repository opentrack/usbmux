# opentrack-usbmux

A CMake build of [libusbmuxd], [libimobiledevice-glue] and [libplist], for
opentrack's SmoothTrack tracker. It produces a single library,
`opentrack-usbmux`, that talks to an iOS device over USB through usbmuxd
(Apple Mobile Device Service on Windows and macOS, the `usbmuxd` daemon on
Linux).

The three libraries are git submodules, checked out at upstream release tags
and not modified. Upstream builds them with autotools only, so on Windows they
need MinGW, and MinGW-built archives do not link reliably into opentrack's MSVC
build. This repository replaces autotools with a `CMakeLists.txt` that works
with MSVC, MinGW, GCC and Clang.

| Submodule               | Version |
|-------------------------|---------|
| `libplist`              | 2.8.0   |
| `libimobiledevice-glue` | 1.3.3   |
| `libusbmuxd`            | 2.1.1   |

## Building

```sh
git clone --recurse-submodules https://github.com/opentrack/usbmux.git
cmake -S usbmux -B build
cmake --build build
ctest --test-dir build
```

The test links against the library, checks it initialised, and asks usbmuxd
for a device list. It passes without a device or a running usbmuxd.

## Using it from another CMake project

```cmake
add_subdirectory(opentrack-usbmux)
target_link_libraries(my-target PRIVATE opentrack-usbmux)
```

```c
#include <usbmuxd.h>
```

Options:

- `OPENTRACK_USBMUX_SHARED` (default `ON`): build a shared library,
  `opentrack-usbmux.dll`, `libopentrack-usbmux.so` or
  `libopentrack-usbmux.dylib`. It exports only the `usbmuxd_*` and
  `libusbmuxd_*` API; libplist and libimobiledevice-glue stay internal. With
  `OFF`, the object files are linked straight into the consumer.
- `OPENTRACK_USBMUX_INSTALL` (default: on when built as the top-level project):
  install the library and headers.
- `OPENTRACK_USBMUX_TESTS` (default: on when built as the top-level project):
  build the link test.

`OFF` does not build a static archive. libimobiledevice-glue calls
`WSAStartup()` from a static initialiser in an object file that nothing else
references, so a linker pulling objects out of an archive would drop it and
every socket call on Windows would fail. Handing the consumer all the object
files avoids that.

## Updating a submodule

```sh
git -C libusbmuxd fetch --tags
git -C libusbmuxd checkout <tag>
```

Then update the matching `*_VERSION` variable at the top of `CMakeLists.txt`
and the table above, and check whether the new release adds source files or
`configure.ac` checks that `CMakeLists.txt` and `cmake/config.h.in` need to
pick up.

## License

libusbmuxd, libimobiledevice-glue and libplist are licensed under the GNU
Lesser General Public License, version 2.1 or later; see their `COPYING`
files. The build files in this repository are under the same license; see
[COPYING](COPYING). Distribute the shared library build alongside opentrack so
that users can replace it.

[libusbmuxd]: https://github.com/libimobiledevice/libusbmuxd
[libimobiledevice-glue]: https://github.com/libimobiledevice/libimobiledevice-glue
[libplist]: https://github.com/libimobiledevice/libplist
