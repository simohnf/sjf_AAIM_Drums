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



class sjf_positionDisplay : public juce::Component
{
public:
    sjf_positionDisplay()
    {
        m_bgColour = juce::Colours::black.withAlpha( 0.2f );
        m_fgColour = juce::Colours::white.withAlpha( 0.2f );
        m_outlineColour = juce::Colours::red.withAlpha( 0.2f );
    }
    ~sjf_positionDisplay(){}
    
    void paint (juce::Graphics& g) override
    {
        g.setColour( m_bgColour );
        g.fillAll( m_bgColour );
        
        g.setColour( m_fgColour );
        auto w = static_cast< float >( getWidth() ) / static_cast< float >( m_nSteps );
        auto r = juce::Rectangle< float >( m_Xpos, 0, w, getHeight() );
        g.fillRect( r );
        
        if (!m_drawOutlineFlag )
            return;
        
        g.setColour( m_outlineColour );
        g.drawRect( getLocalBounds() );
        for ( int i = 1; i < m_nSteps-1; i++ )
            g.drawLine( w*i, 0, w*i, getHeight() );
    }
    
    void setBackGroundColour( juce::Colour c )
    {
        m_bgColour = c;
    }
    
    void setForeGroundColour( juce::Colour c )
    {
        m_fgColour = c;
    }
    
    void setOutlineColour( juce::Colour c )
    {
        m_outlineColour = c;
    }
    
    
    void setCurrentStep( int step )
    {
        m_Xpos = static_cast< float >( getWidth() * step ) / static_cast< float >( m_nSteps );
        DBG("posX " << m_Xpos << " step " << step );
        repaint();
    }
    
    void setNumSteps( int steps )
    {
        m_nSteps = steps;
    }
    
    void shouldDrawOutline( bool trueIfShouldDrawOutline )
    {
        m_drawOutlineFlag = trueIfShouldDrawOutline;
    }
    
private:
    juce::Colour m_bgColour, m_fgColour, m_outlineColour;
    float m_Xpos = 0;
    int m_nSteps = 32;
    bool m_drawOutlineFlag = false;
};

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
    void setPatternMultiTogColours();
    void displayChangedIOI();
    
    juce::AudioProcessorValueTreeState& valueTreeState;
    
    juce::Slider compSlider, restSlider, fillsSlider, swingSlider, bankNumber;
//    sjf_radioButtonSlider bankNumber;
    
    sjf_numBox nBeatsNumBox;
    juce::ComboBox divisionComboBox;
    
    std::unique_ptr< juce::AudioProcessorValueTreeState::SliderAttachment > compSliderAttachment, restSliderAttachment, fillsSliderAttachment, swingSliderAttachment, bankNumberAttachment;
    std::unique_ptr< juce::AudioProcessorValueTreeState::ButtonAttachment > internalSyncResetButtonAttachment;
    
    
    sjf_multitoggle patternMultiTog;
    sjf_positionDisplay posDisplay, bankDisplay;
    sjf_multislider ioiProbsSlider;
    
    sjf_lookAndFeel otherLookAndFeel;
    juce::LookAndFeel_V4 LandF2;
    
    std::array< std::string, 6 > divNames { "/2", "/4", "/8", "/16", "/32", "/64" };
    
    std::array< juce::Colour, 5 > sliderColours
    {
        juce::Colours::darkred, juce::Colours::darkblue, juce::Colours::darkgreen, juce::Colours::darkcyan, juce::Colours::darksalmon
    };
    
    juce::ToggleButton tooltipsToggle, internalSyncResetButton;
    juce::TextButton reverseButton, markovHButton, shuffleButton, palindromeButton, doubleButton, rotateLeftButton, rotateRightButton;
    juce::Label tooltipLabel;
    juce::String MAIN_TOOLTIP = "sjf_AAIM_Drums: \nAlgorithmic variations of drum patterns \n";
    
    juce::Image AAIM_logo = juce::ImageFileFormat::loadFrom( BinaryData::aaim_logo_png, BinaryData::aaim_logo_pngSize );

    
    int m_selectedBank = 0, m_lastStep = -1;
    size_t m_changedIOI = 0;
    bool m_nBeatsDragFlag = false, m_bankFlag = false;
    
    std::array< float, NUM_IOIs > m_ioiProbs;
    juce::Label ioiLabel;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Sjf_AAIM_DrumsAudioProcessorEditor)
};
