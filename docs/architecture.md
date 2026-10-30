# Architecture

How the parts fit together, which way dependencies point, what runs on which thread, and where to extend
it. For the message format itself, see [`wire-format.md`](wire-format.md); for the reasons behind the
technology choices, see [ADR-001](ADR/ADR-001-technology-stack.md).

## The idea

An OpenCV program calls one function to show an image in a separate viewer process. The program takes on
only OpenCV, ZeroMQ and a header-only JSON library, never the viewer's UI toolkit. If no viewer is
running, the call returns quickly and the program carries on.

## Components

```mermaid
flowchart LR
    subgraph apps["Applications (Qt 6)"]
        viewer["viewer<br/>Visual Debug Viewer"]
        sender["example_sender<br/>Visual Debug Example Sender"]
        qtcommon["qt_common<br/>shared Qt helpers"]
    end
    subgraph examples["Examples (no Qt)"]
        pipeline["pipeline_demo"]
        minimal["minimal_sender"]
        live["live_sender"]
    end
    subgraph libs["Libraries (no Qt)"]
        vd["visual_debug<br/>wire format, Client, Server"]
        vc["viewer_core<br/>display, history, readout, zoom"]
    end
    viewer --> qtcommon
    sender --> qtcommon
    viewer --> vd
    viewer --> vc
    sender --> vd
    sender --> vc
    qtcommon --> vd
    qtcommon --> vc
    pipeline --> vd
    minimal --> vd
    live --> vd
    vd --> deps1["OpenCV core, ZeroMQ, nlohmann/json"]
    vc --> deps2["OpenCV core, imgproc"]
```

| Component | Responsibility |
|---|---|
| `libs/visual_debug` | Everything both ends of the channel share. `wire_format`: encodes an image into the two frames and decodes them back, validating everything and never throwing. `ImageSender`: the interface a host application depends on. `Client`: its ZeroMQ implementation. `sendImage()` and `sendLive()`: the one-line convenience calls, using one process-wide `Client`. `Server`: the viewer's receive loop on its own thread. `ImageSink`: the interface through which the server hands images to whoever displays them. `endpoint`: the default endpoint and the loopback check. |
| `libs/viewer_core` | The viewer's logic, free of any UI toolkit, so it is unit-tested without Qt. `toDisplayImage`: any supported image to 8-bit RGBA, with a display range and colour map. `DisplaySettings` and `DisplaySettingsStore`: those choices, by name, for the session. `ImageHistory`: the bounded list of received images. `LiveChannels`: per live channel, the frame to show, its counters and rates, and whether it is idle or frozen. `inspectPixel`: the readout text for one pixel of the original. `Viewport`: zoom and pan arithmetic. |
| `apps/qt_common` | The Qt code both apps need: `toQImage` (an RGBA `cv::Mat` to a `QImage`), choosing and reading an image file, `application_identity` (the apps' names and descriptions; `setApplicationIdentity`, which names what `QSettings` and the standard paths use), `about` (Help → About, which names the OpenCV and Qt versions the app runs with), `appearance` (the shared System, Light or Dark setting and its menu), `outline_icons` (the line icons, drawn in the palette's colours), `controls` (the shared control height, framed buttons and the regular font) and `shades` (the panel, hover, selection and divider greys, blended from the palette's window and text colours; `usePanelGrey`, which keeps a widget on the panel grey as the appearance changes; and `DividerLine`). |
| `apps/viewer` | The Visual Debug Viewer. `MainWindow` composes the parts; `ImageView` paints through a `Viewport`; `ImageSidebar` holds the search field, the sort options and `ThumbnailPanel`, which shows the history: a list view over `ThumbnailModel` (the history's rows, in history order), seen through `ThumbnailFilterModel` (the names the search matches, in the chosen sort) and drawn by `ThumbnailDelegate`, and `LiveChannelPanel`, the Live group's rows; `ImageInfoBar` is the strip above the view, with `DisplaySettingsControl` (the Range and Colours buttons) and Freeze and Snapshot; `QtImageSink` moves images and live frames from the server thread to the UI thread; `ActivityLog` is the activity log (a list model, stored in the endpoint's `DataFolder`) and `ActivityLogPanel` shows it; `DividerSplitter` is a splitter whose handles draw a divider line and a grip; `ImageStore` keeps images in files, `StorageWorker` drives it on its own thread, and `StorageController` owns that thread and the restore for the window. |
| `apps/example_sender` | The Visual Debug Example Sender, which shows how a Qt app uses the library. Its window takes a factory that makes an `ImageSender` for an endpoint, so it never knows which transport is behind it, and a new endpoint in its **Send to** field gets a new sender. |
| `examples/pipeline_demo` | An ordinary OpenCV program that sends each stage of a pipeline. Its stages are a separate `pipeline` library so tests can check them. |
| `examples/minimal_sender` | The smallest use of `sendImage`, compiled so the user guide's first example cannot go stale. |
| `examples/live_sender` | Streams a camera, a video file or a test pattern to live channels with `sendLive`, with an edge channel beside it. Everything but the reading of cameras and files is a separate `live_sender_lib`, so tests check it without a camera; only the executable links OpenCV's `videoio`. |

## The dependency rule

Dependencies point one way: applications and examples depend on the libraries, never the reverse.

- **Neither library includes a Qt header or links Qt.** A host application embedding `visual_debug` must
  not inherit Qt, which is large and could clash with its own Qt version.
- `visual_debug` and `viewer_core` do not depend on each other.
- Only `qt_common` and the apps may use Qt.

CMake enforces this: each library's `CMakeLists.txt` links only OpenCV, ZeroMQ and nlohmann/json;
`qt_standard_project_setup()` and automatic `moc`, `uic` and `rcc` are switched on in `apps/CMakeLists.txt`
only; and configuring with `-DID_VISION_LAB_BUILD_QT_APPS=OFF` never looks for Qt, yet builds both
libraries, the three examples and their tests. Library code that uses Qt fails to link in that build. A bare
Qt `#include` may still compile, because Homebrew puts Qt's headers in the same shared include folder as
ZeroMQ's, so code review has to catch those.

## One image, end to end

```mermaid
sequenceDiagram
    participant Host as Host application
    participant Client as visual_debug::Client
    participant Server as Server (worker thread)
    participant Sink as QtImageSink
    participant UI as MainWindow (UI thread)

    Host->>Client: sendImage(mat, "Sobel X")
    Client->>Client: encode: JSON header + pixel bytes<br/>(copies a non-continuous Mat first)
    Client->>Server: ZeroMQ multipart message (REQ)
    Server->>Server: decode and validate
    Server->>Sink: onImageReceived(message)
    Sink-->>UI: imageReceived signal (queued)
    Sink->>Server: true (accepted)
    Server->>Client: {"status":"ok"} (REP)
    Client->>Host: SendResult::Delivered
    UI->>UI: ImageHistory::add (display conversion)
    UI->>UI: ThumbnailPanel::addImage; selects it if listed
    UI->>UI: ImageView shows it fitted (if selected)
    UI->>Sink: markHandled()
```

The reply goes back as soon as the sink accepts the image, before the UI has drawn it, so a sender never
waits for painting.

## One live frame, end to end

A frame sent with `sendLive(frame, "camera")` carries `live: true` and the sender's clock as `sentAt`
([wire format](wire-format.md#header-fields)). It never enters the history: it updates its channel in place.

```mermaid
sequenceDiagram
    participant Host as Host application
    participant Server as Server (worker thread)
    participant Sink as QtImageSink
    participant UI as MainWindow (UI thread)

    Host->>Server: sendLive(frame, "camera")
    Server->>Sink: onImageReceived(message)
    Sink->>Sink: keep it as the channel's waiting frame,<br/>replacing (and counting) an older one
    Sink-->>UI: liveFramesWaiting (queued, once until the next take)
    Sink->>Server: Acceptance::Taken
    Server->>Host: {"status":"ok"}
    UI->>Sink: takeLiveFrames()
    Sink->>UI: newest frame per channel, refused and evicted names
    UI->>UI: LiveChannels::receive
    UI->>UI: converts and draws the shown channel's frame only
    Note over UI: every 500 ms: idle states, row pictures and rates, info strip
```

**The sink never makes a live sender wait.** Live frames do not use the eight slots or the 200 ms wait:
each channel keeps only its newest waiting frame, so a sender faster than the screen loses frames in the
viewer (counted as dropped), never time, and the sink holds at most one waiting frame per channel. It opens
at most 8 channels and refuses a new one whose frame would take the open channels' frames past 1 GiB; a
channel without a frame for 5 s is idle, and the longest-idle one is closed to make room. A refused frame
is answered `too many live channels`, and the UI logs each refused name once.

**The UI does the least it can per frame.** `takeLiveFrames` hands every channel's newest frame to
`LiveChannels`, but only the channel in view is converted and drawn, wrapped in a `QImage` without a copy
(`qt_common::wrapAsQImage`), and the view keeps its zoom and pan while the frame size stays the same. A
500 ms timer, running only while channels exist, does the rest: it marks channels idle, redraws each
row's picture (shrunk before it is converted, and only for a newer frame) and its rate, and refreshes the
info strip. `LiveChannels` takes every time as an argument; `MainWindow` and the sink share one injectable
`steady_clock`, so tests move time by hand.

**Freeze and Snapshot.** A frozen channel keeps its shown frame and sets newer ones aside, still counting
them; unfreezing shows the newest. Snapshot adds the shown frame to the history as an ordinary image with
`ImageSource::LiveSnapshot` and the channel's display settings. Live channels and snapshots stay in memory:
`StorageController` skips both, so **Keep images between sessions** never writes a live frame to disk.

## Threads

| Thread | Runs |
|---|---|
| The host application's thread | `Client::send` or `sendImage`. `sendImage` serialises calls from several threads behind a mutex; a `Client` is used from one thread at a time. |
| The server's worker thread | `Server`'s receive loop: polls the socket every 100 ms, decodes, calls `ImageSink::onImageReceived`, replies. It checks a stop flag on each poll, so `stop()` returns promptly without killing the thread. |
| The viewer's UI thread | Everything in `viewer_core` and all widgets. Nothing there needs a lock. |
| The viewer's storage thread | `StorageWorker`, started by `StorageController`: writes, deletes and loads the images kept between sessions, and their index. Only a viewer with a data folder has one. |

`ImageSink::onImageReceived` is called **on the worker thread**; every sink is responsible for getting the
image to its own UI thread. `QtImageSink` emits a signal connected with `Qt::QueuedConnection`, so the slot
runs on the UI thread. The server stamps each decoded message with `receivedAt` (`std::chrono::system_clock`,
on the worker thread, when decoding finishes), so the viewer's log shows when an image arrived, not when the
UI got round to it.

`ImageSink::onRequestRejected(reason)` is also called on the worker thread, once for every request the
server answers with an error (malformed, `viewer busy`, `viewer paused`, `internal error`), just before the
reply is sent. The reason is the reply's fixed ASCII text, never request bytes. It must return at once: the
sender is waiting for the reply. The default does nothing.

**Pausing.** `Server::setPaused(true)` keeps the port bound and answers every request with `viewer paused`,
checked before anything else, so a paused viewer neither decodes nor offers images to its sink. Senders see
`Rejected` rather than `ViewerUnavailable`, and images already handed to the UI are unaffected.
`Server::stop()` releases the port, and `start()` binds it again.

**Back-pressure.** At most eight images may wait for the UI. When all eight slots are taken, the worker
thread waits up to 200 ms for the UI to call `markHandled()`. A burst from a fast sender is therefore
slowed down to the UI's pace instead of being dropped, and only a UI stalled for longer makes the sender
see `Rejected` ("viewer busy"). The 200 ms stays well inside the client's 500 ms reply timeout.

**Rejections reach the log as counts.** `QtImageSink::onRequestRejected` only adds one to the count for its
reason, under the sink's mutex, and emits `requestsRejected` when the first count appears. The UI, on the
queued signal, takes every count at once with `takeRejections()` and logs one entry per reason, such as
`Rejected 3 requests: [viewer busy]`. However many requests a sender floods the viewer with, the worker
thread never waits on the UI for them and Qt's event queue holds at most one such signal; the counts are
bounded by the handful of fixed reasons. After a line goes out, the UI leaves the counts to grow for a
second before taking them again, so a sender rejected 30 times a second costs the log one line a second,
not thirty. While paused, `viewer paused` rejections are only added up, and logged as one line at Resume
or Stop; `too many live channels` is never logged as a count, because the UI names each refused channel
once instead.

**Storage.** `MainWindow` tells its `StorageController` every change to the history; the controller lives
on the UI thread and passes each one to its `StorageWorker` as a queued call: store an image, delete one,
rewrite the index, delete them all. A call carries a `cv::Mat` by reference count, and nothing modifies a
history image after it is added, so the worker reads the pixels without a lock. Each call first re-reads the
shared setting with `QSettings::sync()`; when another viewer has turned it off, the worker deletes this
viewer's files instead of acting. At start-up the window listens first, then asks the controller to
restore: the worker reads the stored images one at a time, newest first, and stops once the history's
count or byte limit is reached, so it never reads what could not fit. Each image comes back as one queued
signal; the controller hands it to the window, which adds it with `ImageHistory::addOldest`, below
anything that arrived meanwhile. The index is not rewritten until loading has finished, so it cannot lose
the images still being loaded. **Clear History** during a restore makes the controller discard the images
still to come. Closing the window stops a restore at the next image, leaving the rest on disk, and waits
for the calls already queued, so the newest image is never lost. The server thread is never involved.

**Start-up order.** The viewer starts listening from its event loop, not in the window's constructor, so
it never accepts images before its UI can take them. `MainWindow` declares its `QtImageSink` before its
`Server`, so the server's thread has stopped before the sink is destroyed.

## Display rules

`viewer_core::toDisplayImage` produces what is on screen; the pixel readout always reads the original.
With the default display settings (range Auto, colours Grey):

| Input | Display |
|---|---|
| depth `8U` | values unchanged |
| any other supported depth | min–max normalised to 0–255 across all channels together; a constant image becomes black; NaN and ±Inf are ignored for the range and shown as 0 |
| 1 channel | grey |
| 2 channels | R = channel 0, G = channel 1, B = 0 |
| 3 channels | BGR to RGB |
| 4 channels | BGRA to RGBA |

Each image name and live channel can have its own `DisplaySettings`, kept for the session only in a
`DisplaySettingsStore`:

- **Range.** Auto stretches each image or frame from its own minimum to its maximum, as above. Hold keeps
  the range of the frame shown when it was chosen (`autoRange`), so a live channel's brightness stops
  jumping from frame to frame. Fixed uses a typed range. Hold and Fixed map the range to 0–255 for every
  depth, 8U included, clipping values outside it; a range whose minimum is not below its maximum shows
  black.
- **Colours.** Grey, Turbo or Viridis, for single-channel images only. NaN and ±Inf are black whatever the
  map.

The history keeps each image's display picture as it was converted on arrival; an image whose settings
are not the default is converted again from the original whenever it is shown, copied or saved, so Copy
Image and Save As give what is on screen.

`ImageView` samples with nearest neighbour when zoomed in past 1:1, so single pixels stay sharp, and
smoothly when zoomed out. It floors view-to-image coordinates, so the point at −0.5 is pixel −1 (outside
the image), not pixel 0.

## Transport behaviour

The client follows ZeroMQ's "Lazy Pirate" pattern for a request–reply channel that must survive a missing
or restarting peer:

- **No viewer.** Only the first send on a new socket waits (up to `replyTimeout`, 500 ms) for a connection;
  every later send checks with a zero timeout. A failure starts a back-off (`retryInterval`, 1 s) during
  which sends return `ViewerUnavailable` at once. A host application is thus blocked once, for half a
  second, not on every image.
- **Viewer appears later.** The socket is kept after `ViewerUnavailable`; ZeroMQ reconnects it in the
  background, so the first send after the back-off is delivered.
- **Reply lost.** After `Timeout`, the socket is closed and recreated, because a REQ socket still waiting
  for a reply cannot send again. The deadline is `replyTimeout` plus 10 ms per MiB, so a large image is
  not mistaken for a lost one.
- **Never throws.** Every ZeroMQ or JSON error becomes a `SendResult`.

The server binds to loopback only unless `allowRemote` is set, limits frame size and queue length, replies
only with fixed ASCII reasons, and survives any malformed message. Details are in
[`wire-format.md`](wire-format.md#validation).

## Extension points

- **A new header field** (for example `timestamp` or `overlays`): add it in `encode` and read it in
  `decode` as optional. Older viewers ignore unknown fields, so `version` stays 2; follow the
  [versioning rule](wire-format.md#versioning-rule) and update `wire-format.md`.
- **A new sink**: implement `visual_debug::ImageSink` and pass it to a `Server`. A recording sink that
  writes images to disk, or a viewer in another toolkit, needs nothing else from the library.
- **A new sender**: implement `visual_debug::ImageSender` (for example a shared-memory transport for very
  large frames). Code written against `ImageSender`, like the example sender's window, works with it unchanged.
- **A new pipeline stage**: add an entry to `buildPipeline()` in `examples/pipeline_demo/pipeline.cpp`;
  see the [user guide](user-guide.md#4-add-a-stage-to-pipeline_demo).
- **Viewer features** (histograms, overlays, filters): put the logic in `viewer_core` with unit tests, and
  keep the Qt side to drawing and input.
