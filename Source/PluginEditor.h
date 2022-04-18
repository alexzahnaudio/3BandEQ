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
                                  juce::Slider::TextEntryBoxPosition::NoTextBox)
    {
        //constructor body
    }
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
    
    MonoChain monoChain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (_3BandEQAudioProcessorEditor)
};
