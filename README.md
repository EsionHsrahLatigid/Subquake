# Subquake

Subquake is a YUP-based audio plugin and standalone synth that renders MIDI-triggered low-frequency pressure noise. It builds from this project directory, using the adjacent `../yup` checkout when present.

## Identity

- App ID: `audio.2bit.subquake`
- Plugin ID: `audio.2bit.Subquake`
- AU subtype: `SbQk`
- Vendor: `2bit`
- Type: stereo-output synth accepting MIDI input
- macOS formats: Standalone, VST3, AUv2
- Windows formats: Standalone, VST3

## Standalone Controls

The built-in editor includes a momentary `Trigger` control for the Standalone app. Press and hold the button, press and hold Space while the editor has keyboard focus, or hold both; the editor publishes the combined gate state to the processor. External MIDI input is still accepted and uses the same monophonic engine path.

The editor also shows a lightweight output activity meter. Trigger edges and meter values move through processor-owned atomics, so realtime rendering stays lock-free and allocation-free.

## Build

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug
ctest --preset engine-debug
```

```sh
cmake --preset plugin-release
cmake --build --preset plugin-release
ctest --preset plugin-release
```

Release bundles are generated under `build/plugin-release` by YUP's plugin targets:

- `subquake_release_bundles`
- `subquake_standalone_plugin`
- `subquake_vst3_plugin`
- `subquake_au_plugin` on Apple platforms

On macOS, the local bundle paths are:

- `build/plugin-release/subquake_standalone_plugin.app`
- `build/plugin-release/VST3/Release/subquake_vst3_plugin.vst3`
- `build/plugin-release/subquake_au_plugin.component`

## CI

`.github/workflows/ci.yml` builds Debug engine tests on macOS arm64 and Windows x64, then builds Release plugin bundles. It uploads `Subquake-latest-macos-arm64.zip` and `Subquake-latest-windows-x64.zip`. On `v*` tags, a release job downloads both artifacts, renames them with the tag version, and creates one GitHub Release with both ZIPs attached.

## Layout

- `include/subquake/` contains the realtime-safe DSP engine API and local DSP primitives.
- `source/` contains the engine implementation and YUP plugin/editor/state wrapper.
- `tests/` contains deterministic engine regression tests and a plugin-wrapper bridge test for the built-in synthetic trigger.
- `cmake/` contains the project-local macOS icon conversion workaround used by the standalone target.
