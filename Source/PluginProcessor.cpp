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
    
    // Get current parameter settings in chain
    auto chainSettings = getChainSettings(APVTS);
    
    // Update Peak filter coefficients based on current chain settings.
    // This is a reference-counted wrapper around an array of float values,
    //    allocated on the heap (which is "bad"? Look into this. Why is heap bad for real-time audio?)
    auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                                                chainSettings.peakFreq,
                                                                                chainSettings.peakQ,
                                                                                juce::Decibels::decibelsToGain(chainSettings.peakGain_dB));
    
    // Because JUCE DSP's IIR Coefficients are reference-counted, we need to dereference here
    *leftChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;
    *rightChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;
    
    // Calculate filter order (2, 4, 6, or 8) from filter slope parameters (0, 1, 2, or 3)
    auto lowCutFilterOrder = 2 * (chainSettings.lowCutSlope + 1);
    //auto highCutFilterOrder = 2 * (chainSettings.highCutSlope + 1);
    
    // Calculate low cut filter coefficients
    auto lowCutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
                                                                                                          sampleRate,
                                                                                                          lowCutFilterOrder);
    // Initialize the left chain low cut filter:
    //
    // First, get the left-chain low cut filter
    auto& leftLowCutFilter = leftChain.get<ChainPositions::LowCut>();
    
    // Bypass each 12dB/oct sub-filter in the low cut filter
    leftLowCutFilter.setBypassed<0>(true);
    leftLowCutFilter.setBypassed<1>(true);
    leftLowCutFilter.setBypassed<2>(true);
    leftLowCutFilter.setBypassed<3>(true);
    
    // Re-enable each 12dB/oct sub-filter in the low cut filter as demanded by the current chain settings
    switch( chainSettings.lowCutSlope )
    {
        // If chain settings specify 8th order (48 dB/oct) slope,
        //   turn on all four 12 dB/oct filters. (indeces 0,1,2,3)
        // If chain settings specify 6th order (36 dB/oct) slope,
        //   turn on three of the four 12 dB/oct filters. (indeces 0,1,2)
        // and so on...
        case SLOPE_48:
        {
            // For each required 12dB/oct filter, update its filter coefficients,
            //   and STOP bypassing it.
            *leftLowCutFilter.get<3>().coefficients = *lowCutCoefficients[3];
            leftLowCutFilter.setBypassed<3>(false);
            *leftLowCutFilter.get<2>().coefficients = *lowCutCoefficients[2];
            leftLowCutFilter.setBypassed<2>(false);
            *leftLowCutFilter.get<1>().coefficients = *lowCutCoefficients[1];
            leftLowCutFilter.setBypassed<1>(false);
            *leftLowCutFilter.get<0>().coefficients = *lowCutCoefficients[0];
            leftLowCutFilter.setBypassed<0>(false);
            
            break;
        }
        case SLOPE_36:
        {
            *leftLowCutFilter.get<2>().coefficients = *lowCutCoefficients[2];
            leftLowCutFilter.setBypassed<2>(false);
            *leftLowCutFilter.get<1>().coefficients = *lowCutCoefficients[1];
            leftLowCutFilter.setBypassed<1>(false);
            *leftLowCutFilter.get<0>().coefficients = *lowCutCoefficients[0];
            leftLowCutFilter.setBypassed<0>(false);
            
            break;
        }
        case SLOPE_24:
        {
            *leftLowCutFilter.get<1>().coefficients = *lowCutCoefficients[1];
            leftLowCutFilter.setBypassed<1>(false);
            *leftLowCutFilter.get<0>().coefficients = *lowCutCoefficients[0];
            leftLowCutFilter.setBypassed<0>(false);
            
            break;
        }
        case SLOPE_12:
        {
            *leftLowCutFilter.get<0>().coefficients = *lowCutCoefficients[0];
            leftLowCutFilter.setBypassed<0>(false);
            
            break;
        }
    }
    
    // Now do the same for the right chain:
    //
    // First, get the right-chain low cut filter
    auto& rightLowCutFilter = rightChain.get<ChainPositions::LowCut>();
    
    // Bypass each 12dB/oct sub-filter in the low cut filter
    rightLowCutFilter.setBypassed<0>(true);
    rightLowCutFilter.setBypassed<1>(true);
    rightLowCutFilter.setBypassed<2>(true);
    rightLowCutFilter.setBypassed<3>(true);
    
    // Re-enable each 12dB/oct sub-filter in the low cut filter as demanded by the current chain settings
    switch( chainSettings.lowCutSlope )
    {
        // If chain settings specify 8th order (48 dB/oct) slope,
        //   turn on all four 12 dB/oct filters. (indeces 0,1,2,3)
        // If chain settings specify 6th order (36 dB/oct) slope,
        //   turn on three of the four 12 dB/oct filters. (indeces 0,1,2)
        // and so on...
        case SLOPE_48:
        {
            // For each required 12dB/oct filter, update its filter coefficients,
            //   and STOP bypassing it.
            *rightLowCutFilter.get<3>().coefficients = *lowCutCoefficients[3];
            rightLowCutFilter.setBypassed<3>(false);
            *rightLowCutFilter.get<2>().coefficients = *lowCutCoefficients[2];
            rightLowCutFilter.setBypassed<2>(false);
            *rightLowCutFilter.get<1>().coefficients = *lowCutCoefficients[1];
            rightLowCutFilter.setBypassed<1>(false);
            *rightLowCutFilter.get<0>().coefficients = *lowCutCoefficients[0];
            rightLowCutFilter.setBypassed<0>(false);
            
            break;
        }
        case SLOPE_36:
        {
            *rightLowCutFilter.get<2>().coefficients = *lowCutCoefficients[2];
            rightLowCutFilter.setBypassed<2>(false);
            *rightLowCutFilter.get<1>().coefficients = *lowCutCoefficients[1];
            rightLowCutFilter.setBypassed<1>(false);
            *rightLowCutFilter.get<0>().coefficients = *lowCutCoefficients[0];
            rightLowCutFilter.setBypassed<0>(false);
            
            break;
        }
        case SLOPE_24:
        {
            *rightLowCutFilter.get<1>().coefficients = *lowCutCoefficients[1];
            rightLowCutFilter.setBypassed<1>(false);
            *rightLowCutFilter.get<0>().coefficients = *lowCutCoefficients[0];
            rightLowCutFilter.setBypassed<0>(false);
            
            break;
        }
        case SLOPE_12:
        {
            *rightLowCutFilter.get<0>().coefficients = *lowCutCoefficients[0];
            rightLowCutFilter.setBypassed<0>(false);
            
            break;
        }
    }

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

    //=======================================================================
    // Update chain parameter settings
    //=======================================================================
    
    // Get current parameter settings in chain
    auto chainSettings = getChainSettings(APVTS);
    
    //=======================================================================
    // Update Peak filter coefficients
    //=======================================================================
    
    // Update Peak filter coefficients based on current chain settings.
    // This is a reference-counted wrapper around an array of float values,
    //    allocated on the heap (which is "bad"? Look into this. Why is heap bad for real-time audio?)
    auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(),
                                                                                chainSettings.peakFreq,
                                                                                chainSettings.peakQ,
                                                                                juce::Decibels::decibelsToGain(chainSettings.peakGain_dB));
    
    // Because JUCE DSP's IIR Coefficients are reference-counted, we need to dereference here
    *leftChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;
    *rightChain.get<ChainPositions::Peak>().coefficients = *peakCoefficients;
    
    //========================================================================
    // Update Low Cut filter coefficients
    //=======================================================================
    
    // Calculate filter order (2, 4, 6, or 8) from filter slope parameters (0, 1, 2, or 3)
    auto lowCutFilterOrder = 2 * (chainSettings.lowCutSlope + 1);
    //auto highCutFilterOrder = 2 * (chainSettings.highCutSlope + 1);
    
    // Calculate low cut filter coefficients
    auto lowCutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
                                                                                                          getSampleRate(),
                                                                                                          lowCutFilterOrder);
    // Initialize the left chain low cut filter:
    //
    // First, get the left-chain low cut filter
    auto& leftLowCutFilter = leftChain.get<ChainPositions::LowCut>();
    
    // Bypass each 12dB/oct sub-filter in the low cut filter
    leftLowCutFilter.setBypassed<0>(true);
    leftLowCutFilter.setBypassed<1>(true);
    leftLowCutFilter.setBypassed<2>(true);
    leftLowCutFilter.setBypassed<3>(true);
    
    // Re-enable each 12dB/oct sub-filter in the low cut filter as demanded by the current chain settings
    switch( chainSettings.lowCutSlope )
    {
        // If chain settings specify 8th order (48 dB/oct) slope,
        //   turn on all four 12 dB/oct filters. (indeces 0,1,2,3)
        // If chain settings specify 6th order (36 dB/oct) slope,
        //   turn on three of the four 12 dB/oct filters. (indeces 0,1,2)
        // and so on...
        case SLOPE_48:
        {
            // For each required 12dB/oct filter, update its filter coefficients,
            //   and STOP bypassing it.
            *leftLowCutFilter.get<3>().coefficients = *lowCutCoefficients[3];
            leftLowCutFilter.setBypassed<3>(false);
            *leftLowCutFilter.get<2>().coefficients = *lowCutCoefficients[2];
            leftLowCutFilter.setBypassed<2>(false);
            *leftLowCutFilter.get<1>().coefficients = *lowCutCoefficients[1];
            leftLowCutFilter.setBypassed<1>(false);
            *leftLowCutFilter.get<0>().coefficients = *lowCutCoefficients[0];
            leftLowCutFilter.setBypassed<0>(false);
            
            break;
        }
        case SLOPE_36:
        {
            *leftLowCutFilter.get<2>().coefficients = *lowCutCoefficients[2];
            leftLowCutFilter.setBypassed<2>(false);
            *leftLowCutFilter.get<1>().coefficients = *lowCutCoefficients[1];
            leftLowCutFilter.setBypassed<1>(false);
            *leftLowCutFilter.get<0>().coefficients = *lowCutCoefficients[0];
            leftLowCutFilter.setBypassed<0>(false);
            
            break;
        }
        case SLOPE_24:
        {
            *leftLowCutFilter.get<1>().coefficients = *lowCutCoefficients[1];
            leftLowCutFilter.setBypassed<1>(false);
            *leftLowCutFilter.get<0>().coefficients = *lowCutCoefficients[0];
            leftLowCutFilter.setBypassed<0>(false);
            
            break;
        }
        case SLOPE_12:
        {
            *leftLowCutFilter.get<0>().coefficients = *lowCutCoefficients[0];
            leftLowCutFilter.setBypassed<0>(false);
            
            break;
        }
    }
    
    // Now do the same for the right chain:
    //
    // First, get the right-chain low cut filter
    auto& rightLowCutFilter = rightChain.get<ChainPositions::LowCut>();
    
    // Bypass each 12dB/oct sub-filter in the low cut filter
    rightLowCutFilter.setBypassed<0>(true);
    rightLowCutFilter.setBypassed<1>(true);
    rightLowCutFilter.setBypassed<2>(true);
    rightLowCutFilter.setBypassed<3>(true);
    
    // Re-enable each 12dB/oct sub-filter in the low cut filter as demanded by the current chain settings
    switch( chainSettings.lowCutSlope )
    {
        // If chain settings specify 8th order (48 dB/oct) slope,
        //   turn on all four 12 dB/oct filters. (indeces 0,1,2,3)
        // If chain settings specify 6th order (36 dB/oct) slope,
        //   turn on three of the four 12 dB/oct filters. (indeces 0,1,2)
        // and so on...
        case SLOPE_48:
        {
            // For each required 12dB/oct filter, update its filter coefficients,
            //   and STOP bypassing it.
            *rightLowCutFilter.get<3>().coefficients = *lowCutCoefficients[3];
            rightLowCutFilter.setBypassed<3>(false);
            *rightLowCutFilter.get<2>().coefficients = *lowCutCoefficients[2];
            rightLowCutFilter.setBypassed<2>(false);
            *rightLowCutFilter.get<1>().coefficients = *lowCutCoefficients[1];
            rightLowCutFilter.setBypassed<1>(false);
            *rightLowCutFilter.get<0>().coefficients = *lowCutCoefficients[0];
            rightLowCutFilter.setBypassed<0>(false);
            
            break;
        }
        case SLOPE_36:
        {
            *rightLowCutFilter.get<2>().coefficients = *lowCutCoefficients[2];
            rightLowCutFilter.setBypassed<2>(false);
            *rightLowCutFilter.get<1>().coefficients = *lowCutCoefficients[1];
            rightLowCutFilter.setBypassed<1>(false);
            *rightLowCutFilter.get<0>().coefficients = *lowCutCoefficients[0];
            rightLowCutFilter.setBypassed<0>(false);
            
            break;
        }
        case SLOPE_24:
        {
            *rightLowCutFilter.get<1>().coefficients = *lowCutCoefficients[1];
            rightLowCutFilter.setBypassed<1>(false);
            *rightLowCutFilter.get<0>().coefficients = *lowCutCoefficients[0];
            rightLowCutFilter.setBypassed<0>(false);
            
            break;
        }
        case SLOPE_12:
        {
            *rightLowCutFilter.get<0>().coefficients = *lowCutCoefficients[0];
            rightLowCutFilter.setBypassed<0>(false);
            
            break;
        }
    }

    
    //========================================================================
    
    // create audio block with size of our buffer
    juce::dsp::AudioBlock<float> block(buffer);
    
    // extract left and right channels separately from audio block
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);
    
    // create processing context for left and right channels
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
    
    // tell the left and right mono processing chains to use respective processing contexts
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
    //return new _3BandEQAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void _3BandEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void _3BandEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
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
