# Build guide

How to install the tools, build every part of the project, run the tests and work on it in Qt Creator.
For running the programs, see the [user guide](user-guide.md).

Verified on macOS 27.0.1 on Apple silicon (Mac mini), Apple clang 21, October 2026. Linux steps are written
but not yet verified; Windows is planned for a separate session.

## Prerequisites (macOS)

1. The Xcode Command Line Tools (compiler, `git`, LLDB):

   ```sh
   xcode-select --install
   ```

2. [Homebrew](https://brew.sh), then the libraries and build tools:

   ```sh
   brew install cmake ninja pkgconf qt opencv zeromq cppzmq nlohmann-json googletest
   ```

3. Qt Creator, the reference IDE (optional; any CMake-aware editor works):

   ```sh
   brew install --cask qt-creator
   ```

Versions this was verified with:

| Package | Version |
|---|---|
| `cmake` | 4.4.4 (3.25 or newer is required) |
| `ninja` | 1.13.2 |
| `qt` | 6.12.0 (6.4 or newer is required) |
| `opencv` | 5.0.0 (the code also uses only API that exists in OpenCV 4) |
| `zeromq` / `cppzmq` | 4.3.5 / 4.11.0 |
| `nlohmann-json` | 3.12.0 (3.11 or newer is required) |
| `googletest` | 1.18.0 |
| Qt Creator | 20.0.2 |

## Building

The project uses CMake presets. Each preset builds into its own folder under `build/`:

| Preset | Build type | Folder |
|---|---|---|
| `debug` | Debug | `build/debug` |
| `release` | Release | `build/release` |
| `asan` | Debug with AddressSanitizer and UndefinedBehaviorSanitizer | `build/asan` |

From the repository root:

```sh
cmake --preset debug           # configure
cmake --build --preset debug   # build everything
ctest --preset debug           # run all tests
```

Replace `debug` with `release` or `asan` for the other builds. To rebuild from scratch, add
`--clean-first` to the build command, or delete the preset's folder under `build/`.

The build is warning-free under `-Wall -Wextra -Wpedantic`. Warnings are not errors, so a newer compiler
or library cannot break the build, but a new warning from project code should be fixed.

### What gets built

| Target | What it is | Where it lands (in `build/<preset>/`) |
|---|---|---|
| `visual_debug` | The sender library: wire format, `Client`, `Server`, `sendImage` | `libs/visual_debug/libvisual_debug.a` |
| `viewer_core` | The viewer's UI-free logic: display conversion, history, pixel readout, zoom | `libs/viewer_core/libviewer_core.a` |
| `qt_common` | Qt helpers shared by both apps | `apps/qt_common/libqt_common.a` |
| `image_processing_ui` | The viewer's windows, as a library the Qt tests link | `apps/image_processing/libimage_processing_ui.a` |
| `image_processing` | **The viewer** | `apps/image_processing/image_processing.app` |
| `client` | The Qt demo sender | `apps/client/client.app` |
| `pipeline` | The demo's OpenCV stages | `examples/pipeline_demo/libpipeline.a` |
| `pipeline_demo` | Command-line demo: an OpenCV pipeline that sends every stage | `examples/pipeline_demo/pipeline_demo` |
| `minimal_sender` | The smallest use of `sendImage` | `examples/minimal_sender/minimal_sender` |
| `visual_debug_tests`, `viewer_core_tests`, `qt_ui_tests`, `pipeline_tests` | GoogleTest executables | `tests/` |

To build one target: `cmake --build --preset debug --target pipeline_demo`.

### Options

| Option | Default | Effect |
|---|---|---|
| `IMAGE_PROCESSING_BUILD_QT_APPS` | `ON` | The viewer, the client, `qt_common` and the Qt tests |
| `IMAGE_PROCESSING_BUILD_EXAMPLES` | `ON` | `pipeline_demo`, `minimal_sender` and their tests |
| `IMAGE_PROCESSING_BUILD_TESTS` | `ON` | All GoogleTest executables |
| `IMAGE_PROCESSING_SANITIZERS` | `OFF` | AddressSanitizer and UndefinedBehaviorSanitizer (the `asan` preset turns it on) |

### Building without Qt

The two libraries never use Qt. To prove it, or to build on a machine without Qt, turn the Qt apps off
and build into a separate folder:

```sh
cmake --preset debug -B build/no-qt -DIMAGE_PROCESSING_BUILD_QT_APPS=OFF
cmake --build build/no-qt
```

CMake then does not look for Qt at all; both libraries, both examples and their tests still build.

## Running the tests

```sh
ctest --preset debug                       # everything (139 tests, about 6 seconds)
ctest --preset debug -R WireFormat         # only tests whose name matches a regular expression
ctest --preset debug -R "Client|Server"
ctest --preset asan                        # the same tests under the sanitizers
```

Every GoogleTest case is its own CTest test, so `-R` takes test names such as
`ClientServer.RecoversAfterServerStarts`. A failing test prints its output. Each test has a 60-second
timeout, so a hung socket fails instead of stalling the run.

The Qt tests create an offscreen `QApplication`, so they open no windows and also run without a display.

In the `asan` build, undefined behaviour aborts the program like a memory error, so it fails the test
rather than printing a report into a passing run.

## Working in Qt Creator

These steps are not covered by the automated tests: menu names follow Qt Creator 20.0.2 and are checked
by hand. If a menu has moved in your version, the presets above still work from the command line.

1. Start Qt Creator and choose **File → Open File or Project…**, then select the top-level
   `CMakeLists.txt`.
2. On the **Configure Project** page, tick the presets you want (for example **Debug**) and click
   **Configure Project**. Qt Creator reads `CMakePresets.json`, so the build folders are the same
   `build/<preset>` ones as on the command line.
3. Build with **Build → Build All Projects** (⌘B).
4. Run: pick `image_processing` in the run target selector (the computer icon at the bottom left) and
   press **Run** (⌘R). Pick `pipeline_demo` and run it to send it some images.
5. Tests: open the **Test Results** pane (**View → Output → Test Results**), then **Tools → Tests → Run
   All Tests**. Qt Creator runs each GoogleTest program itself and lists the results by test suite. Unlike
   `ctest`, it runs all the tests of one program in a single process, so a test that leaves global state
   behind (or crashes) affects the tests after it, and the pane reports the program as crashed.
6. Debugging: click in the margin of `apps/image_processing/image_view.cpp` next to the first line of
   `ImageView::wheelEvent` to set a breakpoint, start with **Debug → Start Debugging** (⌘Y), and scroll
   the mouse wheel over an image in the viewer. Qt Creator stops at the breakpoint with LLDB.

## Troubleshooting

**Qt Creator cannot find Ninja, Qt or OpenCV when started from the Dock.** Apps started from the Dock or
Finder get launchd's minimal `PATH`, which does not include `/opt/homebrew/bin`. The presets add it to
`PATH` themselves, so this only happens when configuring without a preset. Use a preset, or start Qt
Creator from a terminal.

**`Could NOT find WrapVulkanHeaders` during configure.** Qt looks for optional Vulkan support. Harmless;
nothing in this project uses Vulkan.

**`[ WARN:0@…] global loadsave.cpp … findDecoder imread_(…): can't open/read file`.** OpenCV's own log
line when a file cannot be read. The program reports the failure itself as well. Set
`OPENCV_LOG_LEVEL=ERROR` to hide these lines.

**AddressSanitizer reports `container-overflow` inside GoogleTest, OpenCV or Qt.** These Homebrew
libraries are not built with AddressSanitizer, which makes this check report false positives.
`cmake/sanitizer_defaults.cpp` turns that one check off for every executable in the `asan` build; leave
it in place.

**`ld: symbol(s) not found` after adding a source file.** Add the file to the target's `add_library` or
`add_executable` list in its `CMakeLists.txt`; CMake does not pick up new files by itself.

## Using the library in your own project

See [Link the library from your own CMake project](user-guide.md#6-link-the-library-from-your-own-cmake-project)
in the user guide.

## Linux (not yet verified)

On Ubuntu 24.04 or Debian 13 these packages should provide everything:

```sh
sudo apt install build-essential cmake ninja-build pkgconf qt6-base-dev libopencv-dev \
    libzmq3-dev cppzmq-dev nlohmann-json3-dev libgtest-dev
```

Then build with the presets as above. The presets add `/opt/homebrew/bin` to `PATH`, which is harmless on
Linux because a missing folder on `PATH` is ignored.

## Windows

Planned for a separate session on a Windows machine: a `vcpkg.json` manifest, Windows presets and a setup
script. The code already follows the portability rules in [`CLAUDE.md`](../CLAUDE.md), so that session
should need packaging, not code changes.
