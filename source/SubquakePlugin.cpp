#include "SubquakePlugin.h"

#include "ParameterGridEditor.h"
#include "ProductState.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace subquake::plugin
{
namespace
{
constexpr std::array<char, 4> stateMagic {{ 'S', 'Q', 'K', '1' }};
constexpr int stateVersion = 1;
constexpr int controlUpdatePeriod = 16;

yup::NormalisableRange<float> makeCutoffRange()
{
    auto range = yup::NormalisableRange<float> (20.0f, 300.0f);
    range.setSkewForCentre (110.0f);
    return range;
}

yup::NormalisableRange<float> makeDecayRange()
{
    auto range = yup::NormalisableRange<float> (0.02f, 8.0f);
    range.setSkewForCentre (1.0f);
    return range;
}

constexpr std::array<std::array<float, 6>, 4> presetValues {{
    {{ 0.72f, 130.0f, 1.40f, 0.45f, 0.18f, 0.70f }},
    {{ 0.88f, 82.0f, 2.70f, 0.62f, 0.36f, 0.78f }},
    {{ 0.55f, 210.0f, 0.48f, 0.91f, 0.12f, 0.62f }},
    {{ 0.96f, 58.0f, 5.80f, 0.30f, 0.74f, 0.84f }}
}};

float sanitizeVelocity (const yup::MidiMessage& message) noexcept
{
    return std::clamp (message.getFloatVelocity(), 0.0f, 1.0f);
}
} // namespace

SubquakePlugin::SubquakePlugin()
    : yup::AudioProcessor ("Subquake",
                           yup::AudioBusLayout ({
                                                    yup::AudioBus ("midi", yup::AudioBus::Midi, yup::AudioBus::Input, 1),
                                                },
                                                {
                                                    yup::AudioBus ("main", yup::AudioBus::Audio, yup::AudioBus::Output, 2),
                                                }))
{
    parameters[weight] = yup::AudioParameterBuilder()
                             .withID ("weight")
                             .withName ("Weight")
                             .withHostID (weight)
                             .withRange (0.0f, 1.0f)
                             .withDefault (presetValues[0][weight])
                             .withSmoothing (20.0f)
                             .withModulatable (true)
                             .build();
    parameters[cutoff] = yup::AudioParameterBuilder()
                             .withID ("cutoff")
                             .withName ("Cutoff")
                             .withHostID (cutoff)
                             .withRange (makeCutoffRange())
                             .withDefault (presetValues[0][cutoff])
                             .withSmoothing (35.0f)
                             .withModulatable (true)
                             .withUnit (yup::AudioParameter::ParameterUnit::Hertz)
                             .build();
    parameters[decay] = yup::AudioParameterBuilder()
                            .withID ("decay")
                            .withName ("Decay")
                            .withHostID (decay)
                            .withRange (makeDecayRange())
                            .withDefault (presetValues[0][decay])
                            .withSmoothing (45.0f)
                            .withModulatable (true)
                            .build();
    parameters[pressure] = yup::AudioParameterBuilder()
                               .withID ("pressure")
                               .withName ("Pressure")
                               .withHostID (pressure)
                               .withRange (0.0f, 1.0f)
                               .withDefault (presetValues[0][pressure])
                               .withSmoothing (20.0f)
                               .withModulatable (true)
                               .build();
    parameters[stereoFault] = yup::AudioParameterBuilder()
                                  .withID ("stereo_fault")
                                  .withName ("Stereo Fault")
                                  .withHostID (stereoFault)
                                  .withRange (0.0f, 1.0f)
                                  .withDefault (presetValues[0][stereoFault])
                                  .withSmoothing (25.0f)
                                  .withModulatable (true)
                                  .build();
    parameters[output] = yup::AudioParameterBuilder()
                             .withID ("output")
                             .withName ("Output")
                             .withHostID (output)
                             .withRange (0.0f, 2.0f)
                             .withDefault (presetValues[0][output])
                             .withSmoothing (30.0f)
                             .withModulatable (true)
                             .build();

    for (const auto& parameter : parameters)
        addParameter (parameter);
}

void SubquakePlugin::prepareToPlay (const yup::AudioSpec& spec)
{
    engine.prepare (spec.sampleRate);

    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
    {
        parameterHandles[i] = yup::AudioParameterHandle (*parameters[i], spec.sampleRate);
        smoothedValues[i] = parameters[i]->getValue();
    }

    engine.reset();
    applyEngineParameters();
    resetPerformanceState();
}

void SubquakePlugin::releaseResources()
{
}

void SubquakePlugin::processBlock (yup::AudioProcessContext<float>& context)
{
    auto& audio = context.audio;
    const auto numSamples = audio.getNumSamples();
    const auto numChannels = audio.getNumChannels();

    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
        parameterHandles[i].prepareBlock (context.params, parameters[i]->getIndexInContainer());

    auto midi = context.midi.begin();
    const auto midiEnd = context.midi.end();
    auto* left = numChannels > 0 ? audio.getWritePointer (0) : nullptr;
    auto* right = numChannels > 1 ? audio.getWritePointer (1) : nullptr;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        while (midi != midiEnd && (*midi).samplePosition <= sample)
        {
            const auto& message = (*midi).getMessage();
            if (message.isNoteOn())
            {
                activeNote = std::clamp (message.getNoteNumber(), 0, 127);
                engine.noteOn (activeNote, sanitizeVelocity (message));
            }
            else if (message.isNoteOff())
            {
                const auto note = std::clamp (message.getNoteNumber(), 0, 127);
                if (note == activeNote)
                {
                    engine.noteOff (note);
                    activeNote = -1;
                }
            }
            ++midi;
        }

        advanceParameterHandles (sample);
        if (controlUpdateCountdown <= 0)
        {
            applyEngineParameters();
            controlUpdateCountdown = controlUpdatePeriod;
        }
        --controlUpdateCountdown;

        const auto frame = engine.processSample();

        if (left != nullptr)
            left[sample] = frame.left;
        if (right != nullptr)
            right[sample] = frame.right;

        for (int channel = 2; channel < numChannels; ++channel)
            audio.getWritePointer (channel)[sample] = 0.0f;
    }

    context.midi.clear();
}

void SubquakePlugin::flush()
{
    engine.reset();
    resetPerformanceState();
}

bool SubquakePlugin::acceptsMidi() const noexcept
{
    return true;
}

int SubquakePlugin::getNumVoices() const
{
    return 1;
}

int SubquakePlugin::getCurrentPreset() const noexcept
{
    return currentPreset.load (std::memory_order_relaxed);
}

void SubquakePlugin::setCurrentPreset (int index) noexcept
{
    if (! yup::isPositiveAndBelow (index, static_cast<int> (presetValues.size())))
        return;

    currentPreset.store (index, std::memory_order_relaxed);
    for (std::size_t i = 0; i < parameters.size(); ++i)
        parameters[i]->setValue (presetValues[static_cast<std::size_t> (index)][i]);
}

int SubquakePlugin::getNumPresets() const
{
    return static_cast<int> (presetNames.size());
}

yup::String SubquakePlugin::getPresetName (int index) const
{
    if (yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size())))
        return presetNames[static_cast<std::size_t> (index)];
    return "Invalid Preset";
}

void SubquakePlugin::setPresetName (int index, yup::StringRef newName)
{
    if (yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size())))
        presetNames[static_cast<std::size_t> (index)] = newName;
}

yup::Result SubquakePlugin::loadStateFromMemory (const yup::MemoryBlock& data)
{
    auto presetIndex = currentPreset.load (std::memory_order_relaxed);
    const auto result = loadProductState (*this, data, stateMagic, stateVersion, getNumPresets(), presetIndex);
    if (result.wasOk())
        currentPreset.store (presetIndex, std::memory_order_relaxed);
    return result;
}

yup::Result SubquakePlugin::saveStateIntoMemory (yup::MemoryBlock& data)
{
    return saveProductState (*this, data, stateMagic, stateVersion, currentPreset.load (std::memory_order_relaxed));
}

bool SubquakePlugin::hasEditor() const
{
    return true;
}

yup::AudioProcessorEditor* SubquakePlugin::createEditor()
{
    return new ParameterGridEditor (*this,
                                    "Subquake",
                                    "Generic parameter editor: product graphics and metering are not implemented in this target.",
                                    0xfff05a28u);
}

void SubquakePlugin::advanceParameterHandles (int samplePosition) noexcept
{
    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
    {
        parameterHandles[i].advanceToSample (samplePosition);
        smoothedValues[i] = parameterHandles[i].getNextValue();
    }
}

void SubquakePlugin::applyEngineParameters() noexcept
{
    subquake::SubquakeParameters engineParameters;
    engineParameters.weight = smoothedValues[weight];
    engineParameters.cutoffHz = smoothedValues[cutoff];
    engineParameters.decaySeconds = smoothedValues[decay];
    engineParameters.pressure = smoothedValues[pressure];
    engineParameters.stereoFault = smoothedValues[stereoFault];
    engineParameters.outputGain = smoothedValues[output];
    engine.setParameters (engineParameters);
}

void SubquakePlugin::resetPerformanceState() noexcept
{
    activeNote = -1;
    controlUpdateCountdown = 0;
}

} // namespace subquake::plugin

extern "C" yup::AudioProcessor* createPluginProcessor()
{
    return new subquake::plugin::SubquakePlugin();
}

