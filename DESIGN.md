# Subquake Design

Subquake is a focused low-frequency pressure-noise instrument. MIDI notes retrigger a monophonic envelope and deterministic noise structure; note number changes the texture seed and pressure pattern, not a pitched oscillator frequency.

## Product Shape

- Plugin formats: Standalone and VST3 on macOS and Windows; AUv2 additionally on macOS.
- Synth behavior: accepts MIDI, produces stereo audio, one active voice.
- Stable identity: `audio.2bit.subquake`, `audio.2bit.Subquake`, AU subtype `SbQk`.
- State format: parameter ID/value pairs with a `SQK1` magic header and version `1`.

## DSP Contract

- The engine is allocation-free during audio rendering.
- All public parameters are sanitized before use.
- Output samples remain finite and hard bounded by the final safety stage.
- Rendering is deterministic for identical seed, note, velocity, parameter, and sample-rate inputs.
- Before a note trigger and after release decay, the engine renders silence.

## UI Contract

The current editor is a YUP parameter grid with direct host-bound controls plus a small Standalone-oriented performance strip.

- The momentary `Trigger` button and Space-key gate are UI commands only; they publish a combined desired gate plus monotonic edge counter through processor-owned atomics. The audio thread consumes pending edges before rendering, including rapid press/release pairs that happen between callbacks.
- Mouse and Space holds are tracked separately by the editor. Closing the editor or losing focus fail-safe releases the UI gate without interfering with external MIDI.
- External MIDI remains supported and is not converted through the UI trigger path.
- The output activity meter is display-only. The audio thread stores a per-block peak as a scaled atomic integer, and the UI timer polls/decays it for drawing.
- The editor still intentionally avoids product graphics until a dedicated Subquake visual language is implemented.
