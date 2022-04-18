/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// Rotary Slider struct
struct RotarySlider : juce::Slider
{
    RotarySlider() : juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
                                  juce::Slider::TextEntryBoxPosition::NoTextBox) {}
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
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    _3BandEQAudioProcessor& audioProcessor;
    // atomic flag to let us know when our parameters have changed...
    // ...and the GUI needs updating
    juce::Atomic<bool> parametersChanged { false };
    // Mono chain
    MonoChain monoChain;
};

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
    RotarySlider peakFreqSlider,
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
