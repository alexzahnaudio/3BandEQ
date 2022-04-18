/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

// Filter slope enum
enum Slope
{
    SLOPE_12,
    SLOPE_24,
    SLOPE_36,
    SLOPE_48
};

// Set up a struct to contain all parameter settings in the chain
struct ChainSettings
{
    float lowCutFreq {0}, highCutFreq {0};
    Slope lowCutSlope {Slope::SLOPE_12}, highCutSlope {Slope::SLOPE_12};
    float peakFreq {0}, peakGain_dB {0}, peakQ {1.f};
};

// Helper function to return all parameter values from the APVTS as a ChainSettings struct
ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& APVTS);

// Shorthand for basic IIR filter. 12dB/oct by default.
using Filter = juce::dsp::IIR::Filter<float>;
// Sub-processing chain for our Low/High Cut filters, consisting of FOUR 12dB/oct filters.
using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
// Our single-channel processing chain: (Low)Cut Filter, Peaking Filter, (High)Cut Filter.
using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;

// Define enum to simplify accessing each link in the processing chain
enum ChainPositions{
    LowCut,     //0
    Peak,       //1
    HighCut     //2
};

//
using Coefficients = Filter::CoefficientsPtr;
void updateCoefficients(Coefficients& old, const Coefficients& replacement);

Coefficients makePeakFilter(const ChainSettings& chainSettings, double sampleRate);


//==============================================================================
/**
*/
class _3BandEQAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    _3BandEQAudioProcessor();
    ~_3BandEQAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    juce::AudioProcessorValueTreeState APVTS {*this, nullptr, "Parameters", createParameterLayout()};
    
private:
    // Declare two of these mono chains. One for left channel, one for right channel.
    MonoChain leftChain, rightChain;
    
    void updatePeakFilter(const ChainSettings& ChainSettings);
    
    // Helper function to update a filter component (one of the four 12dB/oct "sub"-filters that...
    // ...make up the low- and high-cut filters in our chain).
    template<int FilterComponentIndex, typename ChainType, typename CoefficientType>
    void updateFilterComponent(ChainType& chain, CoefficientType& lowCutFilterCoefficients)
    {
        // Pass the current filter-chain parameter settings to the filters as coefficients
        updateCoefficients( chain.template get<FilterComponentIndex>().coefficients,
                            lowCutFilterCoefficients[FilterComponentIndex] );
        // Stop bypassing filter components as required
        chain.template setBypassed<FilterComponentIndex>(false);
    }
    
    // Helper function to update a cut filter
    template<typename ChainType, typename CoefficientType>
    void updateCutFilter(ChainType& lowCutFilter,
                         const CoefficientType& lowCutFilterCoefficients,
                         const Slope& lowCutFilterSlope)
    {
        // Bypass each 12dB/oct filter component in the low cut filter...
        lowCutFilter.template setBypassed<0>(true);
        lowCutFilter.template setBypassed<1>(true);
        lowCutFilter.template setBypassed<2>(true);
        lowCutFilter.template setBypassed<3>(true);

        // ...Then update/re-enable them as demanded by the current chain settings
        switch( lowCutFilterSlope )
        {
            // If chain settings specify 8th order (48 dB/oct) slope,
            //   turn on all four 12 dB/oct filters. (indeces 0,1,2,3)
            // If chain settings specify 6th order (36 dB/oct) slope,
            //   turn on three of the four 12 dB/oct filters. (indeces 0,1,2)
            // and so on...
            case SLOPE_48:
            {
                updateFilterComponent<3>(lowCutFilter, lowCutFilterCoefficients);
            }
            case SLOPE_36:
            {
                updateFilterComponent<2>(lowCutFilter, lowCutFilterCoefficients);
            }
            case SLOPE_24:
            {
                updateFilterComponent<1>(lowCutFilter, lowCutFilterCoefficients);
            }
            case SLOPE_12:
            {
                updateFilterComponent<0>(lowCutFilter, lowCutFilterCoefficients);
            }
        }
    }
    
    // Helper functions to update low- and high- cut filters
    void updateLowCutFilter(const ChainSettings& chainSettings);
    void updateHighCutFilter(const ChainSettings& chainSettings);
    
    
    // Helper function to update all filters in the chain
    void updateFilters();
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (_3BandEQAudioProcessor)
};
