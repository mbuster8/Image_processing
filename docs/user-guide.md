# User guide

How to run the viewer and the senders, what the viewer shows, and how to use `visual_debug` from your own
OpenCV program. To build everything first, see the [build guide](build-guide.md). Paths below are relative
to the repository root and assume the `debug` preset; replace `debug` with `release` for an optimised
build.

## The programs

| Program | What it is for |
|---|---|
| `visual_debug_viewer` (the **Visual Debug Viewer**, or the viewer) | Shows every image sent to it, keeps a history of thumbnails, zooms, pans and reads out the original value of the pixel under the cursor. Can also open image files itself. |
| `visual_debug_example_sender` (the **Visual Debug Example Sender**) | A small Qt app: open an image file and send it to the viewer. Shows how a GUI application uses the library. |
| `pipeline_demo` | A command-line OpenCV pipeline that sends the result of each of its ten stages. Exercises every image type the viewer handles. |
| `minimal_sender` | The smallest real use of the library: one include, one call per image. |
| `live_sender` | Streams a camera, a video file or a moving test pattern to the viewer as two live channels: the frames as `camera` and their Canny edges as `edges`. It has no window of its own: the viewer shows the stream (see [Running live_sender](#running-live_sender)). |

All senders reach the viewer through the `visual_debug` library, over ZeroMQ on `tcp://127.0.0.1:7139` by
default. Start the viewer first.

## Running the viewer

```sh
open build/debug/apps/viewer/visual_debug_viewer.app
```

The window opens maximised, with the keyboard on the sidebar's thumbnails. The sidebar, the status bar and
the activity log sit on a grey a step off the image area's background, in the light and the dark appearance
alike, and a thin line divides each from the image area; the dividers you can drag carry a small grip in the
middle. Around the image is the window's own background colour, which also follows the appearance. The title
names the image in the view and the endpoint without `tcp://`, for example
`Visual Debug Viewer — Sobel X — 127.0.0.1:7139`, so several viewers can be told apart in the Dock and the
**Window** menu. The right-hand end of the status bar shows the server's buttons, a state dot and the
connection state, then **Log ▴** at the far right (see [Activity log](#activity-log)):

| Status | Dot | Buttons | Meaning |
|---|---|---|---|
| `Starting…` | grey ring | none | The window is up; the viewer starts listening as soon as its event loop runs. |
| `Listening on [tcp://127.0.0.1:7139]` | green | Pause, Stop | Ready to receive. Senders started before this appears get `ViewerUnavailable`. |
| `Listening on […] (remote connections allowed)` | green | Pause, Stop | Started with `--allow-remote`; see the warning below. |
| `Paused: senders get [viewer paused]` | amber | Resume, Stop | The port stays taken; every image is refused. |
| `Stopped: released [tcp://127.0.0.1:7139]` | grey ring | Start | The port is free; senders find no viewer. |
| `Not listening: could not listen on [tcp://127.0.0.1:7139]: …` | grey ring | Try again | Usually another viewer already uses the endpoint. This window still works for opening files. |
| `Not listening: refusing to listen on […]: not a loopback address, and remote connections are not allowed` | grey ring | Try again | The endpoint is not on this machine; see `--allow-remote` below. |

### Server controls

The buttons, and the **Server** menu's **Pause** (⌘P) and **Stop** (⌘.), whose titles change to Resume and
Start, control the receiving side without closing the window. The two differ in what senders see:

- **Pause** keeps the port and answers every image with the error `viewer paused`, so senders get
  `Rejected` and go on running. Nothing new reaches the history until you resume. Use it to study the
  images you have while a pipeline keeps sending.
- **Stop** releases the port, so senders get `ViewerUnavailable`, exactly as if no viewer were running, and
  another viewer can take the port. **Start** listens again on the same endpoint. With `--endpoint` and port
  `*`, the new port can differ.

Images already received are never dropped by either. **Try again** retries a start that failed, for example
once the other viewer on the port has quit.

### Activity log

**Log ▴** in the status bar, or **View → Show Log** (⌘L), opens a panel under the image with what the
viewer did, newest at the top; **Log ▾** closes it. Drag the divider above it to change its height. The
viewer remembers whether the panel was open and how tall it was.

Each entry has the local date and time to the microsecond (`2026-10-27 13:02:44.907113`), a mark for its
kind (information, warning or error) and a text. The viewer logs:

- the server's state: listening, paused, stopped, or not listening and why;
- every received image, with its name, size, channels, depth and pixel bytes, timed when the server finished
  decoding it rather than when the window showed it;
- requests it refused, with the reason (`viewer busy`, a malformed message). Refusals that arrive while the
  window is busy are combined, `Rejected 3 requests: [viewer busy]`, and a sender that goes on being refused
  gets at most one line a second. While the viewer is paused, its refusals are only counted, and logged once
  when you resume or stop: `Rejected 900 requests while paused: [viewer paused]`;
- opened and saved files, renames, duplicates, deletes, undos and copies, and files it could not open or save;
- images dropped because the history is full: `Dropped the oldest image`;
- live channels (see [Live channels](#live-channels)): `Live channel [camera] started`,
  `Live channel [camera] idle after 1234 frames (12 dropped)` (the frames since it started, and how many of
  them were dropped), `Closed live channel [camera]`, `Live channel [cam9] replaced idle channel [camera]`,
  `Refused live channel [cam9]: 8 channels are open` (once per name, for up to 8 names; any more are counted
  as `Rejected 150 requests: [too many live channels]`, at most one line a second), and
  `Snapshot [camera 14:03:22.517] (640×480 · 3 ch · 8U · 921.6 kB)`. Never a line per frame.

Names, file paths, endpoints and refusal reasons are written in square brackets, so a name with spaces reads
plainly: `Renamed [old image.jpg] to [new image.jpg]`. Counts and sizes are not.

While the panel is closed, the status bar shows the newest entry's time and text, greyed out. The list shows
new entries at the top while it is scrolled to the top, and keeps the entries you are reading in place while
you read further down. **Copy** puts the selected entries on the clipboard as lines of text, newest first, or
every entry when none is selected. **Clear** empties the panel and the stored log, without asking.

**Kept between sessions.** The newest 1000 entries are stored in `activity.log`, in a folder per endpoint under
`~/Library/Application Support/Visual Debug Viewer/` (such as `tcp_127.0.0.1_7139`), readable by you only, and
loaded at the next start. Three cases keep the log in memory only, and the log says so at start:

- a port of `*`, which changes on every start, so the folder could never be found again (an endpoint
  without `://` has no folder either, and cannot be listened on);
- a second viewer on the same endpoint, while the first one uses that endpoint's folder;
- a folder that cannot be created or written, such as on a full disk.

### Keeping images between sessions

**History → Keep Images Between Sessions** keeps the history on disk, so a viewer you quit and start again
on the same endpoint shows the same images, newest first, with their names (renames included), their
received and modified times and their original pixels at every depth. It is off by default.

- **What is stored, and where.** Each image is written as it arrives, as its original pixels, in the
  endpoint's folder under `~/Library/Application Support/Visual Debug Viewer/` (such as
  `tcp_127.0.0.1_7139/images`), with a small index of the order, names and times. An image's file is deleted
  when it leaves the history: deleted, or dropped because the history is full. **Undo Delete** writes it
  again. The undo buffer itself is not stored.
- **Starting again.** The viewer starts listening at once, then loads the stored images in the background
  and adds them below any image that arrived meanwhile. Newer arrivals take priority for the history's
  limits: a stored image that no longer fits is dropped and its file deleted. The log records
  `Restored N images`, and how many were skipped as damaged or dropped.
- **One setting for every viewer.** Turning it on writes this viewer's current history. Turning it off asks
  `Delete the N stored images?`, where N counts the stored images of **every** endpoint, and then deletes
  them all; other running viewers stop storing at once. Cancel leaves the setting on. A viewer that starts
  with the setting off also deletes images a crashed viewer left behind, but never those of a viewer that
  is still running.
- **Privacy.** The folders and files are readable by your user account only, but they are **not
  encrypted**: anyone with access to your account, or to a backup of it, can read the stored images.
  Leave the setting off for images that must not reach the disk.
- **Clear History…** deletes every image of this viewer, in memory and on disk, after asking. It cannot be
  undone.
- **Port `*`.** A viewer whose port is `*` gets a different port, and so a different folder, on every
  start: it stores nothing and keeps its log in memory only.
- **Live channels.** Live frames are never stored, and neither are snapshots of them or copies of those,
  even with the setting on. Save As and Copy Image are the only ways a live frame leaves the viewer.

### Options

Pass options after `--args`. Add `-n` to start another instance; without it, `open` only brings a running
viewer to the front:

```sh
open -n build/debug/apps/viewer/visual_debug_viewer.app --args --endpoint tcp://127.0.0.1:7140
```

Or start the binary inside the bundle, which also inherits the terminal's environment and prints
`--help` to it:

```sh
build/debug/apps/viewer/visual_debug_viewer.app/Contents/MacOS/visual_debug_viewer --help
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
| Click a thumbnail | Show that image, fitted. The newest image is at the top and is shown as it arrives, unless a live channel is in the view. |
| Click a live channel's row | Show that channel, updating in place (see [Live channels](#live-channels)). |
| ↑ and ↓ | With the sidebar focused, as it is when the window opens, show the image above or below in the list. |
| **File → Open…** (⌘O) | Open an image file (PNG, JPEG, BMP, TIFF, WebP, PNM, or anything else OpenCV reads). It is read with its original depth and channels, so a 16-bit PNG stays 16-bit, and joins the history like a received image. |
| **File → Save As…** (⇧⌘S) | Save the image in the view, as shown on screen, as PNG or JPEG. The suggested name is the image's name without an image-file extension (`photo.webp #3` suggests `photo #3.png`; `result v1.2` keeps its dot). `.png` is added if you type no extension. |
| **Quit** (⌘Q) | Quit. On macOS it is in the application menu (**Visual Debug Viewer**); on other systems in **File**. |
| **About Visual Debug Viewer** | What the viewer is, its version, and the OpenCV and Qt versions it runs with. On macOS it is in the application menu; on other systems in **Help**. |
| **Edit → Undo Delete** (⌘Z) | Bring back the last deleted image at its place in the list, and show it. The last 10 deletions can be undone. |
| **Edit → Rename…** (⌘R) | Rename the image in the view, in the info strip (see below). |
| **Edit → Duplicate** (⌘D) | Add a copy named `<name> copy` directly above the image, and show it. |
| **Edit → Copy Image** (⌘C) | Put the image on the clipboard, as shown on screen (see the note below). |
| **Edit → Delete** (⌘⌫) | Remove the image and show the next one down the sidebar, or the one above if it was last. With the default sort that is the next older image. If the search hides the deleted image, the first one listed is shown. |
| **Edit → Find…** (⌘F) | Put the cursor in the sidebar's search field, with its text selected. |
| **View → Show in Sidebar**, **View → Sort By** | The same choices as the sidebar's options button (see [The sidebar](#the-sidebar)). |
| **View → Show Log** (⌘L) | Open or close the activity log, as **Log ▴** in the status bar does (see [Activity log](#activity-log)). |
| **Image → Range**, **Image → Colours** | The same choices as the info strip's **Range** and **Colours** buttons (see [Display range and colours](#display-range-and-colours)). |
| **Image → Freeze** (⇧⌘T, or Space) | Keep a live channel's frame in view while frames go on arriving; again to show the newest. Space freezes from anywhere but a text field. Only while a live channel is shown (see [Live channels](#live-channels)). |
| **Image → Snapshot** (⌘T) | Copy the live frame in view into the history, as `camera 14:03:22.517`. Only while a live channel is shown. |
| **View → Appearance** | **System** (the default) follows the system's light or dark appearance; **Light** and **Dark** keep one whatever the system does. The choice is kept, and the example sender shares it (see [Running the example sender](#running-the-example-sender)). Needs Qt 6.8 or newer: built with older Qt, the menu is absent and both programs follow the system. |
| **History → Keep Images Between Sessions** | Store the history on disk, for every viewer (see [Keeping images between sessions](#keeping-images-between-sessions)). |
| **History → Clear History…** | Delete every image in this viewer, in memory and on disk, after asking. |
| Thumbnail icons | The pencil, two squares and bin beside a thumbnail rename, duplicate and delete that image (see [The sidebar](#the-sidebar)). |

The Edit menu acts on the image in the view. While you type in a text field, such as the rename field, ⌘C,
⌘Z and ⌘⌫ act on the text instead.

**Copy Image and Save As take the 8-bit picture on screen, not the original values.** Other programs cannot
take 16-bit, signed or floating-point pixels, so a Sobel result pastes as the stretched grey image you see,
in the display range and colours you chose. The readout (below) is where the original values are. With a live
channel in view, they take its frame on screen, and Save As suggests the name a snapshot of it would get.

The history keeps the newest 100 images, or fewer if they would hold more than 2 GiB of pixels; the
oldest are dropped first. The pixels counted are each image's original and its 8-bit copy on screen, and
they include the last deleted images that **Undo Delete** can bring back (these go first when room is
needed) and every duplicate, though a duplicate shares its original's pixels. An image just received or
opened is always kept, even one that is larger than the limit on its own; so is the original of a
duplicate.

### The sidebar

- **Width.** Drag the divider between the sidebar and the image to make the sidebar between 120 and 400 pixels
  wide. The thumbnails scale with it, and the viewer remembers the width.
- **Cells.** Every item has the same size: the image fitted and centred in its box, with its name and details
  (for example `640×480 · 1 ch · 16S · 614.4 kB`, as in the info strip) below. A long name is shortened in the
  middle. The tooltip shows the full name, the details, the time the image arrived and, if it was renamed or
  duplicated since, the time of that.
- **Selection.** The selected item is filled with a neutral grey, a little stronger while the sidebar has the
  keyboard, and keeps it while you work elsewhere in the window; the item under the pointer gets a lighter
  grey. Names, details and icons keep their usual colours, and a thin line separates neighbouring items.
- **Icons.** The selected item, and the one under the pointer, show three icons beside the picture:
  rename (pencil), duplicate (two squares) and delete (bin). Each icon's tooltip names its shortcut.
- **What items show.** The options button's **Show** menu (also **View → Show in Sidebar**) turns the
  Thumbnail, the Name and the Details on or off; all three are on at first, and the viewer remembers the
  choice. At least one stays on: the last one ticked cannot be unticked. Without the thumbnail, each item is a
  compact row, with the text on the left and the three icons in a row on the right, so a names-only list fits
  many more images. The icons' place is kept on every row, so the text never moves when they appear.
- **Right-click menu.** Rename…, Duplicate, Copy Image, Save As… and Delete, for the thumbnail clicked.
- **Keys.** With the sidebar focused (as it is when the window opens, or after clicking a thumbnail), the
  arrow keys move through the images and Delete or Backspace deletes the selected one. On a Mac keyboard the
  key labelled "delete" is Backspace.

#### Searching and sorting

The row at the top of the sidebar holds the search field and an options button, whose menu holds **Show**
(above) and **Sort By**.

- **Search.** Type part of a name: the sidebar lists the images whose names contain it, ignoring case, so
  `canny` finds `Canny edges`. Only names are searched, and the text is taken literally (no wildcards). While
  the field has text, a line below it counts the images listed out of all images, for example `2 of 10`.
  - Esc clears the search; Esc in an empty field moves to the list.
  - Enter or Down selects the first image listed and moves to the list.
  - ⌘F (**Edit → Find…**) jumps to the field from anywhere in the window.
  - The search starts empty every time the viewer starts.
- **The search never changes the image shown.** If it hides the image in the view, the view keeps showing it
  and the sidebar shows no selection; the Edit menu still acts on it. Images that arrive while the search hides
  them are added and counted quietly, without taking the view. Matching ones are shown as they arrive, so
  searching `Canny` while `pipeline_demo` runs shows each new Canny image and lets the other stages pass.
- **Sort.** The options button's **Sort By** menu (also **View → Sort By**) offers:

  | Key | Natural direction | Sorts by |
  |---|---|---|
  | Time Received (default) | Newest first | The order images arrived in. A copy sits directly above its original, and an undone delete returns to its place. |
  | Time Modified | Newest first | The last rename or duplication; an image never renamed counts its arrival. |
  | Name | A to Z | The name, as Finder sorts it: ignoring case, with numbers by value (`stage 2` before `stage 10`). |
  | Size | Largest first | The bytes of pixel data, also for an opened file (not its size on disk). |

  Choosing a key sets its natural direction; **Ascending** and **Descending** then flip it. Images with equal
  keys stay newest first. The list re-sorts at once when an image arrives or is renamed, and the viewer
  remembers the sort.

### Live channels

A program that sends each frame with `visual_debug::sendLive(frame, "camera")` (see
[worked example 1](#1-one-line-debugging-with-sendimage)), such as `live_sender`, streams it as a **live
channel**: the viewer shows the channel's newest frame in place, instead of adding every frame to the history.

- **The Live group.** While channels exist, a **Live** group sits at the top of the sidebar, above the search,
  with a row per channel in the order they started: the newest frame, the channel's name, its frame rate
  (`30 fps`: frames that arrived in the last second) and its state as a dot and a word. **live**, with a dot in
  the accent colour, while frames arrive; **idle**, with a grey dot, once none has come for 5 seconds;
  **frozen** while frozen, its dot still showing whether frames arrive. Three rows show at once; more scroll.
- **Showing a channel.** Click its row. The first channel to start is shown at once if nothing else is in the
  view. The view shows a history image or a channel, never both: clicking a thumbnail leaves the channel, and
  clicking the row again returns to it. Frames of the same size keep the zoom and pan; a frame of another size
  is fitted. The readout reads the frame on screen, and the window's title names the channel.
- **Nothing takes its place.** While a channel is shown, images that arrive are listed in the sidebar without
  taking the view, so the stream stays in view. Rename, Duplicate and Delete are off; Copy Image and Save As
  take the frame on screen.
- **The info strip** shows, for example,
  `camera  live · 1920×1080 · 3 ch · 8U · 30 fps in · 30 drawn · 12 dropped · 18 ms`: the state, the frame's
  size and type, the frames that arrived and the frames the view drew in the last second, the frames dropped
  since the channel's row appeared, idle spells included (a newer frame arrived before the view could take
  the older one), and the frame's
  age: the time it arrived minus the time the sender stamped on it. The age compares two clocks, so it means
  something only when sender and viewer run on one computer; a frame without a stamp shows none. The counters
  refresh twice a second.
- **Freeze** (**Image → Freeze**, ⇧⌘T, Space, or the strip's **Freeze** button) keeps the frame in view while
  frames go on arriving and being counted; freezing again shows the newest. Each channel is frozen on its own.
- **Snapshot** (**Image → Snapshot**, ⌘T, or the strip's **Snapshot** button) copies the frame in view, frozen
  or not, into the history as `camera 14:03:22.517`, its arrival time to the millisecond, with the channel's
  range and colours. The channel stays in view. Snapshots stay in memory, even with Keep Images Between
  Sessions on.
- **Closing.** A channel that has gone idle shows a close button (✕) on its row. One that still streams has
  none: stop its sender, and 5 seconds later it is idle. Closing the channel in view shows the image you
  looked at before it, if it is still in the history.
- **Limits.** At most 8 channels, whose newest frames may hold 1 GiB together. A ninth channel takes the place
  of the one that has been idle longest; with all 8 active, its frames are refused, and its sender gets
  `Rejected` (`too many live channels`). So does any frame that would take the channels past 1 GiB, unless
  closing idle channels makes room. Keep a channel's name the same for the whole stream: a name with a frame
  number in it makes every frame a new channel. A sender is never slowed down: the viewer keeps one waiting frame per
  channel, the newest, and counts the older ones it replaces as dropped.

### The info strip

One line above the image shows its name, a pencil and its details, for example
`Sobel X ✎  640×480 · 1 ch · 16S · 614.4 kB`. The size is the image's pixel data (rows × columns × bytes per
element) for a received image, and the file's size on disk, marked `file`, for an opened one. Sizes use decimal
units, as Finder shows them. With no image, the strip is hidden. At its right end are the **Range** and
**Colours** buttons (see [Display range and colours](#display-range-and-colours)) and, while a live channel is
shown, **Freeze** and **Snapshot**; a live channel's name has no pencil.

**Renaming.** Click the pencil (here or on the thumbnail) and type. Enter saves, Esc cancels, and clicking
elsewhere saves, as in Finder. The rename stays with the image it started on, even if new images arrive
meanwhile.

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
| 4 channels | Read as BGRA; transparent areas show a grey checkerboard, in greys that suit dark or light mode. |

**Save As…** writes this displayed version; PNG and JPEG cannot hold every depth.

### Display range and colours

The info strip's **Range: Auto ▾** and, for a single-channel image, **Colours: Grey ▾** change how the
picture in the view maps the original values to the screen, for history images and live channels alike. The
**Image** menu holds the same choices. The readout still shows the original values.

| Range | On screen |
|---|---|
| **Auto** (the default) | As in the table above: 8-bit values as they are, other depths stretched from each image's smallest value to its largest. |
| **Hold** | The range Auto shows now, kept for the frames that follow, so a live channel's brightness stops changing with each frame's range. |
| **Fixed…** | A minimum and maximum that you type, with a full stop as the decimal mark. **Apply** is enabled while the minimum is below the maximum. |

Hold and Fixed apply to 8-bit images too. The minimum shows black and the maximum white, and values outside
the range are clipped to them.

| Colours | On screen |
|---|---|
| **Grey** (the default) | Grey levels. |
| **Turbo** | OpenCV's Turbo map, from dark blue for low values through green to dark red for high ones. |
| **Viridis** | OpenCV's Viridis map, from dark purple to yellow, which keeps its order in grey print and for most colour-blind readers. |

NaN and infinite values stay black under every map. The choice is kept for the image's name until the viewer
quits: other images of that name, later arrivals among them, look the same; a rename takes it along; a
snapshot takes its channel's. Thumbnails keep the Auto, grey picture. Copy Image and Save As take the picture
as shown.

## Running the example sender

```sh
open build/debug/apps/example_sender/visual_debug_example_sender.app
```

The Visual Debug Example Sender shows how a Qt application uses the library. Its icon, a white paper
plane in the viewer's blue ring, tells it apart from the viewer's eye in the Dock. The bar along the top, on the same panel grey as the viewer's sidebar, holds **Open…** (Ctrl+O), **Send
image** (Ctrl+Return) and the **Send to** field with the endpoint images go to; the preview fills the rest of
the window. Open an image and a preview appears;
**Send image** stays disabled until then. Each send shows its name and result in the status bar, such as
`[photo.png #1]: Delivered` (see [Send results](#send-results)).

The viewer shows the image as `<file name> #1`, then `#2` for the next send, and so on. A send the viewer
did not take (`Viewer unavailable`, `Rejected`, `Invalid image`, `Invalid endpoint`) keeps its number, so
the next send uses it. `Timeout` uses up its number, because the image may still have reached the
viewer. The numbers count every send of the session, whichever file it was, and start again
at `#1` when the example sender restarts.

**View → Appearance** offers the viewer's System, Light and Dark choices, and the two programs share one
setting: choose Dark in either, and the other turns dark when you next switch to it. With Qt older than 6.8
the example sender has no View menu. **About Visual Debug Example Sender** sits in the application menu on
macOS, and in **Help** elsewhere.

**Send to** starts with `$VISUAL_DEBUG_ENDPOINT` if set, otherwise with the endpoint the example sender last
sent to, otherwise with `tcp://127.0.0.1:7139`. Edit it to send to another viewer, such as one started with
`--endpoint tcp://127.0.0.1:7140`; the next send goes there, and the example sender remembers it for the
next start.
A send that finds the endpoint unusable (`Invalid endpoint`, for example `tcp://localhost` without a port)
outlines the field in red until you edit it, and is not remembered. A variable set to
`tcp://127.0.0.1:7139` itself counts as unset, so the remembered endpoint is used.

## Running live_sender

`live_sender` reads frames, sends each one to the viewer as live channel `camera` and its Canny edges as live
channel `edges`, and prints a summary when it stops. Everything you look at and every control (Freeze,
Snapshot, the display range) is in the viewer: this is how a vision program uses the library, doing the
processing while the viewer shows the stages.

```text
live_sender [--camera N | <video file> | --pattern [--size WxH] [--fps N]] [--frames N] [--endpoint <uri>]
live_sender --list-cameras
```

| Argument | Meaning |
|---|---|
| `--camera N` | Read camera number `N`. `0` is the first camera macOS lists; a USB camera, or an iPhone through Continuity Camera, may be `1` or `2`. |
| `<video file>` | Read a video file instead, such as a `.mov` recorded with QuickTime Player (**File → New Movie Recording**) or a clip from a phone: any format the installed OpenCV decodes. Homebrew's OpenCV uses FFmpeg, so `.mov`, `.mp4`, `.mkv` and `.avi` all work. |
| `--pattern` | Send a generated moving test pattern; needs no camera and no file. `--size WxH` sets its size (default `640x480`, at most what the viewer takes) and `--fps N` its rate (default 30, from 0.1 to 1000). |
| *(none of the three)* | The same as `--camera 0`. |
| `--frames N` | Stop after `N` frames. Without it, a camera runs until Ctrl+C and a file until its end. |
| `--endpoint <uri>` | The viewer to send to: by default `$VISUAL_DEBUG_ENDPOINT` if set, else `tcp://127.0.0.1:7139`. |
| `--list-cameras` | Try camera numbers 0 to 9, print the ones that open with their frame size, and exit. OpenCV cannot read cameras' names, so this is how you find your USB camera's number. |

A video file is sent at its own frame rate, and the pattern at `--fps`, so they play like a recording; a
camera delivers frames at its own pace. On macOS, camera access is granted to the program that starts
`live_sender` (Terminal, Qt Creator or VS Code), so macOS may ask that program for permission the first time.
Ctrl+C stops it, and the summary still prints:

```sh
build/debug/examples/live_sender/live_sender --pattern --frames 90
```

```text
camera: 90 sent, 90 delivered
edges: 90 sent, 90 delivered
```

Exit codes follow `pipeline_demo`: 0 when every frame was delivered, 1 when any was not, and 2 for bad
arguments or a camera or file that cannot be read.

**Trying live channels by hand:**

1. Start the viewer, then run `live_sender --list-cameras` and note your camera's number.
2. Run `live_sender --camera 1` (or the number you found). The sidebar gets a **Live** group with `camera`
   and `edges`, each with a frame rate and a dot; the first one shows in the view.
3. Zoom and pan while it runs, and hover to read pixels. Press Space to freeze and again to resume; press ⌘T
   for a snapshot. Choose `edges` and try **Range → Hold**, **Fixed…**, and **Colours → Turbo**.
4. Stop `live_sender` with Ctrl+C. After 5 seconds both channels turn grey and idle; close them with their ✕.
5. Repeat with a `.mov` instead of the camera, and with two viewers on different endpoints.

## Configuration

`VISUAL_DEBUG_ENDPOINT` sets the endpoint for `sendImage()`, `sendLive()`, the example sender,
`pipeline_demo`, `live_sender` and the viewer alike, so they find each other without configuration. An empty
value counts as unset. An app started with
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
| `Rejected` | The viewer answered with an error. Usually it is busy (eight images were waiting for its display for over 200 ms) or paused, or, for a live frame, it already shows as many live channels as it can, or the frame would take them past their memory budget (`too many live channels`). | Send less often, or ignore it; the viewer is still running. A paused viewer takes images again once resumed. |
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

For a stream, such as a camera or a video, send each frame with `visual_debug::sendLive(frame, "camera")`
instead; a `Client` has the same `sendLive`. It shares `sendImage`'s connection, marks the frame as part of
the live channel `camera`, and stamps it with the time it was sent (the `live` and `sentAt` fields of the
[wire format](wire-format.md)). A viewer that predates live channels shows it as an ordinary image.

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
$ build/debug/examples/pipeline_demo/pipeline_demo resources/icons/viewer_icon.png
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
open build/debug/apps/viewer/visual_debug_viewer.app
open -n build/debug/apps/viewer/visual_debug_viewer.app --args --endpoint tcp://127.0.0.1:7140
```

`open` returns before the viewers are ready; when the second one's status bar says
`Listening on [tcp://127.0.0.1:7140]`, point a sender at it:

```sh
VISUAL_DEBUG_ENDPOINT=tcp://127.0.0.1:7140 build/debug/examples/minimal_sender/minimal_sender
build/debug/examples/pipeline_demo/pipeline_demo --endpoint tcp://127.0.0.1:7140
```

Both senders reach the second viewer only; the first keeps listening on 7139. This is useful for
comparing two versions of an algorithm, one per window.

### 6. Link the library from your own CMake project

`visual_debug` needs OpenCV, ZeroMQ (`cppzmq`) and nlohmann/json, but never Qt. Add this repository as a
subdirectory, for example as a git submodule in `external/id-vision-lab`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(my_app LANGUAGES CXX)

# Only the libraries: no Qt apps, examples or tests from the visual debugger.
set(ID_VISION_LAB_BUILD_QT_APPS OFF)
set(ID_VISION_LAB_BUILD_EXAMPLES OFF)
set(ID_VISION_LAB_BUILD_TESTS OFF)
add_subdirectory(external/id-vision-lab)

find_package(OpenCV REQUIRED COMPONENTS core imgcodecs)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE visual_debug::visual_debug opencv_imgcodecs)
```

Linking `visual_debug::visual_debug` brings its include folder, C++20 and OpenCV core with it. Your own
project still finds the OpenCV modules it uses itself (here `imgcodecs` for `cv::imread`). The `main.cpp`
of example 2 builds with exactly this file.
