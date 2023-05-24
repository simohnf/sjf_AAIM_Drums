/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//#include "../../sjf_audio/sjf_widgets.h"
//#include "../../sjf_audio/sjf_LookAndFeel.h"
#include "../sjf_AAIM_Cplusplus/sjf_audio/sjf_widgets.h"
#include "../sjf_AAIM_Cplusplus/sjf_audio/sjf_LookAndFeel.h"
//==============================================================================
/**
*/
class Sjf_AAIM_DrumsAudioProcessorEditor  : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    Sjf_AAIM_DrumsAudioProcessorEditor ( Sjf_AAIM_DrumsAudioProcessor&, juce::AudioProcessorValueTreeState& vts );
    ~Sjf_AAIM_DrumsAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    void timerCallback() override;
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    Sjf_AAIM_DrumsAudioProcessor& audioProcessor;
    
    void setIOISliderValues();
    void setPattern();
    juce::AudioProcessorValueTreeState& valueTreeState;
    
    juce::Slider compSlider, restSlider, fillsSlider;
    sjf_numBox nBeatsNumBox;
    juce::ComboBox divisionComboBox;
    
//    std::unique_ptr< juce::AudioProcessorValueTreeState::ButtonAttachment > filterOnOffButtonAttachment;
    std::unique_ptr< juce::AudioProcessorValueTreeState::SliderAttachment > compSliderAttachment, restSliderAttachment, fillsSliderAttachment, nBeatsNumBoxAttachment;
    std::unique_ptr< juce::AudioProcessorValueTreeState::ComboBoxAttachment > divisionComboBoxAttachment;
    
    sjf_multitoggle patternMultiTog;
    sjf_multislider ioiProbsSlider;
    sjf_lookAndFeel otherLookAndFeel;
    
    std::array< std::string, 6 > divNames { "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" };
    
    std::array< juce::Colour, 5 > sliderColours
    {
        juce::Colours::darkred, juce::Colours::darkblue, juce::Colours::darkgreen, juce::Colours::darkcyan, juce::Colours::darksalmon
    };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Sjf_AAIM_DrumsAudioProcessorEditor)
};
