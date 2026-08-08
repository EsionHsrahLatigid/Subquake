#pragma once

#include "subquake/SubquakeEngine.h"

#include <yup_audio_processors/yup_audio_processors.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace subquake::plugin
{

class SubquakePlugin final : public yup::AudioProcessor
{
public:
    SubquakePlugin();

    void prepareToPlay (const yup::AudioSpec& spec) override;
    void releaseResources() override;
    void processBlock (yup::AudioProcessContext<float>& context) override;
    void flush() override;

    bool acceptsMidi() const noexcept override;
    int getNumVoices() const override;

    int getCurrentPreset() const noexcept override;
    void setCurrentPreset (int index) noexcept override;
    int getNumPresets() const override;
    yup::String getPresetName (int index) const override;
    void setPresetName (int index, yup::StringRef newName) override;

    yup::Result loadStateFromMemory (const yup::MemoryBlock& data) override;
    yup::Result saveStateIntoMemory (yup::MemoryBlock& data) override;

    bool hasEditor() const override;
    yup::AudioProcessorEditor* createEditor() override;

    void setStandaloneTriggerGate (bool shouldBeOn) noexcept;
    [[nodiscard]] bool isStandaloneTriggerGateRequested() const noexcept;
    [[nodiscard]] float getOutputPeakLevel() const noexcept;
    [[nodiscard]] std::uint32_t getStandaloneTriggerEdgeCountForTests() const noexcept;

private:
    enum ParameterIndex
    {
        weight,
        cutoff,
        decay,
        pressure,
        stereoFault,
        output,
        parameterCount
    };

    void advanceParameterHandles (int samplePosition) noexcept;
    void consumeStandaloneTriggerGate() noexcept;
    void applyEngineParameters() noexcept;
    void resetPerformanceState() noexcept;

    std::array<yup::AudioParameter::Ptr, parameterCount> parameters;
    std::array<yup::AudioParameterHandle, parameterCount> parameterHandles;
    std::array<float, parameterCount> smoothedValues {};
    subquake::SubquakeEngine engine;

    enum class ActiveSource
    {
        none,
        midi,
        standalone
    };

    int activeNote = -1;
    ActiveSource activeSource = ActiveSource::none;
    bool audioStandaloneGate = false;
    std::uint32_t consumedStandaloneGateEdges = 0;
    int controlUpdateCountdown = 0;
    std::atomic<int> standaloneTriggerDesiredGate { 0 };
    std::atomic<std::uint32_t> standaloneTriggerGateEdges { 0 };
    std::atomic<int> outputPeakMilli { 0 };
    std::atomic<int> currentPreset { 0 };
    std::array<yup::String, 4> presetNames {
        "Fault Weight",
        "Concrete Bloom",
        "Pressure Hull",
        "Aftershock Dub"
    };
};

} // namespace subquake::plugin
