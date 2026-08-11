# Subquake

Subquake is a YUP-based audio plugin and standalone synth that renders MIDI-triggered low-frequency pressure noise. It builds from this project directory, using the adjacent `../yup` checkout when present.

## Identity

- App ID: `jp.ehl.subquake`
- Plugin ID: `jp.ehl.subquake`
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

Release bundles are staged under the stable `artifacts/plugin-release/<platform-arch>/` tree. `build/` is CMake's internal workspace:

- `subquake_release_bundles`
- `subquake_standalone_plugin`
- `subquake_vst3_plugin`
- `subquake_au_plugin` on Apple platforms

On macOS, the local bundle paths are:

- `artifacts/plugin-release/macos-arm64/standalone/subquake_standalone_plugin.app`
- `artifacts/plugin-release/macos-arm64/vst3/subquake_vst3_plugin.vst3`
- `artifacts/plugin-release/macos-arm64/au/subquake_au_plugin.component`

Windows uses `artifacts/plugin-release/windows-x64/` with `standalone/` and `vst3/` directories.

## CI

`.github/workflows/ci.yml` is the required CI entrypoint for pushes to `main`, pull requests, and manual runs. A lightweight Linux classifier always runs. Changes limited to `README.md`, `DESIGN.md`, `LICENSE`, `docs/**`, or `.github/ISSUE_TEMPLATE/**` skip the heavy jobs; every other change runs Debug tests and Release bundle builds on macOS arm64 and Windows x64. Manual dispatches default to forcing both heavy jobs.

Successful heavy runs upload two immutable, 14-day artifacts:

- `Subquake-latest-macos-arm64`, containing `Subquake-latest-macos-arm64.zip` and `SHA256SUMS.txt`
- `Subquake-latest-windows-x64`, containing `Subquake-latest-windows-x64.zip` and `SHA256SUMS.txt`

`.github/workflows/release.yml` is the only `v*` tag workflow. It performs no compilation. The Ubuntu release job resolves lightweight or annotated tags to a commit, requires the tag version to match the CMake project version, requires one successful `CI` push run on `main` for that exact SHA, downloads exactly the two expected artifacts, verifies their SHA-256 manifests and ZIP integrity, then publishes versioned assets such as `Subquake-0.2.1-macos-arm64.zip` and `Subquake-0.2.1-windows-x64.zip`. Publication uses a draft release whose asset list is sanitized and rechecked to contain exactly those two ZIPs. A missing, expired, ambiguous, or mismatched provenance chain fails closed.

Release operator sequence: merge or push the version commit to `main`, wait for both platform jobs and `CI Summary` to pass, then create and push the version tag. GitHub CLI 2.x or newer is required by the release runner. Never move or reuse a published tag; correct the source and use the next patch version instead.

## Layout

- `include/subquake/` contains the realtime-safe DSP engine API and local DSP primitives.
- `source/` contains the engine implementation and YUP plugin/editor/state wrapper.
- `tests/` contains deterministic engine regression tests and a plugin-wrapper bridge test for the built-in synthetic trigger.
- `cmake/` contains the project-local macOS icon conversion workaround used by the standalone target.
