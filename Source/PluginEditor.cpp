/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
_3BandEQAudioProcessorEditor::_3BandEQAudioProcessorEditor (_3BandEQAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      peakFreqSliderAttachment(audioProcessor.APVTS, "Peak_Freq", peakFreqSlider),
      peakGainSliderAttachment(audioProcessor.APVTS, "Peak_Gain", peakGainSlider),
      peakQSliderAttachment(audioProcessor.APVTS, "Peak_Q", peakQSlider),
      lowCutFreqSliderAttachment(audioProcessor.APVTS, "LowCut_Freq", lowCutFreqSlider),
      lowCutSlopeSliderAttachment(audioProcessor.APVTS, "LowCut_Slope", lowCutSlopeSlider),
      highCutFreqSliderAttachment(audioProcessor.APVTS, "HighCut_Freq", highCutFreqSlider),
      highCutSlopeSliderAttachment(audioProcessor.APVTS, "HighCut_Slope", highCutSlopeSlider)
{
    // Add our GUI components
    for (auto* component : getComponents())
    {
        addAndMakeVisible(component);
    }
    // Set the plugin window size
    setSize (600, 400);
}

_3BandEQAudioProcessorEditor::~_3BandEQAudioProcessorEditor()
{
}

//==============================================================================
void _3BandEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void _3BandEQAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    
    auto bounds = getLocalBounds();
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    
    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);
    
    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(lowCutArea.getHeight() * 0.5));
    lowCutSlopeSlider.setBounds(lowCutArea);
    
    highCutFreqSlider.setBounds(highCutArea.removeFromTop(highCutArea.getHeight() * 0.5));
    highCutSlopeSlider.setBounds(highCutArea);
    
    peakFreqSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.33));
    peakGainSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.5));
    peakQSlider.setBounds(bounds);
}

std::vector<juce::Component*> _3BandEQAudioProcessorEditor::getComponents()
{
    return
    {
        &peakFreqSlider,
        &peakGainSlider,
        &peakQSlider,
        &lowCutFreqSlider,
        &lowCutSlopeSlider,
        &highCutFreqSlider,
        &highCutSlopeSlider
    };
}
