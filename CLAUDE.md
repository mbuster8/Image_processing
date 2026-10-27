# CLAUDE.md — Image Processing visual debugger

A 2013 thesis project revived in 2026: the `visual_debug` library sends a `cv::Mat` over ZeroMQ to a
separate Qt 6 viewer with zoom, history and pixel readout. Read the linked documents rather than
re-deriving them:

- [`docs/architecture.md`](docs/architecture.md): components, dependency rule, threads, transport, extension points
- [`docs/wire-format.md`](docs/wire-format.md): the message format (authoritative)
- [`docs/build-guide.md`](docs/build-guide.md): tools, presets, targets, tests, Qt Creator, troubleshooting
- [`docs/user-guide.md`](docs/user-guide.md): running the programs, `SendResult` meanings, worked examples
- [`docs/ADR/`](docs/ADR/): decision records

## Working rules

- **Never commit, push or merge** unless the owner asks in that request. Leave changes in the working tree.
- **One simple command per Bash call**: no `&&`, `;` or pipes; absolute paths instead of `cd`.
- Test-first for library code: write the failing GoogleTest, watch it fail, then implement.
- `docs/superpowers/` (specs and plans) is git-ignored; nothing committed may link into it.

## Commands

```sh
cmake --preset debug                  # also: release, asan
cmake --build --preset debug
ctest --preset debug                  # all tests; -R <regex> to filter
ctest --preset asan                   # sanitizers; undefined behaviour aborts the test
cmake --preset debug -B build/no-qt -DIMAGE_PROCESSING_BUILD_QT_APPS=OFF   # proves the libraries need no Qt
```

Binaries land in `build/<preset>/`: `apps/image_processing/image_processing.app`, `apps/client/client.app`,
`examples/pipeline_demo/pipeline_demo`, `examples/minimal_sender/minimal_sender`.

## Layout

`libs/visual_debug` (transport, no Qt) · `libs/viewer_core` (viewer logic, no Qt) · `apps/image_processing`
(viewer) · `apps/client` · `apps/qt_common` · `examples/{pipeline_demo,minimal_sender}` ·
`tests/{visual_debug,viewer_core,image_processing,examples}` · `cmake/` (warnings, sanitizers).

## Rules that keep it working

- **The libraries stay Qt-free.** No Qt include or link in `libs/`. Automatic moc/uic/rcc is enabled in
  `apps/CMakeLists.txt` only. The no-Qt configure above must keep building.
- **Threading:** `ImageSink::onImageReceived` runs on the server's worker thread. `QtImageSink` hands images
  to the UI thread by queued signal; the UI must call `markHandled()` for every image it takes, or the sink
  fills its eight slots and the viewer reports busy. `viewer_core` is UI-thread only and has no locks.
- **Wire-format changes:** a new optional header field keeps `version` 2; changing or removing a field
  bumps it. Update `docs/wire-format.md` and `docs/user-guide.md` in the same change.
- **Security:** no exception ever leaves `visual_debug` (every send returns a `SendResult`); reply reasons
  are fixed ASCII and never echo request bytes; the header is parsed without exceptions and never copied
  or dumped (a deeply nested header would overflow the worker's stack); the server binds loopback only
  unless `allowRemote` is set.
- **Portability** (Windows comes next): no POSIX- or GNU-only APIs; paths through `std::filesystem` or
  Qt; production code reads environment variables only through `visual_debug::defaultEndpoint()`; non-ASCII source is UTF-8
  (MSVC gets `/utf-8`); OpenCV API must also exist in 4.x (OpenCV 5 moved some headers: guard them with
  `__has_include`).
- **Warnings:** zero from project code under `-Wall -Wextra -Wpedantic`.

## Conventions

Files `snake_case.h/.cpp`; classes `PascalCase`; functions `camelCase`; members `m_`; constants
`kPascalCase`; namespaces lower case; tests `<unit>_test.cpp`. British English in identifiers, comments, UI
text and docs (`normalise`, `colour`); third-party names keep theirs (`cv::COLOR_BGR2RGB`). New-style Qt
`connect`. No `using namespace` in headers. `.clang-format`: LLVM base, 4 spaces, 110 columns. Comments
explain why.

## Former Slovak names

For reading old commits: `klient` = client · `Obrazky.qrc` = `resources.qrc` · `/zakladne` = `/icons` ·
`TYP_UDAJOV` = the header's `type` · `nazov` = `name` · `sirka`/`vyska` = `cols`/`rows` · `lbl_Okno` =
`ImageView` · `lbl_obrazok` = `previewLabel` · `menuNazov` = `menuFile` · `môj obrázok č.` = `<file name> #<n>`.
`my_main_qlabel` became `ImageView` + `viewer_core::Viewport`; `My_label` became `ThumbnailPanel`;
`mythread` became `visual_debug::Server` + `QtImageSink`.
