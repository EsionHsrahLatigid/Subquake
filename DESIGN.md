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

The current editor is a generic YUP parameter grid with direct host-bound controls. It intentionally avoids product graphics and metering until a dedicated Subquake visual language is implemented.
