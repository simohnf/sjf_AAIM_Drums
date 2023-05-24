/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#define TEXT_HEIGHT 20
#define INDENT 10
#define SLIDERSIZE 100
#define WIDTH SLIDERSIZE*8 +INDENT*2
//==============================================================================
Sjf_AAIM_DrumsAudioProcessorEditor::Sjf_AAIM_DrumsAudioProcessorEditor (Sjf_AAIM_DrumsAudioProcessor& p, juce::AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor (&p), audioProcessor (p), valueTreeState( vts )
{
    setLookAndFeel( &otherLookAndFeel );
    
    addAndMakeVisible( &compSlider );
    compSlider.setSliderStyle( juce::Slider::Rotary );
    compSliderAttachment.reset( new juce::AudioProcessorValueTreeState::SliderAttachment ( valueTreeState, "complexity", compSlider )  );
    compSlider.setTextBoxStyle( juce::Slider::TextBoxBelow, false, compSlider.getWidth(), TEXT_HEIGHT );
    
    addAndMakeVisible( &restSlider );
    restSlider.setSliderStyle( juce::Slider::Rotary );
    restSliderAttachment.reset( new juce::AudioProcessorValueTreeState::SliderAttachment ( valueTreeState, "rests", restSlider )  );
    restSlider.setTextBoxStyle( juce::Slider::TextBoxBelow, false, restSlider.getWidth(), TEXT_HEIGHT );
    
    addAndMakeVisible( &fillsSlider );
    fillsSlider.setSliderStyle( juce::Slider::Rotary );
    fillsSliderAttachment.reset( new juce::AudioProcessorValueTreeState::SliderAttachment ( valueTreeState, "fills", fillsSlider )  );
    fillsSlider.setTextBoxStyle( juce::Slider::TextBoxBelow, false, fillsSlider.getWidth(), TEXT_HEIGHT );
    
    
    addAndMakeVisible( &nBeatsNumBox );
    nBeatsNumBoxAttachment.reset( new juce::AudioProcessorValueTreeState::SliderAttachment( valueTreeState, "nBeats", nBeatsNumBox ) );
    nBeatsNumBox.sendLookAndFeelChange();
    
    
    addAndMakeVisible( &divisionComboBox );
    for ( size_t i = 0; i < divNames.size(); i++ )
    {
        divisionComboBox.addItem( divNames[ i ], static_cast< int >(i) + 1 );
    }
    divisionComboBoxAttachment.reset( new juce::AudioProcessorValueTreeState::ComboBoxAttachment(valueTreeState, "rhythmicDivision", divisionComboBox) );
    
    
    addAndMakeVisible( &patternMultiTog );
    patternMultiTog.setNumRowsAndColumns( NUM_VOICES, MAX_NUM_STEPS );
    patternMultiTog.onMouseEvent = [this]
    {
        for ( int i = 0; i < patternMultiTog.getNumRows(); i++ )
        {
            audioProcessor.setPattern( i, patternMultiTog.getRow( NUM_VOICES - 1 - i ) );
        }
        
        audioProcessor.setNonAutomatableParameterValues();
    };
    
    for ( int i = 0; i < MAX_NUM_STEPS; i++ )
    {
        auto colour1 = otherLookAndFeel.sliderFillColour;
        if ( i % 4 == 0 )
            patternMultiTog.setColumnColour( i, colour1 );
    }
    setPattern();
    for ( int i = 0; i < NUM_VOICES; i++ )
    {
        auto pat = audioProcessor.getPattern( i );
        auto row = NUM_VOICES - 1 - i;
        for ( size_t j = 0; j < pat.size(); j++ )
            patternMultiTog.setToggleState( row, static_cast<int>(j), pat[ j ] );
    }
    
    
    addAndMakeVisible( &ioiProbsSlider );
    ioiProbsSlider.setNumSliders( static_cast< int >( audioProcessor.ioiFactors.size() ) );
    ioiProbsSlider.onMouseEvent = [this]
    {
        for ( size_t i = 0; i < ioiProbsSlider.getNumSliders(); i++ )
        audioProcessor.setIOIProbability( audioProcessor.ioiFactors[ i ], ioiProbsSlider.fetch( static_cast<int>(i) ) );
        audioProcessor.setNonAutomatableParameterValues();
    };
    for ( int i = 0; i < ioiProbsSlider.getNumSliders(); i++ )
        ioiProbsSlider.setSliderColour( static_cast<int>(i), sliderColours[ i % sliderColours.size()] );
    setIOISliderValues();
    
    
    setSize (WIDTH, 600);
    startTimer( 300 );
}

Sjf_AAIM_DrumsAudioProcessorEditor::~Sjf_AAIM_DrumsAudioProcessorEditor()
{
    setLookAndFeel( nullptr );
}

//==============================================================================
void Sjf_AAIM_DrumsAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("sjf_AAIM_Drums", 0, 0, getWidth(), TEXT_HEIGHT, juce::Justification::centred, 1);
}

void Sjf_AAIM_DrumsAudioProcessorEditor::resized()
{
    compSlider.setBounds( INDENT, TEXT_HEIGHT+INDENT, SLIDERSIZE, SLIDERSIZE);
    restSlider.setBounds( compSlider.getRight(), compSlider.getY(), SLIDERSIZE, SLIDERSIZE);
    fillsSlider.setBounds( restSlider.getRight(), restSlider.getY(), SLIDERSIZE, SLIDERSIZE);
    ioiProbsSlider.setBounds( fillsSlider.getRight(), fillsSlider.getY(), SLIDERSIZE*5, SLIDERSIZE );
    nBeatsNumBox.setBounds( compSlider.getX(), compSlider.getBottom()+INDENT, SLIDERSIZE, TEXT_HEIGHT );
    divisionComboBox.setBounds( restSlider.getX(), restSlider.getBottom()+INDENT, SLIDERSIZE, TEXT_HEIGHT );

    patternMultiTog.setBounds( nBeatsNumBox.getX(), nBeatsNumBox.getBottom(), SLIDERSIZE*8, SLIDERSIZE*4 );
}


void Sjf_AAIM_DrumsAudioProcessorEditor::timerCallback()
{
    if ( audioProcessor.stateLoaded() )
    {
        setPattern();
        setIOISliderValues();
        audioProcessor.setStateLoadedFalse();
    }
}


void Sjf_AAIM_DrumsAudioProcessorEditor::setIOISliderValues()
{
    auto probs = audioProcessor.getIOIProbability();
    for ( size_t i = 0; i < probs.size(); i++ )
    {
        auto sliderNum = 0;
        for ( size_t j = 0; j < audioProcessor.ioiFactors.size(); j++ )
        {
            if ( probs[ i ][ 0 ] == audioProcessor.ioiFactors[ j ] )
            {
                sliderNum = static_cast<int>(j);
                break;
            }
        }
        ioiProbsSlider.setSliderValue( sliderNum, probs[ i ][ 1 ] );
    }
}

void Sjf_AAIM_DrumsAudioProcessorEditor::setPattern()
{
    for ( int i = 0; i < NUM_VOICES; i++ )
    {
        auto pat = audioProcessor.getPattern( i );
        auto row = NUM_VOICES - 1 - i;
        for ( size_t j = 0; j < pat.size(); j++ )
            patternMultiTog.setToggleState( row, static_cast<int>(j), pat[ j ] );
    }

}
