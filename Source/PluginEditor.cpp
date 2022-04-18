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
    // Tell our Listener to listen to the main audio processor chain parameters
    const auto& parameters = audioProcessor.getParameters();
    for (auto parameter : parameters)
    {
        parameter->addListener(this);
    }
    
    // Start the timer, update GUI at 60Hz refresh rate
    startTimerHz(60);
    
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
    // Tell our Listener to stop listening to the main audio processor chain parameters
    const auto& parameters = audioProcessor.getParameters();
    for (auto parameter : parameters)
    {
        parameter->removeListener(this);
    }
}

//==============================================================================
void _3BandEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (Colours::tan);
    
    auto bounds = getLocalBounds();
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    auto width = responseArea.getWidth();
    
    auto& peakFilter = monoChain.get<ChainPositions::Peak>();
    auto& lowCutFilter = monoChain.get<ChainPositions::LowCut>();
    auto& highCutFilter = monoChain.get<ChainPositions::HighCut>();
    
    auto sampleRate = audioProcessor.getSampleRate();
    
    // Magnitudes as doubles representing Gain (multiplicative)
    std::vector<double> magnitudes;
    // One magnitude value per pixel within response area width
    magnitudes.resize(width);
    // Calculate the magnitude at each pixel
    for (int i=0; i<width; i++)
    {
        // calculate corresponding frequency for this pixel
        auto freq = mapToLog10(double(i) / double(width), 20.0, 20000.0);
        
        // start with unity gain
        double magnitude = 1.f;
        // multiply the magnitude value by the resulting gain at this frequency...
        // ...from each non-bypassed filter in our processing chain
        // Peak Filter
        if (! monoChain.isBypassed<ChainPositions::Peak>())
            magnitude *= peakFilter.coefficients->getMagnitudeForFrequency(freq, sampleRate);
        // Low Cut Filter
        if (! lowCutFilter.isBypassed<0>())
            magnitude *= lowCutFilter.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (! lowCutFilter.isBypassed<1>())
            magnitude *= lowCutFilter.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (! lowCutFilter.isBypassed<2>())
            magnitude *= lowCutFilter.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (! lowCutFilter.isBypassed<3>())
            magnitude *= lowCutFilter.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        // High Cut Filter
        if (! highCutFilter.isBypassed<0>())
            magnitude *= highCutFilter.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (! highCutFilter.isBypassed<1>())
            magnitude *= highCutFilter.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (! highCutFilter.isBypassed<2>())
            magnitude *= highCutFilter.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (! highCutFilter.isBypassed<3>())
            magnitude *= highCutFilter.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        
        // Convert gain to decibels
        magnitudes[i] = Decibels::gainToDecibels(magnitude);
        
        // Declare our response curve
        Path responseCurve;
        
        const double yMin = responseArea.getBottom();
        const double yMax = responseArea.getY();
        // function that converts decibels to screen coordinates
        auto map = [yMin, yMax](double input)
        {
            return jmap(input, -24.0, 24.0, yMin, yMax);
        };
        
        responseCurve.startNewSubPath( responseArea.getX(), map(magnitudes.front()) );
        
        for (size_t i=1; i<magnitudes.size(); i++)
        {
            responseCurve.lineTo( responseArea.getX() + i, map(magnitudes[i]) );
        }
        
        // draw rounded rectangle border.
        // second argument is corner size. third argument is line thickness
        g.setColour(Colours::orange);
        g.drawRoundedRectangle(responseArea.toFloat(), 4.f, 1.f);
        
        // draw response path
        // second argument is line thickness
        g.setColour(Colours::white);
        g.strokePath(responseCurve, PathStrokeType(2.f));
    }
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

void _3BandEQAudioProcessorEditor::parameterValueChanged(int parameterIndex, float newValue)
{
    // raise our atomic flag
    parametersChanged.set(true);
}

void _3BandEQAudioProcessorEditor::timerCallback()
{
    // if parameters have been changed, lower the flag and enter this block
    if (parametersChanged.compareAndSetBool(false, true))
    {
        // update the mono chain
        auto chainSettings = getChainSettings(audioProcessor.APVTS);
        auto peakCoefficients = makePeakFilter(chainSettings, audioProcessor.getSampleRate());
        updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
        
        // signal a repaint
        repaint();
    }
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
