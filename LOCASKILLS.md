# Established technical knowledge

- SetupDi HID enumeration can open metadata with desired access 0 and shared
  read/write. Per-interface errors must not abort unrelated enumeration results.
- The connected 06A3:075C PS28 exposes a Windows HID input buffer of 15 bytes,
  despite the user-supplied endpoint maximum of 16. HIDP_CAPS is authoritative for
  ReadFile buffer sizing. Report byte 0 was observed as zero.
- HidP_GetUsageValue uses usage page, usage and link collection from value caps.
  LogicalMin determines whether to sign-extend BitSize. LogicalMax's unsigned
  interpretation requires care when LogicalMin is nonnegative.
- HidP_GetUsages returns currently pressed usages. Rebuild the current report's
  button values so released buttons do not remain stuck. Other report IDs must
  retain separate state and freshness.
- The observed hat range is 1..8 with HasNull; raw zero is a neutral null. It is not
  an invalid report. No physical hat identity has been inferred from this alone.
- CancelIoEx requests cancellation; GetOverlappedResult(wait=true) drains completion
  before releasing the OVERLAPPED/buffer. Cancellation is not a hardware reset.
- UTF-8 persisted JSON plus Unicode Win32 paths support non-ASCII user directories.
- A syntactically valid HID report does not establish stick-side electrical health.
  Marked fault intervals and separately learned groups are needed to test continued
  throttle activity. No PS28 fault signature has yet been established.
- Build intermediates use MSBuildProjectName: ProjectName is unavailable when the
  early Directory.Build.props is evaluated on this installed toolchain.
- Physical catalog IDs identify hardware features, not HID usage numbers. Whole
  hats may be scalar values; directional hats may appear as separate buttons.
  USER_ASSIGNED and OBSERVED links are not verified mappings.
- GDI+ resource image decoding needs objidl.h before gdiplus.h with lean Windows
  headers. Keep the source IStream alive for the Bitmap lifetime, and destroy
  bitmaps before GdiplusShutdown. Five PNGs are embedded so installed paths do not
  affect offline identification photos.

Sources for API semantics are linked in docs/architecture.md. Raw physical findings
must be recorded separately from descriptor semantics and hypotheses.

- The installed X52 driver 8.0.116.0 still uses 06A3:075C, but Windows names its
  nodes X52 H.O.T.A.S. (USB)/(HID). Manufacturer PR0 control definitions are in
  System32/SaiD075C.pr0. They are vendor declarations, not dropout evidence.
- The stock PR0 and profiler 8.0.213.0's sample use UTF-16LE with BOM. Sample
  version is 5; stock template is version 3. Trigger-to-Space uses keyboard page 7,
  usage 0x2C, value 1 in an actioncommand/actionblock.
- Battlefield keyboard button numbers are scan codes, not USB HID usages.
  Translate only the supported set; keep Logitech mouse defaults and fallbacks.
