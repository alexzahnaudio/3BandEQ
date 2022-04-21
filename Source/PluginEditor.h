/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

using RAP = juce::RangedAudioParameter;

// This class determines the appearance of the rotary slider...
// ...and is a component of the RotarySliderWithLabels class
struct LookAndFeel : juce::LookAndFeel_V4
{
    void drawRotarySlider (juce::Graphics&,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider&) override { };
};

// Rotary Slider struct
struct RotarySliderWithLabels : juce::Slider
{
    RotarySliderWithLabels(RAP& rap, const juce::String& unitSuffix) :
    juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
    juce::Slider::TextEntryBoxPosition::NoTextBox),
    RAP(&rap),
    suffix(unitSuffix)
    {
        setLookAndFeel(&laf);
    }
    
    ~RotarySliderWithLabels()
    {
        setLookAndFeel(nullptr);
    }
    
    void paint(juce::Graphics& g) override { };
    juce::Rectangle<int> getSliderBounds() const;
    int getTextHeight() const { return 14; }
    juce::String getDisplayString() const;
    
private:
    LookAndFeel laf;
    
    RAP* RAP;
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
