# Former names

For reading commits from before October 2026. The README's
[From Slovak to English](../README.md#from-slovak-to-english) tells the story; this is the lookup table.

## The rename to ID Vision Lab (October 2026)

Until October 2026 the repository was `Image_processing` and the CMake project `ImageProcessing`, with
options `IMAGE_PROCESSING_*`, functions `image_processing_*` and the define `IMAGE_PROCESSING_VERSION`; they
are now `IdVisionLab`, `ID_VISION_LAB_*`, `id_vision_lab_*` and `ID_VISION_LAB_VERSION`.

Until October 2026 the apps were "Image Processing" in `apps/image_processing` (now `apps/viewer`) and
"Image Processing Client" in `apps/client` (now `apps/example_sender`); their tests were `qt_ui_tests` (now
`viewer_ui_tests` and `qt_common_tests`) and `client_ui_tests` (now `example_sender_ui_tests`).

## The 2013 thesis code's Slovak names

| 2013 | Now |
|---|---|
| `klient` | the client, now the Visual Debug Example Sender |
| `Obrazky.qrc` | `resources.qrc` |
| `/zakladne` | `/icons` |
| `TYP_UDAJOV` | the header's `type` |
| `nazov` | `name` |
| `sirka`, `vyska` | `cols`, `rows` |
| `lbl_Okno` | `ImageView` |
| `lbl_obrazok` | `previewLabel` |
| `menuNazov` | `menuFile` |
| `môj obrázok č.` | `<file name> #<n>` |
| `my_main_qlabel` | `ImageView` and `viewer_core::Viewport` |
| `My_label` | `ThumbnailPanel` |
| `mythread` | `visual_debug::Server` and `QtImageSink` |
