/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

enum FFTOrder
{
    ORDER_2048 = 11,
    ORDER_4096 = 12,
    ORDER_8192 = 13
};

// This class generates FFT data from an audio buffer
template<typename BlockType>
struct FFTDataGenerator
{
    void produceFFTDataForRendering(const juce::AudioBuffer<float>& audioData, const float negativeInf)
    {
        const auto fftSize = getFFTSize();
        
        fftData.assign(fftData.size(), 0);
        auto* readIndex = audioData.getReadPointer(0);
        std::copy(readIndex, readIndex + fftSize, fftData.begin());
        
        // First apply a windowing function to our data
        window->multiplyWithWindowingTable( fftData.data(), fftSize );
        // then render our FFT data
        forwardFFT->performFrequencyOnlyForwardTransform( fftData.data() );
        
        int numBins = (int)fftSize / 2;
        
        // Normalize FFT values
        for (int i=0; i<numBins; i++)
        {
            fftData[i] /= (float)numBins;
        }
        
        // Convert the FFT values to dB
        for (int i=0; i<numBins; i++)
        {
            fftData[i] = juce::Decibels::gainToDecibels(fftData[i], negativeInf);
        }
        
        fftDataFIFO.push(fftData);
    }
    
    void changeOrder(FFTOrder newOrder)
    {
        // When FFT order is changed, recreate the window, forwardFFT, fifo, fftData
        // and reset the fifoIndex
        // Things that need recreating should be created on the heap via std::make_unique<>
        order = newOrder;
        auto fftSize = getFFTSize();
        
        forwardFFT = std::make_unique<juce::dsp::FFT>(order);
        window = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize,
                                                                       juce::dsp::WindowingFunction<float>::blackmanHarris);
        
        fftData.clear();
        fftData.resize(fftSize * 2, 0);
        
        fftDataFIFO.prepare(fftData.size());
    }
    //======================================================================================
    int getFFTSize() const { return 1 << order; }
    int getNumAvailableFFTDataBlocks() const { return fftDataFIFO.getNumAvailableForReading(); }
    //======================================================================================
    bool getFFTData(BlockType& fftData) { return fftDataFIFO.pull(fftData); }
private:
    FFTOrder order;
    BlockType fftData;
    std::unique_ptr<juce::dsp::FFT> forwardFFT;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    
    Fifo<BlockType> fftDataFIFO;
};

// Generates a path from FFT data
template<typename PathType>
struct AnalyzerPathGenerator
{
    // Converts renderData[] into a juce::Path
    void generatePath(const std::vector<float>& renderData,
                      juce::Rectangle<float> fftBounds,
                      int fftSize,
                      float binWidth,
                      float negativeInf)
    {
        auto top = fftBounds.getY();
        auto bottom = fftBounds.getHeight();
        auto width = fftBounds.getWidth();
        
        int numBins = (int)fftSize / 2;
        
        PathType p;
        p.preallocateSpace( 3 * (int)fftBounds.getWidth() );
        
        auto map = [bottom, top, negativeInf](float v)
        {
            return juce::jmap(v,
                              negativeInf, 0.f,
                              float(bottom), top);
        };
        
        auto y = map(renderData[0]);
        
        jassert( !std::isnan(y) && !std::isinf(y) );
        
        p.startNewSubPath(0, y);
        
        // Draw line-to's every pathResolution pixels
        const int pathResolution = 2;
        
        for ( int binNum = 1; binNum < numBins; binNum += pathResolution )
        {
            y = map(renderData[binNum]);
            
            //jassert( !std::isnan(y) && !std::isinf(y) );
            
            if ( !std::isnan(y) && !std::isinf(y) )
            {
                auto binFreq = binNum * binWidth;
                auto normalizedBinX = juce::mapFromLog10(binFreq, 1.f, 20000.f);
                int binX = std::floor(normalizedBinX * width);
                p.lineTo(binX, y);
            }
        }
        
        pathFIFO.push(p);
    }
    
    int getNumPathsAvailable() const
    {
        return pathFIFO.getNumAvailableForReading();
    }
    
    bool getPath(PathType& path)
    {
        return pathFIFO.pull(path);
    }
private:
    Fifo<PathType> pathFIFO;
};

// This class determines the appearance of the rotary slider...
// ...and is a component of the RotarySliderWithLabels class
struct LookAndFeel : juce::LookAndFeel_V4
{
    void drawRotarySlider (juce::Graphics&,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider&) override;
};

// Rotary Slider struct
struct RotarySliderWithLabels : juce::Slider
{
    RotarySliderWithLabels(juce::RangedAudioParameter& rap, const juce::String& unitSuffix) :
    juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
    juce::Slider::TextEntryBoxPosition::NoTextBox),
    rap(&rap),
    suffix(unitSuffix)
    {
        setLookAndFeel(&laf);
    }
    
    ~RotarySliderWithLabels()
    {
        setLookAndFeel(nullptr);
    }
    
    struct LabelWithPosition
    {
        float position;
        juce::String label;
    };
    
    juce::Array<LabelWithPosition> labels;
    
    void paint(juce::Graphics& g) override;
    juce::Rectangle<int> getSliderBounds() const;
    int getTextHeight() const { return 14; }
    juce::String getDisplayString() const;
    
private:
    LookAndFeel laf;
    
    juce::RangedAudioParameter* rap;
    juce::String suffix;
};

// Response Curve struct
struct ResponseCurve : juce::Component,
juce::AudioProcessorParameter::Listener,
juce::Timer
{
    ResponseCurve(_3BandEQAudioProcessor&);
    ~ResponseCurve();
    
    void parameterValueChanged (int parameterIndex, float newValue) override;
    
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override {};
    
    void timerCallback() override;
        
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    _3BandEQAudioProcessor& audioProcessor;
    // atomic flag to let us know when our parameters have changed...
    // ...and the GUI needs updating
    juce::Atomic<bool> parametersChanged { false };
    // Mono chain
    MonoChain monoChain;
    void updateChain();
    // Response curve grid background
    juce::Image background;
    juce::Rectangle<int> getRenderArea();
    juce::Rectangle<int> getAnalysisArea();
    
    SingleChannelSampleFifo<_3BandEQAudioProcessor::BlockType>* leftChannelFIFO;
//    SingleChannelSampleFifo<_3BandEQAudioProcessor::BlockType>* rightChannelFIFO;
    
    juce::AudioBuffer<float> monoBuffer;
    
    FFTDataGenerator<std::vector<float>> leftChannelFFTDataGenerator;
    
    AnalyzerPathGenerator<juce::Path> pathGenerator;
    
    juce::Path leftChannelFFTPath;
};

//==============================================================================
// Class Declaration
//==============================================================================
/**
*/
class _3BandEQAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    _3BandEQAudioProcessorEditor (_3BandEQAudioProcessor&);
    ~_3BandEQAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    _3BandEQAudioProcessor& audioProcessor;
    
    ResponseCurve responseCurve;
    
    // Declare our rotary sliders
    RotarySliderWithLabels peakFreqSlider,
                 peakGainSlider,
                 peakQSlider,
                 lowCutFreqSlider,
                 lowCutSlopeSlider,
                 highCutFreqSlider,
                 highCutSlopeSlider;
    
    // Declare parameter attachments for each of our sliders
    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;
    Attachment peakFreqSliderAttachment,
               peakGainSliderAttachment,
               peakQSliderAttachment,
               lowCutFreqSliderAttachment,
               lowCutSlopeSliderAttachment,
               highCutFreqSliderAttachment,
               highCutSlopeSliderAttachment;
    
    // Declare a function to return all our rotary sliders as a vector
    std::vector<juce::Component*> getComponents();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (_3BandEQAudioProcessorEditor)
};
