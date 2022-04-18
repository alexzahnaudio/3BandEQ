/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
_3BandEQAudioProcessor::_3BandEQAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

_3BandEQAudioProcessor::~_3BandEQAudioProcessor()
{
}

//==============================================================================
const juce::String _3BandEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool _3BandEQAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool _3BandEQAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool _3BandEQAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double _3BandEQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int _3BandEQAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int _3BandEQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void _3BandEQAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String _3BandEQAudioProcessor::getProgramName (int index)
{
    return {};
}

void _3BandEQAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void _3BandEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    
    // Set up Process Spec
    juce::dsp::ProcessSpec processSpec;
    processSpec.maximumBlockSize = samplesPerBlock;
    processSpec.numChannels = 1;
    processSpec.sampleRate = sampleRate;

    // Prepare both the left and right chains with our Process Spec
    leftChain.prepare(processSpec);
    rightChain.prepare(processSpec);

    // Get the current parameter values and update all filters in the chain
    updateFilters();
}

void _3BandEQAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool _3BandEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void _3BandEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    // Get the current parameter values and update all filters in the chain
    updateFilters();
    
    // create audio block with size of our buffer
    juce::dsp::AudioBlock<float> block(buffer);
    // extract left and right channels separately from audio block
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);
    // create processing context for left and right channels
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
    // tell the left and right mono processing chains to use these processing contexts
    leftChain.process(leftContext);
    rightChain.process(rightContext);
}

//==============================================================================
bool _3BandEQAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* _3BandEQAudioProcessor::createEditor()
{
    return new _3BandEQAudioProcessorEditor (*this);
//    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void _3BandEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Use JUCE ValueTree type as an intermediary to save parameter state to memory
    
    // Create a memory output stream
    // 'true' here means "append to existing data"
    juce::MemoryOutputStream MOS(destData, true);
    // Write our APVTS current state to the memory output stream
    APVTS.state.writeToStream(MOS);
}

void _3BandEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Restore parameter state from memory, using JUCE ValueTree type as intermediary
    
    // Grab the stored parameter values
    auto valueTree = juce::ValueTree::readFromData(data, sizeInBytes);
    // Make sure the values are valid
    if( valueTree.isValid() )
    {
        // Feed the values to our APVTS
        APVTS.replaceState(valueTree);
        // Update filter values to reflect the new state
        updateFilters();
    }
}

// Helper function to return all parameter values from the APVTS as a ChainSettings struct
ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& APVTS)
{
    ChainSettings settings;
    
    // Get all current parameter values from APVTS
    settings.lowCutFreq     = APVTS.getRawParameterValue("LowCut_Freq")->load();
    settings.lowCutSlope    = static_cast<Slope>( APVTS.getRawParameterValue("LowCut_Slope")->load() );
    settings.highCutFreq    = APVTS.getRawParameterValue("HighCut_Freq")->load();
    settings.highCutSlope   = static_cast<Slope>( APVTS.getRawParameterValue("HighCut_Slope")->load() );
    settings.peakFreq       = APVTS.getRawParameterValue("Peak_Freq")->load();
    settings.peakGain_dB    = APVTS.getRawParameterValue("Peak_Gain")->load();
    settings.peakQ          = APVTS.getRawParameterValue("Peak_Q")->load();
    
    return settings;
}

//=======================================================================================
// Filter update functions
//=======================================================================================

Coefficients makePeakFilter(const ChainSettings& chainSettings, double sampleRate)
{
    // Calculate Peak filter coefficients based on current chain settings.
    // This is a reference-counted wrapper around an array of float values,
    //    allocated on the heap (which is "bad"? Look into this. Why is heap bad for real-time audio?)
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                               chainSettings.peakFreq,
                                                               chainSettings.peakQ,
                                                               juce::Decibels::decibelsToGain(chainSettings.peakGain_dB));
}

// Helper function to update the peak filter
void _3BandEQAudioProcessor::updatePeakFilter(const ChainSettings &chainSettings)
{
    // Calculate Peak filter coefficients based on current chain settings.
    auto updatedPeakCoefficients = makePeakFilter(chainSettings, getSampleRate());
    // Apply those coefficients to the peak filter (left chain and right chain)
    updateCoefficients(leftChain.get<ChainPositions::Peak>().coefficients, updatedPeakCoefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak>().coefficients, updatedPeakCoefficients);
}

// Helper function to update filter coefficients
// (Free function)
void updateCoefficients(Coefficients &old, const Coefficients &replacement)
{
    // Because JUCE DSP's IIR Coefficients are reference-counted on the heap, we need to dereference here
    *old = *replacement;
}

// Helper function to update the low cut filter
void _3BandEQAudioProcessor::updateLowCutFilter(const ChainSettings &chainSettings)
{
    // Calculate filter order (2, 4, 6, or 8) from filter slope parameters (0, 1, 2, or 3)
    auto lowCutFilterOrder = 2 * (chainSettings.lowCutSlope + 1);
    //auto highCutFilterOrder = 2 * (chainSettings.highCutSlope + 1);
    
    // Calculate low cut filter coefficients
    auto lowCutFilterCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
                                                                                                          getSampleRate(),
                                                                                                          lowCutFilterOrder);
    // Get the low cut filter (left and right chains)
    auto& leftLowCutFilter = leftChain.get<ChainPositions::LowCut>();
    auto& rightLowCutFilter = rightChain.get<ChainPositions::LowCut>();
    // Use chain settings to calculate new filter coefficients and apply them to the filter (L and R chains)
    updateCutFilter(leftLowCutFilter, lowCutFilterCoefficients, chainSettings.lowCutSlope);
    updateCutFilter(rightLowCutFilter, lowCutFilterCoefficients, chainSettings.lowCutSlope);
}

// Helper function to update the high cut filter
void _3BandEQAudioProcessor::updateHighCutFilter(const ChainSettings &chainSettings)
{
    // Calculate filter order (2, 4, 6, or 8) from filter slope parameters (0, 1, 2, or 3)
    auto highCutFilterOrder = 2 * (chainSettings.highCutSlope + 1);
    
    // Calculate high cut filter coefficients
    auto highCutFilterCoefficients = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFreq,
                                                                                                                getSampleRate(),
                                                                                                                highCutFilterOrder);
    // Get the high cut filter (left and right chains)
    auto& leftHighCutFilter = leftChain.get<ChainPositions::HighCut>();
    auto& rightHighCutFilter = rightChain.get<ChainPositions::HighCut>();
    // Use chain settings to calculate new filter coefficients and apply them to the filter (L and R chains)
    updateCutFilter(leftHighCutFilter, highCutFilterCoefficients, chainSettings.highCutSlope);
    updateCutFilter(rightHighCutFilter, highCutFilterCoefficients, chainSettings.highCutSlope);
}

// Helper function to update all the filters
void _3BandEQAudioProcessor::updateFilters()
{
    // Get the current chain settings (parameter values)
    auto settings = getChainSettings(APVTS);
    
    // Update the low-cut, peaking, and high-cut filters
    updateLowCutFilter(settings);
    updatePeakFilter(settings);
    updateHighCutFilter(settings);
}

//=======================================================================================
// Parameter Layout
//=======================================================================================

juce::AudioProcessorValueTreeState::ParameterLayout _3BandEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    // Set up filter slope options (12, 24, 36, or 48 dB/octave)
    juce::StringArray filterSlopeOptionsArray;
    for(int i=0; i<4; i++)
    {
        juce::String str;
        str << (12 + i*12);
        str << " dB/oct";
        filterSlopeOptionsArray.add(str);
    }
    
    // Low Cut Frequency Parameter
    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut_Freq",
                                                           "LowCut_Freq",
                                                           juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f),
                                                           20.f));
    
    // Low Cut Slope
    layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut_Slope",
                                                            "LowCut_Slope",
                                                            filterSlopeOptionsArray,
                                                            0) );
    
    // High Cut Frequency Parameter
    layout.add(std::make_unique<juce::AudioParameterFloat>("HighCut_Freq",
                                                           "HighCut_Freq",
                                                           juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f),
                                                           20000.f));

    // High Cut Slope
    layout.add(std::make_unique<juce::AudioParameterChoice>("HighCut_Slope",
                                                            "HighCut_Slope",
                                                            filterSlopeOptionsArray,
                                                            0) );
    
    // Peak Frequency Parameter
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak_Freq",
                                                           "Peak_Freq",
                                                           juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f),
                                                           750.f));
    
    // Peak Gain Parameter
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak_Gain",
                                                           "Peak_Gain",
                                                           juce::NormalisableRange<float>(-24.f, 24.f, 0.5f, 1.f),
                                                           0.f));
    // Peak Q Parameter
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak_Q",
                                                           "Peak_Q",
                                                           juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f),
                                                           1.f));
        
    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new _3BandEQAudioProcessor();
}
