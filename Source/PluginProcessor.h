/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include <array>

enum Channel
{
    LEFT,   // 0
    RIGHT   // 1
};

// FIFO queue for the SingleChannelSampleFifo class
template<typename T>
struct Fifo
{
    void prepare(int numChannels, int numSamples)
    {
        static_assert(std::is_same_v<T, juce::AudioBuffer<float>>,
                      "prepare(numChannels, numSamples) should only be used when the FIFO is holding a juce::AudioBuffer<float>.");
        
        for ( auto& buffer : buffers )
        {
            buffer.setSize(numChannels,
                           numSamples,
                           false,           // clear everything?
                           true,            // including the extra space?
                           true);           // avoid reallocating?
            buffer.clear();
        }
    }
    
    void prepare(size_t numElements)
    {
        static_assert(std::is_same_v<T, std::vector<float>>,
                      "prepare(numElements) should only be used when the FIFO is holding a std::vector<float>.");
        
        for ( auto& buffer : buffers )
        {
            buffer.clear();
            buffer.resize(numElements, 0);
        }

    }
    
    bool push(const T& t)
    {
        auto write = fifo.write(1);
        if ( write.blockSize1 > 0 )
        {
            buffers[write.startIndex1] = t;
            return true;
        }
        
        return false;
    }
    
    bool pull(T& t)
    {
        auto read = fifo.read(1);
        if ( read.blockSize1 > 0 )
        {
            t = buffers[read.startIndex1];
            return true;
        }
        
        return false;
    }
    
    int getNumAvailableForReading() const
    {
        return fifo.getNumReady();
    }
private:
    static constexpr int Capacity = 30;
    std::array<T, Capacity> buffers;
    juce::AbstractFifo fifo {Capacity};
};

// Converts some-number-of-samples from a host buffer into a FIFO queue of fixed-size blocks
template<typename BlockType>
struct SingleChannelSampleFifo
{
    SingleChannelSampleFifo(Channel channel) : channelToUse(channel)
    {
        prepared.set(false);
    }
    
    void update(const BlockType& buffer)
    {
        jassert(prepared.get());
        jassert(buffer.getNumChannels() > channelToUse);
        auto* channelPtr = buffer.getReadPointer(channelToUse);
        
        for (int i = 0; i < buffer.getNumSamples(); i++)
        {
            pushNextSampleIntoFifo(channelPtr[i]);
        }
    }
    
    void prepare(int bufferSize)
    {
        prepared.set(false);
        size.set(bufferSize);
        
        bufferToFill.setSize(1,             // channel
                             bufferSize,    // number of samples
                             false,         // keep existing content?
                             true,          // clear extra space?
                             true);         // avoid reallocating?
        audioBufferFifo.prepare(1, bufferSize);
        fifoIndex = 0;
        prepared.set(true);
    }
    //===========================================================================
    int getNumCompleteBuffersAvailable() const { return audioBufferFifo.getNumAvailableForReading(); }
    bool isPrepared() const { return prepared.get(); }
    int getSize() const { return size.get(); }
    //===========================================================================
    bool getAudioBuffer(BlockType& buffer) { return audioBufferFifo.pull(buffer); }
private:
    Channel channelToUse;
    int fifoIndex = 0;
    Fifo<BlockType> audioBufferFifo;
    BlockType bufferToFill;
    juce::Atomic<bool> prepared = false;
    juce::Atomic<int> size = 0;
    
    void pushNextSampleIntoFifo(float sample)
    {
        if (fifoIndex == bufferToFill.getNumSamples())
        {
            auto ok = audioBufferFifo.push(bufferToFill);
            
            juce::ignoreUnused(ok);
            
            fifoIndex = 0;
        }
        
        bufferToFill.setSample(0, fifoIndex, sample);
        fifoIndex++;
    }
};

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

inline auto makeLowCutFilter(const ChainSettings& chainSettings, double sampleRate)
{
    // Calculate filter order (2, 4, 6, or 8) from filter slope parameters (0, 1, 2, or 3)
    auto lowCutFilterOrder = 2 * (chainSettings.lowCutSlope + 1);
    // Calculate and return the low cut filter coefficients
    return juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
                                                                                       sampleRate,
                                                                                       lowCutFilterOrder);
}

inline auto makeHighCutFilter(const ChainSettings& chainSettings, double sampleRate)
{
    // Calculate filter order (2, 4, 6, or 8) from filter slope parameters (0, 1, 2, or 3)
    auto highCutFilterOrder = 2 * (chainSettings.highCutSlope + 1);
    // Calculate and return the high cut filter coefficients
    return juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFreq,
                                                                                      sampleRate,
                                                                                      highCutFilterOrder);
}

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

    using BlockType = juce::AudioBuffer<float>;
    SingleChannelSampleFifo<BlockType> leftChannelFIFO { Channel::LEFT };
    SingleChannelSampleFifo<BlockType> rightChannelFIFO { Channel::RIGHT };
    
private:
    // Declare two of these mono chains. One for left channel, one for right channel.
    MonoChain leftChain, rightChain;
    
    void updatePeakFilter(const ChainSettings& ChainSettings);
    
    // Helper functions to update low- and high- cut filters
    void updateLowCutFilter(const ChainSettings& chainSettings);
    void updateHighCutFilter(const ChainSettings& chainSettings);
    
    
    // Helper function to update all filters in the chain
    void updateFilters();
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (_3BandEQAudioProcessor)
};
