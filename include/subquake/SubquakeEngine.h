#pragma once

#include "subquake/SubquakeDspPrimitives.h"

#include <cstdint>

namespace subquake
{

/** Realtime-safe parameter set for the Subquake low-frequency noise instrument.

    All values are sanitized by setParameters():
    weight [0, 1], cutoffHz [20, 300], decaySeconds [0.02, 8],
    pressure [0, 1], stereoFault [0, 1], outputGain [0, 2].
*/
struct SubquakeParameters
{
    float weight = 0.72f;
    float cutoffHz = 130.0f;
    float decaySeconds = 1.4f;
    float pressure = 0.45f;
    float stereoFault = 0.18f;
    float outputGain = 0.7f;
};

/** Monophonic MIDI-like low-frequency noise instrument.

    noteOn() restarts a velocity-sensitive attack/hold/release envelope. The
    MIDI note number changes deterministic seed material and pressure structure;
    it does not tune the output to 12-TET pitch. processSample() and process()
    allocate no memory and always return finite, ceiling-bounded samples.
*/
class SubquakeEngine
{
public:
    SubquakeEngine();

    /** Sets the sample rate and rebuilds filters; invalid rates fall back to 44.1 kHz. */
    void prepare (double sampleRate) noexcept;

    /** Clears state and sets the deterministic base seed used by future noteOn calls. */
    void reset (std::uint32_t seed = 1u) noexcept;

    /** Applies sanitized parameters and updates filter/envelope coefficients. */
    void setParameters (const SubquakeParameters& parameters) noexcept;

    /** Starts or retriggers the monophonic note, with velocity clamped to [0, 1]. */
    void noteOn (int noteNumber, float velocity) noexcept;

    /** Releases the current note; mismatched note numbers are ignored while another note is held. */
    void noteOff (int noteNumber) noexcept;

    /** Renders one stereo frame. Silent before noteOn and after envelope decay. */
    [[nodiscard]] StereoFrame processSample() noexcept;

    /** Renders numSamples into stereo buffers when both pointers are valid. */
    void process (float* left, float* right, int numSamples) noexcept;

private:
    enum class EnvelopeStage
    {
        idle,
        attack,
        hold,
        release
    };

    struct ClampedParameters
    {
        float weight = 0.72f;
        float cutoffHz = 130.0f;
        float decaySeconds = 1.4f;
        float pressure = 0.45f;
        float stereoFault = 0.18f;
        float outputGain = 0.7f;
    };

    static std::uint32_t mixSeed (std::uint32_t value) noexcept;
    static int clampNote (int noteNumber) noexcept;

    void updateFilters() noexcept;
    void updateEnvelopeRates() noexcept;
    [[nodiscard]] float processEnvelope() noexcept;
    [[nodiscard]] float nextPressureImpulse() noexcept;
    [[nodiscard]] StereoFrame sanitizeFrame (float left, float right) const noexcept;

    ClampedParameters params;
    double sampleRate = 44100.0;
    std::uint32_t baseSeed = 1u;
    int currentNote = -1;

    DeterministicNoise noiseLeft;
    DeterministicNoise noiseRight;
    DeterministicNoise pressureNoise;
    Biquad lowPassLeft;
    Biquad lowPassRight;
    DcBlocker dcLeft;
    DcBlocker dcRight;

    EnvelopeStage envelopeStage = EnvelopeStage::idle;
    float envelope = 0.0f;
    float velocity = 0.0f;
    float attackStep = 1.0f;
    float releaseCoefficient = 0.999f;

    float brownLeft = 0.0f;
    float brownRight = 0.0f;
    float pressureState = 0.0f;
    int pressureCountdown = 1;
    int pressureBaseInterval = 240;
};

} // namespace subquake

