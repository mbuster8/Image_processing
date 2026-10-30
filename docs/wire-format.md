# Wire format, version 2

This is the authoritative description of what `visual_debug` sends to the viewer. A sender written in any
language can talk to the viewer from this page alone. The C++ implementation is
`libs/visual_debug/src/wire_format.cpp`; its tests are `tests/visual_debug/wire_format_test.cpp`.

## Transport

ZeroMQ **REQ/REP**. The sender (`visual_debug::Client`) uses a REQ socket and connects; the viewer
(`visual_debug::Server`) uses a REP socket and binds. Every request gets exactly one reply.

The default endpoint is `tcp://127.0.0.1:7139`. Both sides use the `VISUAL_DEBUG_ENDPOINT` environment
variable instead when it is set and not empty.

## Request: one image

One ZeroMQ **multipart** message with exactly two frames:

| Frame | Content |
|---|---|
| 1. Header | A UTF-8 JSON object, at most 64 KiB. |
| 2. Pixels | Raw bytes, `rows × cols × channels × element size`, row-major, tightly packed (no row padding), channels interleaved in OpenCV order (BGR, BGRA), little-endian elements. |

The pixels stay binary: base64 inside the JSON would make the message a third larger and cost a decode
for every image. The JSON only describes them, so new optional fields can be added without breaking
existing viewers. Jupyter kernels use the same split (JSON frames plus binary buffers over ZeroMQ).

### Header fields

```json
{
  "protocol": "visual_debug",
  "version": 2,
  "type": "image",
  "name": "Sobel X",
  "rows": 480,
  "cols": 640,
  "depth": "16S",
  "channels": 1
}
```

| Field | Rule |
|---|---|
| `protocol` | exactly `"visual_debug"` |
| `version` | the integer `2` (the 2013 binary layout is retrospectively version 1) |
| `type` | exactly `"image"` |
| `name` | a string of at most 1024 bytes of UTF-8 |
| `rows`, `cols` | integers from 1 to 32768 |
| `depth` | one of `"8U"`, `"8S"`, `"16U"`, `"16S"`, `"32S"`, `"32F"`, `"64F"` |
| `channels` | an integer from 1 to 4 |
| `live` | optional: a JSON boolean. `true` marks a frame of a live channel (a camera or video stream) named by `name`; the viewer shows it in place of the channel's previous frame instead of adding it to the history |
| `sentAt` | optional: an integer from 0 to 253402300799999 (the end of the year 9999), the sender's clock in whole milliseconds since the Unix epoch when it sent the message |

A version 2 viewer that predates `live` ignores both optional fields and shows a live frame as an ordinary
image; a sender that predates them sends neither. The C++ sender writes them for `sendLive` only, and leaves
`sentAt` out if its clock reads before 1970 or after 9999 rather than send a value the viewer would refuse.

Element sizes: 1 byte for `8U` and `8S`, 2 for `16U` and `16S`, 4 for `32S` and `32F`, 8 for `64F`. The
pixel frame may hold at most 512 MiB; the C++ sender refuses larger images before touching the network.

Integer fields must be JSON integers: `true`, `2.0` and `"2"` are all rejected. Values are read as 64-bit
integers and range-checked before narrowing, so `4294967297` is rejected rather than read as `1`.

The C++ sender also makes every name valid UTF-8 (each invalid byte becomes U+FFFD) and truncates it at a
character boundary to fit 1024 bytes.

### Example

The header `visual_debug::encode` produces for a 640×480 single-channel 16-bit signed image named
`Sobel X` (120 bytes, followed by a pixel frame of 640 × 480 × 1 × 2 = 614 400 bytes):

```text
{"protocol":"visual_debug","version":2,"type":"image","name":"Sobel X","rows":480,"cols":640,"depth":"16S","channels":1}
```

```text
7b 22 70 72 6f 74 6f 63 6f 6c 22 3a 22 76 69 73   {"protocol":"vis
75 61 6c 5f 64 65 62 75 67 22 2c 22 76 65 72 73   ual_debug","vers
69 6f 6e 22 3a 32 2c 22 74 79 70 65 22 3a 22 69   ion":2,"type":"i
6d 61 67 65 22 2c 22 6e 61 6d 65 22 3a 22 53 6f   mage","name":"So
62 65 6c 20 58 22 2c 22 72 6f 77 73 22 3a 34 38   bel X","rows":48
30 2c 22 63 6f 6c 73 22 3a 36 34 30 2c 22 64 65   0,"cols":640,"de
70 74 68 22 3a 22 31 36 53 22 2c 22 63 68 61 6e   pth":"16S","chan
6e 65 6c 73 22 3a 31 7d                           nels":1}
```

A live frame of a 640×480 colour channel named `camera`, as `visual_debug::sendLive` sends it at
13:23:22.517 UTC on 30 October 2026:

```text
{"protocol":"visual_debug","version":2,"type":"image","name":"camera","rows":480,"cols":640,"depth":"8U","channels":3,"live":true,"sentAt":1793366602517}
```

## Validation

The viewer rejects a message when:

- it does not have exactly two frames;
- the header is over 64 KiB, is not valid UTF-8 JSON, or is not a JSON object;
- a required field is missing, has the wrong JSON type, or is out of range, or an optional field is present
  with the wrong JSON type or out of range;
- the pixel frame is not exactly `rows × cols × channels × element size` bytes. The size is computed in
  64-bit arithmetic from fields already range-checked, so it is at most 2³⁵ and cannot overflow, and it is
  checked before anything is allocated.

Unknown header fields are **ignored**, so a newer sender can add optional fields that older viewers skip.

The decoder reads the parsed header's fields directly and never copies or re-serialises it. Parsing and
destroying a deeply nested 64 KiB document is safe on the viewer's receive thread; copying or printing one
would overflow that thread's stack.

The viewer also limits how much a peer can make it buffer: each frame may be at most 512 MiB
(`ZMQ_MAXMSGSIZE`) and at most four messages from each connected sender wait in its receive queue
(`ZMQ_RCVHWM`, which ZeroMQ applies per connection).

## Reply

One frame of JSON, either

```json
{"status":"ok"}
```

or

```json
{"status":"error","reason":"rows out of range"}
```

`reason` is always one of these fixed ASCII texts. A reply never contains bytes from the request, so a
hostile header cannot make the reply itself fail to encode.

| Reason | Meaning |
|---|---|
| `expected exactly two frames` | The request was not a header frame plus a pixel frame. |
| `header too large` | The header frame is over 64 KiB. |
| `header is not valid JSON` | The header did not parse (including invalid UTF-8). |
| `header is not a JSON object` | The header parsed but is an array, string or number. |
| `unknown protocol` | `protocol` is missing or not `"visual_debug"`. |
| `unsupported version` | `version` is missing, not an integer, or not `2`. |
| `unsupported message type` | `type` is missing or not `"image"`. |
| `name is missing or not a string` | |
| `name too long` | `name` is over 1024 bytes. |
| `rows out of range` | Missing, not an integer, or outside 1–32768. |
| `cols out of range` | Missing, not an integer, or outside 1–32768. |
| `unsupported depth` | Missing, not a string, or not one of the seven depths. |
| `channels out of range` | Missing, not an integer, or outside 1–4. |
| `live is not a boolean` | `live` is present but not `true` or `false`. |
| `sentAt out of range` | `sentAt` is present but not an integer, or outside 0–253402300799999. |
| `image too large` | The pixel data would exceed 512 MiB. |
| `pixel frame has the wrong size` | The pixel frame's length does not match the header. |
| `could not decode message` | Any other failure while decoding. |
| `viewer busy` | The viewer accepted the message but its display is too far behind (eight images waiting for longer than 200 ms). |
| `too many live channels` | A live frame (`live: true`) for a new channel, while the viewer already shows as many live channels as it can. |
| `viewer paused` | The viewer is paused: it keeps its port but takes no images until resumed. Sent for every request while paused, before any other check. |
| `internal error` | An unexpected failure in the viewer; it keeps running. |

The C++ client reports any error reply as `SendResult::Rejected`.

A reply may be at most 64 KiB; the C++ client refuses anything larger, so whatever answers on the port
cannot push a large message into the host application.

## Versioning rule

- Adding an **optional** field does not change `version`; viewers that do not know it ignore it.
- Changing or removing a field, or changing what a field means, **bumps `version`**.
- Any change updates this document and `docs/user-guide.md` in the same commit.

## Room to grow

Optional fields the format is ready for, left as learning exercises: `source` (file and line of the
`sendImage` call); `stream` (to group the images of one pipeline; a live channel already groups a stream's
frames); `overlays` (rectangles, points or contours drawn over the image). `sentAt` took the place of the
`timestamp` this list once proposed.

## Security

The channel is neither encrypted nor authenticated. The viewer therefore binds to loopback only
(`127.0.0.1`, `[::1]`, `ipc://` or `inproc://`) and refuses any other endpoint unless started with
`--allow-remote`. With `--allow-remote`, any machine that can reach the port can send images to the
viewer, and the images travel in clear text; use it only on a trusted network.

One limit is accepted rather than solved: ZeroMQ delivers a multipart message only once every frame has
arrived and has no limit on the number of frames, so a peer can make the viewer buffer several large
frames before the frame-count check rejects the message. Loopback-only binding limits this to processes
already running on this machine, under any user account: a loopback TCP port is open to every local user,
not only to the one who started the viewer.
