#include "subquake/SubquakeEngine.h"

#include <algorithm>
#include <cmath>

namespace subquake
{

namespace
{
constexpr float ceiling = 0.98f;
constexpr float attackSeconds = 0.003f;
}

SubquakeEngine::SubquakeEngine()
{
    prepare (44100.0);
    reset (1u);
}

void SubquakeEngine::prepare (double newSampleRate) noexcept
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 1.0 ? newSampleRate : 44100.0;
    updateFilters();
    updateEnvelopeRates();
}

void SubquakeEngine::reset (std::uint32_t seed) noexcept
{
    baseSeed = seed != 0u ? seed : 1u;
    currentNote = -1;
    envelopeStage = EnvelopeStage::idle;
    envelope = 0.0f;
    velocity = 0.0f;
    brownLeft = 0.0f;
    brownRight = 0.0f;
    pressureState = 0.0f;
    pressureCountdown = 1;

    noiseLeft.reset (mixSeed (baseSeed ^ 0x2c1b3c6du));
    noiseRight.reset (mixSeed (baseSeed ^ 0x9e3779b9u));
    pressureNoise.reset (mixSeed (baseSeed ^ 0x85ebca6bu));
    lowPassLeft.reset();
    lowPassRight.reset();
    dcLeft.reset();
    dcRight.reset();
}

void SubquakeEngine::setParameters (const SubquakeParameters& parameters) noexcept
{
    params.weight = clampFinite (parameters.weight, 0.0f, 1.0f, SubquakeParameters {}.weight);
    params.cutoffHz = clampFinite (parameters.cutoffHz, 20.0f, 300.0f, SubquakeParameters {}.cutoffHz);
    params.decaySeconds = clampFinite (parameters.decaySeconds, 0.02f, 8.0f, SubquakeParameters {}.decaySeconds);
    params.pressure = clampFinite (parameters.pressure, 0.0f, 1.0f, SubquakeParameters {}.pressure);
    params.stereoFault = clampFinite (parameters.stereoFault, 0.0f, 1.0f, SubquakeParameters {}.stereoFault);
    params.outputGain = clampFinite (parameters.outputGain, 0.0f, 2.0f, SubquakeParameters {}.outputGain);

    updateFilters();
    updateEnvelopeRates();
}

void SubquakeEngine::noteOn (int noteNumber, float newVelocity) noexcept
{
    const auto incomingNote = clampNote (noteNumber);
    const auto incomingVelocity = clampFinite (newVelocity, 0.0f, 1.0f, 1.0f);

    if (incomingVelocity <= 0.0f)
    {
        noteOff (incomingNote);
        return;
    }

    currentNote = incomingNote;
    velocity = incomingVelocity;

    const auto noteSeed = mixSeed (baseSeed ^ (static_cast<std::uint32_t> (currentNote) * 0x45d9f3bu));
    noiseLeft.reset (mixSeed (noteSeed ^ 0x3c6ef372u));
    noiseRight.reset (mixSeed (noteSeed ^ 0xbb67ae85u));
    pressureNoise.reset (mixSeed (noteSeed ^ 0xa54ff53au));

    brownLeft = 0.0f;
    brownRight = 0.0f;
    pressureState = 0.0f;
    pressureCountdown = 1;

    const auto structuralBucket = static_cast<int> ((noteSeed >> 24u) & 63u);
    pressureBaseInterval = std::max (24, static_cast<int> (sampleRate / static_cast<double> (22 + structuralBucket)));

    envelope = 0.0f;
    envelopeStage = EnvelopeStage::attack;
    lowPassLeft.reset();
    lowPassRight.reset();
    dcLeft.reset();
    dcRight.reset();
}

void SubquakeEngine::noteOff (int noteNumber) noexcept
{
    const auto safeNote = clampNote (noteNumber);
    if (currentNote == safeNote && envelopeStage != EnvelopeStage::idle)
        envelopeStage = EnvelopeStage::release;
}

StereoFrame SubquakeEngine::processSample() noexcept
{
    const auto envelopeValue = processEnvelope();
    if (envelopeValue <= 0.0f)
        return {};

    const auto sourceStep = 0.0015f + params.weight * 0.012f;
    brownLeft = std::clamp (brownLeft * 0.9988f + noiseLeft.nextFloat() * sourceStep, -1.3f, 1.3f);
    brownRight = std::clamp (brownRight * 0.9984f + noiseRight.nextFloat() * sourceStep, -1.3f, 1.3f);

    const auto pressure = nextPressureImpulse();
    const auto monoSource = (brownLeft + brownRight) * 0.5f + pressure;
    const auto fault = params.stereoFault;
    const auto leftSource = monoSource * (1.0f - 0.35f * fault) + brownLeft * fault + pressure * 0.12f * fault;
    const auto rightSource = monoSource * (1.0f - 0.35f * fault) + brownRight * fault - pressure * 0.10f * fault;

    const auto left = dcLeft.process (lowPassLeft.process (leftSource)) * envelopeValue * params.outputGain;
    const auto right = dcRight.process (lowPassRight.process (rightSource)) * envelopeValue * params.outputGain;

    return sanitizeFrame (left, right);
}

void SubquakeEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto frame = processSample();
        left[i] = frame.left;
        right[i] = frame.right;
    }
}

std::uint32_t SubquakeEngine::mixSeed (std::uint32_t value) noexcept
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value != 0u ? value : 0x6d2b79f5u;
}

int SubquakeEngine::clampNote (int noteNumber) noexcept
{
    return std::clamp (noteNumber, 0, 127);
}

void SubquakeEngine::updateFilters() noexcept
{
    lowPassLeft.setLowPass (sampleRate, params.cutoffHz, 0.62f);
    lowPassRight.setLowPass (sampleRate, params.cutoffHz * (1.0f + 0.04f * params.stereoFault), 0.62f);
    dcLeft.prepare (sampleRate, 3.0f);
    dcRight.prepare (sampleRate, 3.0f);
}

void SubquakeEngine::updateEnvelopeRates() noexcept
{
    attackStep = 1.0f / static_cast<float> (std::max (1.0, sampleRate * static_cast<double> (attackSeconds)));
    releaseCoefficient = std::exp (-6.90775527898f / static_cast<float> (std::max (1.0, sampleRate * static_cast<double> (params.decaySeconds))));
}

float SubquakeEngine::processEnvelope() noexcept
{
    if (envelopeStage == EnvelopeStage::attack)
    {
        envelope += attackStep;
        if (envelope >= 1.0f)
        {
            envelope = 1.0f;
            envelopeStage = EnvelopeStage::hold;
        }
    }
    else if (envelopeStage == EnvelopeStage::release)
    {
        envelope *= releaseCoefficient;
        if (envelope < 1.0e-5f)
        {
            envelope = 0.0f;
            velocity = 0.0f;
            currentNote = -1;
            envelopeStage = EnvelopeStage::idle;
        }
    }

    return envelope * velocity;
}

float SubquakeEngine::nextPressureImpulse() noexcept
{
    pressureState *= 0.992f;

    if (--pressureCountdown <= 0)
    {
        const auto jitter = static_cast<int> (pressureNoise.nextWord() % static_cast<std::uint32_t> (std::max (1, pressureBaseInterval)));
        pressureCountdown = std::max (8, pressureBaseInterval / 2 + jitter);
        pressureState += pressureNoise.nextBinary() * params.pressure * (0.16f + 0.24f * params.weight);
        pressureState = std::clamp (pressureState, -0.8f, 0.8f);
    }

    return pressureState;
}

StereoFrame SubquakeEngine::sanitizeFrame (float left, float right) const noexcept
{
    const auto safeLeft = boundedDrive (left, 1.35f);
    const auto safeRight = boundedDrive (right, 1.35f);
    return { std::clamp (safeLeft, -ceiling, ceiling),
             std::clamp (safeRight, -ceiling, ceiling) };
}

} // namespace subquake

