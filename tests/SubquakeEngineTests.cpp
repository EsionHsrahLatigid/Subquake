#include "subquake/SubquakeEngine.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

using subquake::SubquakeEngine;
using subquake::SubquakeParameters;

namespace
{

std::vector<float> renderNote (std::uint32_t seed, int note, float velocity, SubquakeParameters params, int samples)
{
    SubquakeEngine engine;
    engine.prepare (48000.0);
    engine.setParameters (params);
    engine.reset (seed);
    engine.noteOn (note, velocity);

    std::vector<float> output;
    output.reserve (static_cast<std::size_t> (samples));
    for (int i = 0; i < samples; ++i)
        output.push_back (engine.processSample().left);

    return output;
}

float sumSquares (const std::vector<float>& samples)
{
    float energy = 0.0f;
    for (const auto sample : samples)
        energy += sample * sample;
    return energy;
}

float lowBandEnergy (const std::vector<float>& samples, float sampleRate)
{
    const auto coefficient = std::exp (-2.0f * 3.14159265358979323846f * 180.0f / sampleRate);
    float state = 0.0f;
    float energy = 0.0f;

    for (const auto sample : samples)
    {
        state = (1.0f - coefficient) * sample + coefficient * state;
        energy += state * state;
    }

    return energy;
}

float highBandEnergy (const std::vector<float>& samples, float sampleRate)
{
    const auto coefficient = std::exp (-2.0f * 3.14159265358979323846f * 1000.0f / sampleRate);
    float low = 0.0f;
    float energy = 0.0f;

    for (const auto sample : samples)
    {
        low = (1.0f - coefficient) * sample + coefficient * low;
        const auto high = sample - low;
        energy += high * high;
    }

    return energy;
}

void testSilentBeforeTrigger()
{
    SubquakeEngine engine;
    engine.prepare (48000.0);
    engine.reset (123u);

    for (int i = 0; i < 1024; ++i)
    {
        const auto frame = engine.processSample();
        assert (frame.left == 0.0f);
        assert (frame.right == 0.0f);
    }
}

void testDeterministicSameEvents()
{
    SubquakeParameters params;
    params.weight = 0.83f;
    params.pressure = 0.62f;

    const auto a = renderNote (4242u, 36, 0.8f, params, 4096);
    const auto b = renderNote (4242u, 36, 0.8f, params, 4096);

    assert (a == b);
}

void testNoteChangesStructureButNotPitch()
{
    SubquakeParameters params;
    params.cutoffHz = 115.0f;

    const auto lowNote = renderNote (99u, 24, 1.0f, params, 4096);
    const auto highNote = renderNote (99u, 84, 1.0f, params, 4096);

    int different = 0;
    for (std::size_t i = 0; i < lowNote.size(); ++i)
        different += lowNote[i] != highNote[i] ? 1 : 0;

    assert (different > 3500);
    assert (lowBandEnergy (lowNote, 48000.0f) > highBandEnergy (lowNote, 48000.0f) * 6.0f);
    assert (lowBandEnergy (highNote, 48000.0f) > highBandEnergy (highNote, 48000.0f) * 6.0f);
}

void testNoteOffDecaysToSilence()
{
    SubquakeParameters params;
    params.decaySeconds = 0.04f;
    params.outputGain = 1.0f;

    SubquakeEngine engine;
    engine.prepare (48000.0);
    engine.setParameters (params);
    engine.reset (777u);
    engine.noteOn (40, 1.0f);

    for (int i = 0; i < 1024; ++i)
        (void) engine.processSample();

    engine.noteOff (40);

    std::vector<float> tail;
    tail.reserve (12000);
    for (int i = 0; i < 12000; ++i)
        tail.push_back (engine.processSample().left);

    assert (sumSquares (tail) > 0.0001f);
    for (int i = 0; i < 2048; ++i)
    {
        const auto frame = engine.processSample();
        assert (std::fabs (frame.left) < 1.0e-4f);
        assert (std::fabs (frame.right) < 1.0e-4f);
    }
}

void testFiniteBoundedExtremeParameters()
{
    SubquakeParameters params;
    params.weight = 1000.0f;
    params.cutoffHz = 1000000.0f;
    params.decaySeconds = 1000000.0f;
    params.pressure = 1000.0f;
    params.stereoFault = 1000.0f;
    params.outputGain = 1000.0f;

    SubquakeEngine engine;
    engine.prepare (0.0);
    engine.setParameters (params);
    engine.reset (0u);
    engine.noteOn (999, 1000.0f);

    for (int i = 0; i < 8192; ++i)
    {
        const auto frame = engine.processSample();
        assert (std::isfinite (frame.left));
        assert (std::isfinite (frame.right));
        assert (frame.left >= -0.9801f && frame.left <= 0.9801f);
        assert (frame.right >= -0.9801f && frame.right <= 0.9801f);
    }
}

void testNonFiniteParametersFallbackSafely()
{
    SubquakeParameters params;
    params.weight = std::numeric_limits<float>::quiet_NaN();
    params.cutoffHz = std::numeric_limits<float>::infinity();
    params.decaySeconds = -std::numeric_limits<float>::infinity();
    params.pressure = std::numeric_limits<float>::quiet_NaN();
    params.stereoFault = std::numeric_limits<float>::infinity();
    params.outputGain = std::numeric_limits<float>::quiet_NaN();

    SubquakeEngine engine;
    engine.prepare (std::numeric_limits<double>::infinity());
    engine.setParameters (params);
    engine.reset (31337u);
    engine.noteOn (-100, std::numeric_limits<float>::infinity());

    bool sawEnergy = false;
    for (int i = 0; i < 4096; ++i)
    {
        const auto frame = engine.processSample();
        assert (std::isfinite (frame.left));
        assert (std::isfinite (frame.right));
        assert (frame.left >= -0.9801f && frame.left <= 0.9801f);
        assert (frame.right >= -0.9801f && frame.right <= 0.9801f);
        sawEnergy = sawEnergy || std::fabs (frame.left) > 1.0e-6f || std::fabs (frame.right) > 1.0e-6f;
    }

    assert (sawEnergy);
}

void testLowFrequencySignature()
{
    SubquakeParameters params;
    params.weight = 1.0f;
    params.cutoffHz = 150.0f;
    params.pressure = 0.35f;
    params.outputGain = 0.9f;

    const auto samples = renderNote (5150u, 48, 1.0f, params, 48000);
    const auto low = lowBandEnergy (samples, 48000.0f);
    const auto high = highBandEnergy (samples, 48000.0f);

    assert (sumSquares (samples) > 0.1f);
    assert (low > high * 8.0f);
}

void testVelocityZeroActsSilent()
{
    SubquakeEngine engine;
    engine.prepare (48000.0);
    engine.reset (4u);
    engine.noteOn (44, 0.0f);

    for (int i = 0; i < 1024; ++i)
    {
        const auto frame = engine.processSample();
        assert (frame.left == 0.0f);
        assert (frame.right == 0.0f);
    }
}

void testMismatchedVelocityZeroDoesNotReleaseHeldNote()
{
    SubquakeEngine engine;
    engine.prepare (48000.0);
    engine.reset (123u);
    engine.noteOn (40, 1.0f);
    for (int i = 0; i < 256; ++i)
        (void) engine.processSample();

    engine.noteOn (41, 0.0f);
    float energy = 0.0f;
    for (int i = 0; i < 2048; ++i)
    {
        const auto frame = engine.processSample();
        energy += frame.left * frame.left + frame.right * frame.right;
    }
    assert (energy > 0.001f);
}

void testVeryLowSampleRateFallsBackSafely()
{
    SubquakeEngine engine;
    engine.prepare (1.5);
    engine.noteOn (40, 1.0f);
    for (int i = 0; i < 512; ++i)
    {
        const auto frame = engine.processSample();
        assert (std::isfinite (frame.left));
        assert (std::isfinite (frame.right));
    }
}

} // namespace

int main()
{
    testSilentBeforeTrigger();
    testDeterministicSameEvents();
    testNoteChangesStructureButNotPitch();
    testNoteOffDecaysToSilence();
    testFiniteBoundedExtremeParameters();
    testNonFiniteParametersFallbackSafely();
    testLowFrequencySignature();
    testVelocityZeroActsSilent();
    testMismatchedVelocityZeroDoesNotReleaseHeldNote();
    testVeryLowSampleRateFallsBackSafely();

    std::cout << "SubquakeEngineTests passed\n";
    return 0;
}

