# ZeroFG source snapshot

This directory contains the ZeroFG engine **v1.1.0**, published at
https://github.com/hy300leosquizz-ctrl/ZeroFG (tag `v1.1.0`, commit
`2b7711e47680c25dd461110c182ee5dd4a637f99`). The engine sources, public headers,
shader sources, shipped SPIR-V headers, shader compiler and CMake file are that
commit's, byte for byte. No laboratory recordings, runtime binaries, device
identifiers or experimental environment controls are included.

The host adapter in `integration/xenia/` is the thin adapter from the same
release's `host/xendroid/` directory. Xendroid's presenter owns recording order,
queue submission, synchronization, presentation and resource lifetimes.

Product mapping in this integration:

| User choice | Core mode |
|---|---|
| Off | Native Xendroid+ presentation |
| ReallyZero | `zerofg::Mode::kReallyZero` (the dense optical flow) |
| Zero | `zerofg::Mode::kZero` (RC7, the layered engine) |

The integration names them `Mode::kDenseFlow` and `Mode::kRC7`, the same engines
under their development names. One interpolator serves the selected mode, with
per-flight descriptors. Recordings are submitted serially in their recording
order; an abandoned recording cannot be followed by a retry using its retained
history. Mode changes drain the old session before recreating generation state.

The engine is Apache-2.0; see `LICENSE`. Xenia-derived host integration files
outside this directory retain their original notices.
