#include "PluginEditor.h"

FourBXAudioProcessorEditor::FourBXAudioProcessorEditor (FourBXAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      gainAttachment (p.apvts, FourBXAudioProcessor::gainParamId, gainSlider)
{
    gainSlider.setTextValueSuffix (" dB");
    addAndMakeVisible (gainSlider);

    gainLabel.setText ("Gain", juce::dontSendNotification);
    gainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (gainLabel);

    setSize (300, 240);
}

void FourBXAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (20.0f));
    g.drawFittedText ("4BX", getLocalBounds().removeFromTop (40), juce::Justification::centred, 1);
}

void FourBXAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    area.removeFromTop (30);
    gainLabel.setBounds (area.removeFromTop (20));
    gainSlider.setBounds (area);
}
