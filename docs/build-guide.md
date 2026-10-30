# Build guide

How to install the tools, build every part of the project, run the tests and work on it in Qt Creator.
For running the programs, see the [user guide](user-guide.md).

Verified on macOS 27.0.1 on Apple silicon (Mac mini), Apple clang 21, October 2026. Continuous integration
also builds and tests every push on Ubuntu 24.04; Windows is planned for a separate session.

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

Then run what you built (the [user guide](user-guide.md) explains each program), with `build/release` for
the release build:

```sh
open build/debug/apps/viewer/visual_debug_viewer.app                   # the Visual Debug Viewer
build/debug/examples/pipeline_demo/pipeline_demo                       # sends it ten images
open build/debug/apps/example_sender/visual_debug_example_sender.app   # the Visual Debug Example Sender
```

The build is warning-free under `-Wall -Wextra -Wpedantic`. Warnings are not errors, so a newer compiler
or library cannot break the build, but a new warning from project code should be fixed.

### What gets built

| Target | What it is | Where it lands (in `build/<preset>/`) |
|---|---|---|
| `visual_debug` | The sender library: wire format, `Client`, `Server`, `sendImage` | `libs/visual_debug/libvisual_debug.a` |
| `viewer_core` | The viewer's UI-free logic: display conversion, history, pixel readout, zoom | `libs/viewer_core/libviewer_core.a` |
| `qt_common` | Qt helpers shared by both apps | `apps/qt_common/libqt_common.a` |
| `viewer_ui` | The viewer's windows, as a library its tests link | `apps/viewer/libviewer_ui.a` |
| `visual_debug_viewer` | **The Visual Debug Viewer** | `apps/viewer/visual_debug_viewer.app` |
| `example_sender_ui` | The example sender's window, as a library its tests link | `apps/example_sender/libexample_sender_ui.a` |
| `visual_debug_example_sender` | The Visual Debug Example Sender, a Qt app that sends image files | `apps/example_sender/visual_debug_example_sender.app` |
| `pipeline` | The demo's OpenCV stages | `examples/pipeline_demo/libpipeline.a` |
| `pipeline_demo` | Command-line demo: an OpenCV pipeline that sends every stage | `examples/pipeline_demo/pipeline_demo` |
| `minimal_sender` | The smallest use of `sendImage` | `examples/minimal_sender/minimal_sender` |
| `visual_debug_tests`, `viewer_core_tests`, `qt_common_tests`, `viewer_ui_tests`, `example_sender_ui_tests`, `pipeline_tests` | GoogleTest executables | `tests/` |

To build one target: `cmake --build --preset debug --target pipeline_demo`.

The apps' file names are the same on every platform. On macOS, Finder lists the file name, while the menu
bar, ⌘-Tab and the Dock show the bundle's name, such as **Visual Debug Viewer**. Each bundle's icon is
made at build time from the PNG in `resources/icons/`, with the `sips` and `iconutil` tools that come with
macOS.

### Options

| Option | Default | Effect |
|---|---|---|
| `ID_VISION_LAB_BUILD_QT_APPS` | `ON` | The viewer, the example sender, `qt_common` and the Qt tests |
| `ID_VISION_LAB_BUILD_EXAMPLES` | `ON` | `pipeline_demo`, `minimal_sender` and their tests |
| `ID_VISION_LAB_BUILD_TESTS` | `ON` | All GoogleTest executables |
| `ID_VISION_LAB_SANITIZERS` | `OFF` | AddressSanitizer and UndefinedBehaviorSanitizer (the `asan` preset turns it on) |

### Building without Qt

The two libraries never use Qt. To prove it, or to build on a machine without Qt, turn the Qt apps off
and build into a separate folder:

```sh
cmake --preset debug -B build/no-qt -DID_VISION_LAB_BUILD_QT_APPS=OFF
cmake --build build/no-qt
ctest --test-dir build/no-qt               # the 170 tests that need no Qt
```

CMake then does not look for Qt at all; both libraries, both examples and their tests still build.

## Running the tests

```sh
ctest --preset debug                       # everything (397 tests, about 50 seconds)
ctest --preset debug -R WireFormat         # only tests whose name matches a regular expression
ctest --preset debug -R "Client|Server"
ctest --preset asan                        # the same tests under the sanitizers (about a minute)
ctest --preset debug -L whole              # only the whole-program runs
ctest --preset debug -LE whole             # everything except them
```

Every GoogleTest case is its own CTest test, so `-R` takes test names such as
`ClientServer.RecoversAfterServerStarts`. A failing test prints its output. Each test has a 60-second
timeout, so a hung socket fails instead of stalling the run.

Each test program also runs once as a whole, as `<program> (whole program)` with the label `whole`. That is
how Qt Creator and other IDEs run it, and state one test leaves behind (a changed global setting, say) breaks
the next test only when the two share a process.

The Qt tests create an offscreen `QApplication`, so they open no windows and also run without a display.

In the `asan` build, undefined behaviour aborts the program like a memory error, so it fails the test
rather than printing a report into a passing run.

## Checking changes before a push

`scripts/check.sh` runs what continuous integration runs, on this machine:

```sh
scripts/check.sh            # clang-format, debug and asan here, and the Ubuntu job in Docker
scripts/check.sh --quick    # clang-format and debug only (about a minute)
scripts/check.sh --linux    # only the Ubuntu job in Docker
```

It prints one line per step and stops at the first failure, printing the end of that step's output; the full
output of every step is in `build/check-logs/`. clang-format must be version 22.1.8: the script finds Qt
Creator's bundled copy, a `clang-format` on `PATH`, or the one `CLANG_FORMAT` names.

The Ubuntu job needs Docker Desktop running. It builds an image from `docker/ubuntu-24.04.Dockerfile` with the
CI's packages (the first time takes a few minutes), then builds and tests a copy of the working tree in a
throwaway container, with the C locale and Qt offscreen as on GitHub's runners. Your own `build/` folders are
never touched. To look around inside the container, for example to run one failing test:

```sh
docker run --rm -it --volume "$PWD":/src:ro --entrypoint bash id-vision-lab-ubuntu
```

To run the full check before every `git push`, turn on the repository's hook once per clone:

```sh
git config core.hooksPath scripts/git-hooks   # off again: git config --unset core.hooksPath
git push --no-verify                          # skips the hook for one push
```

In Claude Code, two skills in `.claude/skills/` use the script: **`local-ci`** picks the mode for the change,
runs it, reads the logs of a failing step and reproduces the failing test; **`pr-ready`** runs the full check,
reviews the branch's changes against its base and drafts the pull request's title and description with the
measured results. Neither pushes, commits or opens a pull request unless asked.

## Working in Qt Creator

These steps are not covered by the automated tests: menu names follow Qt Creator 20.0.2 and are checked
by hand. If a menu has moved in your version, the presets above still work from the command line.

1. Start Qt Creator and choose **File → Open File or Project…**, then select the top-level
   `CMakeLists.txt`.
2. On the **Configure Project** page, tick the presets you want (for example **Debug**) and click
   **Configure Project**. Qt Creator reads `CMakePresets.json`, so the build folders are the same
   `build/<preset>` ones as on the command line.
3. Build with **Build → Build All Projects** (⌘B).
4. Run: pick `visual_debug_viewer` in the run target selector (the computer icon at the bottom left) and
   press **Run** (⌘R). Pick `pipeline_demo` and run it to send it some images.
5. Tests: open the **Test Results** pane (**View → Output → Test Results**), then **Tools → Tests → Run
   All Tests**. Qt Creator runs each GoogleTest program itself and lists the results by test suite. Unlike
   `ctest`, it runs all the tests of one program in a single process, so a test that leaves global state
   behind (or crashes) affects the tests after it, and the pane reports the program as crashed. If
   `viewer_ui_tests` crashes in `ThumbnailPanel`, see the `ImageIO` entry under Troubleshooting.
6. Debugging: click in the margin of `apps/viewer/image_view.cpp` next to the first line of
   `ImageView::wheelEvent` to set a breakpoint, start with **Debug → Start Debugging** (⌘Y), and scroll
   the mouse wheel over an image in the viewer. Qt Creator stops at the breakpoint with LLDB.

## Troubleshooting

**Qt Creator cannot find Ninja, Qt or OpenCV when started from the Dock.** Apps started from the Dock or
Finder get launchd's minimal `PATH`, which does not include `/opt/homebrew/bin`. The presets add it to
`PATH` themselves, so this only happens when configuring without a preset. Use a preset, or start Qt
Creator from a terminal.

**A test or the viewer crashes in `ImageIO` (`SIGBUS`) only when started from Qt Creator.** Qt Creator's
run setting **Add build library search path to DYLD_LIBRARY_PATH and DYLD_FRAMEWORK_PATH** puts
`/opt/homebrew/lib`, where Homebrew's OpenCV, ZeroMQ and Qt live, on `DYLD_LIBRARY_PATH`. macOS then loads
Homebrew's `libpng` in place of its own, and drawing a colour emoji (an image name such as `ž😀`, as in
`ThumbnailPanel.LongNameKeepsCellSize`) crashes. In **Projects → Run Settings**, pick each run configuration
in turn (the viewer, the example sender and each test program) and untick that setting, just below **Run
in terminal**: the executables find their libraries through their rpath and need no search path. A run
configuration Qt Creator makes for a new or renamed target starts with the setting ticked again. `ctest` and
Finder never set it.

**`Could NOT find WrapVulkanHeaders` during configure.** Qt looks for optional Vulkan support. Harmless;
nothing in this project uses Vulkan.

**`[ WARN:0@…] global loadsave.cpp … findDecoder imread_(…): can't open/read file`.** OpenCV's own log
line when a file cannot be read. The program reports the failure itself as well. Set
`OPENCV_LOG_LEVEL=ERROR` to hide these lines.

**AddressSanitizer reports `container-overflow` inside GoogleTest, OpenCV or Qt.** These Homebrew
libraries are not built with AddressSanitizer, which makes this check report false positives.
`cmake/sanitizer_defaults.cpp` turns that one check off for every executable in the `asan` build; leave
it in place.

**`Operation not permitted` while building under `~/Documents`.** Antivirus folder protection, such as
Bitdefender's Safe Files, which guards `~/Documents`, blocks a tool it does not trust from writing there. The
error names the file, not the antivirus. Add the tool to the antivirus's trusted applications only if its
path is in Homebrew (`/opt/homebrew`), the Xcode Command Line Tools (`/Library/Developer/CommandLineTools`)
or this project's `build` folder; otherwise, find out what the tool is first.

**`ld: symbol(s) not found` after adding a source file.** Add the file to the target's `add_library` or
`add_executable` list in its `CMakeLists.txt`; CMake does not pick up new files by itself.

## Using the library in your own project

See [Link the library from your own CMake project](user-guide.md#6-link-the-library-from-your-own-cmake-project)
in the user guide.

## Linux

Continuous integration builds and tests every push on Ubuntu 24.04 with the distribution's packages, which
give OpenCV 4.6 and Qt 6.4, the oldest versions supported. The list lives in
[`scripts/install-ubuntu-deps.sh`](../scripts/install-ubuntu-deps.sh), which CI and the Docker image both run;
on an Ubuntu 24.04 machine:

```sh
sudo scripts/install-ubuntu-deps.sh
```

Then build with the presets as above. With Qt older than 6.8 the **View → Appearance** menu is absent. The
presets add `/opt/homebrew/bin` to `PATH`, which is harmless on Linux because a missing folder on `PATH` is
ignored. The apps are then plain executables rather than bundles, under the same names:
`build/debug/apps/viewer/visual_debug_viewer` and
`build/debug/apps/example_sender/visual_debug_example_sender`. Their windows show their icons, but there is
no desktop launcher for them yet.

## Windows

Planned for a separate session on a Windows machine: a `vcpkg.json` manifest, Windows presets and a setup
script. The code already follows the portability rules in [`CLAUDE.md`](../CLAUDE.md), so that session
should need packaging, not code changes.
