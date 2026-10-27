# User guide

How to run the viewer and the senders, what the viewer shows, and how to use `visual_debug` from your own
OpenCV program. To build everything first, see the [build guide](build-guide.md). Paths below are relative
to the repository root and assume the `debug` preset; replace `debug` with `release` for an optimised
build.

## The programs

| Program | What it is for |
|---|---|
| `image_processing` (the **viewer**) | Shows every image sent to it, keeps a history of thumbnails, zooms, pans and reads out the original value of the pixel under the cursor. Can also open image files itself. |
| `client` | A small Qt app: open an image file and send it to the viewer. Shows how a GUI application uses the library. |
| `pipeline_demo` | A command-line OpenCV pipeline that sends the result of each of its ten stages. Exercises every image type the viewer handles. |
| `minimal_sender` | The smallest real use of the library: one include, one call per image. |

All senders reach the viewer through the `visual_debug` library, over ZeroMQ on `tcp://127.0.0.1:7139` by
default. Start the viewer first.

## Running the viewer

```sh
open build/debug/apps/image_processing/image_processing.app
```

The window opens maximised. The right-hand end of the status bar shows the connection state:

| Status | Meaning |
|---|---|
| `Starting…` | The window is up; the viewer starts listening as soon as its event loop runs. |
| `Listening on tcp://127.0.0.1:7139` | Ready to receive. Senders started before this appears get `ViewerUnavailable`. |
| `Listening on … (remote connections allowed)` | Started with `--allow-remote`; see the warning below. |
| `Not listening: could not listen on tcp://127.0.0.1:7139: …` | Usually another viewer already uses the endpoint. This window still works for opening files. |
| `Not listening: refusing to listen on …: not a loopback address, and remote connections are not allowed` | The endpoint is not on this machine; see `--allow-remote` below. |

### Options

Pass options after `--args`. Add `-n` to start another instance; without it, `open` only brings a running
viewer to the front:

```sh
open -n build/debug/apps/image_processing/image_processing.app --args --endpoint tcp://127.0.0.1:7140
```

Or start the binary inside the bundle, which also inherits the terminal's environment and prints
`--help` to it:

```sh
build/debug/apps/image_processing/image_processing.app/Contents/MacOS/image_processing --help
```

| Option | Effect |
|---|---|
| `--endpoint <uri>` | The ZeroMQ endpoint to listen on. Default: `$VISUAL_DEBUG_ENDPOINT` if set, else `tcp://127.0.0.1:7139`. |
| `--allow-remote` | Allow an endpoint that other machines can reach, such as `tcp://*:7139` or a network interface. |
| `--help`, `--version` | Print help or the version and exit. |

> **Warning:** the channel is neither encrypted nor authenticated. With `--allow-remote`, anyone who can
> reach the port can send images to your viewer, and images sent to it cross the network in clear text.
> Use it only on a network you trust.

### Viewer controls

| Action | Effect |
|---|---|
| Mouse wheel | Zoom in or out by 1.25× per notch, keeping the point under the cursor in place. Trackpad scrolling zooms smoothly. The closest zoom still shows at least 3×3 image pixels. |
| Drag with the left button | Pan. The image cannot be dragged past its edges. |
| Double-click | Fit the whole image to the window again. |
| Hover | The status bar shows the pixel under the cursor (see below). Leaving the image clears it. |
| Click a thumbnail | Show that image, fitted. The newest image is at the top and is shown as it arrives. |
| **File → Open…** (⌘O) | Open an image file (PNG, JPEG, BMP, TIFF, WebP, PNM, or anything else OpenCV reads). It is read with its original depth and channels, so a 16-bit PNG stays 16-bit, and joins the history like a received image. |
| **File → Save As…** (⇧⌘S) | Save the displayed image as PNG or JPEG. `.png` is added if you type no extension. |
| **Quit** (⌘Q) | Quit. On macOS it is in the application menu (**Image Processing**); on other systems in **File**. |

The history keeps the newest 100 images, or fewer if they would hold more than 2 GiB of pixels; the
oldest are dropped first, and the newest is always kept.

### Reading pixel values

The readout uses the **original** image as it was sent, not the 8-bit picture on screen:

```text
X=12 Y=40 · B=12 G=200 R=3
```

| Channels | Labels |
|---|---|
| 1 | `I` (intensity) |
| 2 | `C0 C1` |
| 3 | `B G R` |
| 4 | `B G R A` |

Integers print exactly, including negative values of signed images. Floating-point values print with up
to six significant digits, always with a decimal point regardless of the system's language settings.

### How images are displayed

| Image | On screen |
|---|---|
| 8-bit unsigned | Values as they are. |
| Any other depth (8-bit signed, 16-bit, 32-bit integer, 32- and 64-bit floating point) | Stretched so the smallest value in the image is black and the largest white, across all channels together. A constant image is black. NaN and infinite values are ignored when stretching and shown black. |
| 1 channel | Grey. |
| 2 channels | Channel 0 as red, channel 1 as green (useful for optical flow). |
| 3 channels | Read as BGR, OpenCV's order. |
| 4 channels | Read as BGRA; transparent areas show the dark background through. |

**Save As…** writes this displayed version; PNG and JPEG cannot hold every depth.

## Running the client

```sh
open build/debug/apps/client/client.app
```

Click **Open…** and choose an image; a preview appears. Click **Send image**: the viewer shows it as
`<file name> #1`, then `#2` for the next send, and so on. The client's status bar shows the result
(`Delivered`, `Viewer unavailable`, …; see [Send results](#send-results)). The client sends to
`$VISUAL_DEBUG_ENDPOINT` if set, otherwise to `tcp://127.0.0.1:7139`.

## Configuration

`VISUAL_DEBUG_ENDPOINT` sets the endpoint for `sendImage()`, the client, `pipeline_demo` and the viewer
alike, so they find each other without configuration. An empty value counts as unset. An app started with
`open` does not see your shell's variables: use `open --env VISUAL_DEBUG_ENDPOINT=… <app>`, or the
viewer's `--endpoint` option.

Pointing it at another machine works, but the images travel unencrypted.

## Send results

Every send returns a `visual_debug::SendResult`. Sending never throws and never blocks for long: with no
viewer, the first send waits up to `replyTimeout` (500 ms by default) and later sends fail at once.

| Result | Meaning | What to do |
|---|---|---|
| `Delivered` | The viewer accepted the image. | Nothing. |
| `ViewerUnavailable` | No viewer is listening, or a retry back-off is running after a failure (1 s by default). | Start the viewer. The next send after the back-off connects; no restart of your program is needed. |
| `Rejected` | The viewer answered with an error. Usually it is busy: eight images were waiting for its display for over 200 ms. | Send less often, or ignore it; the viewer is still running. |
| `InvalidImage` | The image is empty, not 2-D, larger than 32768 pixels in a dimension or 512 MiB in total, or of a type the format does not carry (more than four channels, or the OpenCV 5 depths `16F`, `16BF`, `Bool`, `64U`, `64S`, `32U`). Nothing was sent. | Convert the image, for example with `convertTo(CV_32F)`. |
| `Timeout` | The image went out but no answer came back in time. The deadline grows by 10 ms per MiB, so large images are not cut short. | Check that the viewer is not frozen. The connection is rebuilt automatically. |
| `InvalidEndpoint` | The endpoint string cannot be used, for example an empty string or `tcp://localhost` without a port. | Fix the endpoint. |

## Worked examples

Each example below was run on macOS while writing this guide; the output shown is what it printed.

### 1. One-line debugging with `sendImage`

`examples/minimal_sender/main.cpp` is the whole program:

```cpp
#include "visual_debug/send_image.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <iostream>
#include <string>

int main() {
    // A horizontal gradient, 0 on the left to 255 on the right.
    cv::Mat gradient(240, 256, CV_8UC1);
    for (int x = 0; x < gradient.cols; ++x) {
        gradient.col(x).setTo(x);
    }

    for (const int threshold : {64, 128, 192}) {
        cv::Mat binary;
        cv::threshold(gradient, binary, threshold, 255, cv::THRESH_BINARY);

        const std::string name = "threshold " + std::to_string(threshold);
        const visual_debug::SendResult result = visual_debug::sendImage(binary, name);
        std::cout << name << ": " << visual_debug::toString(result) << '\n';
    }
}
```

With the viewer running:

```text
$ build/debug/examples/minimal_sender/minimal_sender
threshold 64: Delivered
threshold 128: Delivered
threshold 192: Delivered
```

The viewer shows three black-and-white images whose edge moves right. `sendImage` can be called from any
thread; calls share one connection and take turns.

### 2. A `Client` with your own options

For more control, create one `visual_debug::Client` and keep it for the life of your program. This
program sends an image file to a second viewer on port 7140 and handles every result:

```cpp
#include "visual_debug/client.h"

#include <opencv2/imgcodecs.hpp>

#include <chrono>
#include <iostream>

int main(int argc, char* argv[]) {
    using namespace std::chrono_literals;

    // One Client for the whole program: it keeps its socket, so later sends are fast.
    visual_debug::Client client(visual_debug::ClientOptions{
        .endpoint = "tcp://127.0.0.1:7140", // a second viewer, started with --endpoint tcp://127.0.0.1:7140
        .replyTimeout = 300ms,              // the default is 500 ms; keep it above about 250 ms
        .retryInterval = 2s,                // the default is 1 s
    });

    const cv::Mat image = cv::imread(argc > 1 ? argv[1] : "photo.png", cv::IMREAD_UNCHANGED);

    switch (client.send(image, "input")) {
    case visual_debug::SendResult::Delivered:
        std::cout << "shown in the viewer\n";
        break;
    case visual_debug::SendResult::ViewerUnavailable:
        std::cout << "no viewer running; carrying on without it\n";
        break;
    case visual_debug::SendResult::Rejected:
        std::cout << "the viewer refused the image (busy, or an incompatible version)\n";
        break;
    case visual_debug::SendResult::InvalidImage:
        std::cout << "nothing to send: the file did not load, or the image type is not supported\n";
        break;
    case visual_debug::SendResult::Timeout:
        std::cout << "the viewer did not answer in time\n";
        break;
    case visual_debug::SendResult::InvalidEndpoint:
        std::cout << "the endpoint string is wrong\n";
        break;
    }
}
```

Built as `my_app` (see example 6) and run three ways:

```text
$ ./my_app resources/icons/open_icon.png          # no viewer on 7140
no viewer running; carrying on without it
$ ./my_app /nonexistent.png
[ WARN:0@0.001] global loadsave.cpp:278 findDecoder imread_('/nonexistent.png'): can't open/read file: check file path/integrity
nothing to send: the file did not load, or the image type is not supported
$ ./my_app resources/icons/open_icon.png          # viewer started with --endpoint tcp://127.0.0.1:7140
shown in the viewer
```

The `WARN` line is OpenCV's own log; `OPENCV_LOG_LEVEL=ERROR` hides it. `Client` is not thread-safe: use
one per thread, or `sendImage`.

Keep `replyTimeout` above about 250 ms. A busy viewer waits up to 200 ms for its display before it
answers, so a shorter timeout turns "busy" into `Timeout`, and every `Timeout` costs a new connection
and a retry back-off.

### 3. `pipeline_demo` on your own image

```text
pipeline_demo [image-path] [--endpoint <uri>]
```

Without a path it uses a generated test pattern; with one, it reads the file as 8-bit colour. It exits
with 0 when every stage was delivered, 1 when any was not, and 2 for a bad argument or an unreadable file.

```text
$ build/debug/examples/pipeline_demo/pipeline_demo resources/icons/application_icon.png
Original: Delivered
With alpha: Delivered
Greyscale: Delivered
Gaussian blur: Delivered
Sobel X: Delivered
Canny edges: Delivered
Distance transform: Delivered
Laplacian: Delivered
Centre crop: Delivered
Multi-channel 16-bit: Delivered
```

For the 640×480 test pattern, the thumbnails show these summaries:

| Stage | Summary | What to look at |
|---|---|---|
| Original | `640×480 · 3 ch · 8U` | The input. |
| With alpha | `640×480 · 4 ch · 8U` | Transparent on the left, opaque on the right. |
| Greyscale | `640×480 · 1 ch · 8U` | |
| Gaussian blur | `640×480 · 1 ch · 8U` | |
| Sobel X | `640×480 · 1 ch · 16S` | Hover an edge: the readout shows negative values on one side and positive on the other. Black is the most negative value, not zero. |
| Canny edges | `640×480 · 1 ch · 8U` | |
| Distance transform | `640×480 · 1 ch · 32F` | Each pixel's Euclidean distance to the nearest Canny edge: fractional values such as 1.41421 where the nearest edge is diagonal. |
| Laplacian | `640×480 · 1 ch · 64F` | |
| Centre crop | `320×240 · 3 ch · 8U` | A region of interest that shares memory with the original; the library sends it correctly anyway. |
| Multi-channel 16-bit | `640×480 · 3 ch · 16U` | Values up to 65535 in the readout, although the screen shows 8 bits. |

With no viewer running, the program finishes in about half a second:

```text
$ time build/debug/examples/pipeline_demo/pipeline_demo
Original: Viewer unavailable
With alpha: Viewer unavailable
…
Multi-channel 16-bit: Viewer unavailable
  0.04s user 0.02s system 10% cpu 0.538 total
```

### 4. Add a stage to `pipeline_demo`

The stages are a list in `examples/pipeline_demo/pipeline.cpp`; the loop that sends them never changes.
To add an Otsu threshold after the Canny edges, add one entry to the list in `buildPipeline()`:

```cpp
        {"Canny edges", edges},
        {"Otsu threshold",
         [](const cv::Mat& original) {
             cv::Mat result;
             cv::threshold(toGrey(original), result, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
             return result;
         }},
```

Every stage receives the original image, so a stage never depends on the one before it. Rebuild and run
`pipeline_demo`: the viewer shows eleven images. `ctest --preset debug -R Pipeline` then fails in
`Pipeline.StagesProduceTheDocumentedTypes`, which lists the stages and their types: add
`{"Otsu threshold", CV_8UC1}` to its table (and update the stage index in
`Pipeline.CentreCropIsANonContinuousRegionOfInterest`).

### 5. Two viewers side by side

Start a second viewer on its own port:

```sh
open build/debug/apps/image_processing/image_processing.app
open -n build/debug/apps/image_processing/image_processing.app --args --endpoint tcp://127.0.0.1:7140
```

`open` returns before the viewers are ready; when the second one's status bar says
`Listening on tcp://127.0.0.1:7140`, point a sender at it:

```sh
VISUAL_DEBUG_ENDPOINT=tcp://127.0.0.1:7140 build/debug/examples/minimal_sender/minimal_sender
build/debug/examples/pipeline_demo/pipeline_demo --endpoint tcp://127.0.0.1:7140
```

Both senders reach the second viewer only; the first keeps listening on 7139. This is useful for
comparing two versions of an algorithm, one per window.

### 6. Link the library from your own CMake project

`visual_debug` needs OpenCV, ZeroMQ (`cppzmq`) and nlohmann/json, but never Qt. Add this repository as a
subdirectory, for example as a git submodule in `external/Image_processing`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(my_app LANGUAGES CXX)

# Only the libraries: no Qt apps, examples or tests from the visual debugger.
set(IMAGE_PROCESSING_BUILD_QT_APPS OFF)
set(IMAGE_PROCESSING_BUILD_EXAMPLES OFF)
set(IMAGE_PROCESSING_BUILD_TESTS OFF)
add_subdirectory(external/Image_processing)

find_package(OpenCV REQUIRED COMPONENTS core imgcodecs)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE visual_debug::visual_debug opencv_imgcodecs)
```

Linking `visual_debug::visual_debug` brings its include folder, C++20 and OpenCV core with it. Your own
project still finds the OpenCV modules it uses itself (here `imgcodecs` for `cv::imread`). The `main.cpp`
of example 2 builds with exactly this file.
