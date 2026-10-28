# ADR-001: Technology stack for the 2026 revival

**Date:** 2026-10-26
**Status:** Accepted

## Context

This repository is a 2013 bachelor's thesis project: a small library (`visual_debug`) that an OpenCV
application calls to send a `cv::Mat` to a separate viewer process, which shows it with zoom and per-pixel
inspection. It was built with Qt 4.8.1 and the OpenCV 2.4 API on Ubuntu, using qmake, and ZeroMQ for the
inter-process channel. None of that toolchain builds today.

The revival has to:

- build and run on macOS (Apple silicon) now, and on Windows and Linux later, with dependencies a
  developer can install from a package manager in a few commands;
- stay a good codebase to learn image processing on, and be presentable to others;
- keep the thesis's central promise: a host application gains a debugging channel with **one call** and
  **minimal dependencies**.

Options considered, by concern:

| Concern | Options | Evaluation |
|---|---|---|
| Language | C++20 | OpenCV's native language; where its documentation, books and jobs are. Modern standard library removes most 2013-era pain. Supported by Qt 6, Apple clang, GCC 13+, MSVC 2022. |
| | Rust | Memory-safe and modern, but OpenCV's Rust bindings are generated and lag the C++ API; a third toolchain to learn alongside OpenCV. |
| UI | Qt 6 Widgets | Direct port of the original. Standard toolkit for desktop imaging tools (3D Slicer, ParaView, CloudCompare, QGIS, napari). Free under LGPLv3; runs on macOS, Windows, Linux; one language and one debugger end to end. |
| | Second viewer in Dear ImGui + GLFW | Lean immediate-mode UI common in developer tools. Planned as "Phase B" to prove the format is UI-agnostic; that proof is now provided more cheaply (see Decision 3), so it adds a second codebase for no new capability. |
| | Electron + TypeScript | Modern web UI and plays to TypeScript skills, but over 100 MB per app, a second implementation of all viewer logic in another language, image bytes crossing a native–JavaScript boundary, and npm install scripts that conflict with the `--ignore-scripts` supply-chain rule. |
| Inter-process transport | ZeroMQ (libzmq + cppzmq) | Mature (since 2007), MPL-2.0, one package on Homebrew, vcpkg and apt. Gives message framing, automatic reconnection and multipart messages. Already the thesis's choice. |
| | Qt `QLocalSocket` / `QTcpSocket` | Native to the viewer, but would force Qt into the sender library and therefore into every host application. |
| | Raw sockets or Asio | No messaging dependency, but framing, reconnection and Windows/POSIX differences become our code. |
| | gRPC | Heavy dependency tree and code generation; difficult to build on Windows; far beyond what one message type needs. |
| | Shared memory | Fastest for very large frames, but complex synchronisation and same-machine only. A possible later optimisation, not a starting point. |
| Message format | All binary (fixed header + pixels) | Smallest, no dependency, but every new field is a breaking layout change. |
| | All JSON (pixels base64-encoded) | Readable, but a third larger and a decode per image. |
| | JSON header frame + binary pixel frame | Extensible and readable where it matters, compact where it matters. Same split Jupyter kernels use over ZeroMQ. |
| JSON library | nlohmann/json | Header-only, MIT, actively maintained, readable modern API. Checked 2026-10-26: Homebrew 3.12.0 is 1.5 MB with no compiled library; vcpkg 3.12.0 supported on all triplets with no runtime dependencies. |
| | RapidJSON | Faster, but last release 1.1.0 is from 2016; package managers patch or snapshot it to build with modern compilers. Speed is irrelevant for headers under 1 KB. |
| Unit tests | GoogleTest + CTest | Most widely used C++ framework; shown in Qt Creator's test pane; on Homebrew, vcpkg and apt. |
| Build | CMake + Ninja + `CMakePresets.json` | Opens in Qt Creator, Visual Studio, VS Code and CLion; replaces three hand-maintained qmake projects with hard-coded build paths. |

## Decision

1. **C++20** throughout.
2. **Qt 6 Widgets for every application**: the viewer (`apps/viewer`) and the example sender
   (`apps/example_sender`). **Qt Creator** is the reference IDE (free, LLDB debugging, UI designer, test
   runner).
   No second viewer: the Dear ImGui and Electron options are rejected.
3. **The two libraries stay Qt-free.** `visual_debug` (transport) depends only on OpenCV core, ZeroMQ and
   nlohmann/json; `viewer_core` (display conversion, history, pixel readout, zoom arithmetic) depends only
   on OpenCV. CMake does not link Qt into either, their unit tests run without Qt, and `pipeline_demo`
   sends images from a program with no Qt at all. This is what proves the channel is independent of any UI.
4. **ZeroMQ** is the transport, using REQ/REP so each image is acknowledged, hardened with a reply timeout
   (scaled with image size), a retry back-off and socket recreation after a lost reply ("Lazy Pirate"),
   so a send waits at most for its timeout, and while no viewer is running only the first send after
   each back-off waits at all; the others fail at once. The viewer listens on loopback only unless started with `--allow-remote`; the channel is
   neither encrypted nor authenticated, so remote use is for trusted networks only.
5. **Wire format version 2:** one ZeroMQ multipart message of exactly two frames: a **JSON header**
   (`protocol`, `version`, `type`, `name`, `rows`, `cols`, `depth`, `channels`) and the **raw pixel
   bytes**. Unknown header fields are ignored; any breaking change bumps `version`. The authoritative
   field table and validation rules are in `docs/wire-format.md`.
6. **nlohmann/json** for the header, on the owner's condition that it stays light and easy to install on
   every platform (verified above). If that ever stops being true, the header can be read and written
   with a small hand-written parser instead, because it has only eight flat fields.
7. **GoogleTest + CTest** for unit tests, written test-first, covering all UI-free code and the thin Qt
   helpers.
8. **CMake + Ninja + presets** as the only build system. Dependencies come from the platform's package
   manager: Homebrew on macOS (now), vcpkg on Windows and apt on Linux (later).

## Consequences

- Positive: one language, one IDE and one debugger from host application to viewer.
- Positive: a host application takes on only OpenCV, ZeroMQ and a header-only JSON library, never Qt.
- Positive: the JSON header lets the format grow (timestamps, source location, overlays) without breaking
  existing viewers.
- Positive: every dependency is a single package-manager install on macOS, Windows and Linux.
- Negative: the viewer looks like a traditional desktop application rather than a web UI. Qt Quick (QML)
  is the path to a more modern look if that ever matters.
- Negative: Qt is large to install, especially on Windows (Qt Online Installer with a free account, or
  vcpkg building it from source).
- Negative: ZeroMQ copies each image into a message; very large or very fast streams may later justify
  shared memory.
- Operational: Windows is unverified until a bring-up session on the owner's Windows machine adds
  `vcpkg.json`, Windows presets and setup scripts. The code follows portability rules (listed in
  `CLAUDE.md`) so that session should need packaging, not rewriting.

## Links

- `docs/wire-format.md`: the message format, validation rules and replies.
- `docs/build-guide.md`: installing the dependencies, building, testing and debugging.
- `docs/user-guide.md`: running the viewer, the example sender and the examples.
- `docs/architecture.md`: components, the dependency rule, threads and extension points.
