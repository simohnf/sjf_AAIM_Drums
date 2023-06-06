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
#define HEIGHT TEXT_HEIGHT*3 + INDENT*4 + SLIDERSIZE*5
//==============================================================================
Sjf_AAIM_DrumsAudioProcessorEditor::Sjf_AAIM_DrumsAudioProcessorEditor (Sjf_AAIM_DrumsAudioProcessor& p, juce::AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor (&p), audioProcessor (p), valueTreeState( vts )
{
    
    
    setLookAndFeel( &otherLookAndFeel );
    
    auto multiTogBgCol = findColour( sjf_multitoggle::backgroundColourId ).withAlpha(0.2f);
    auto multiSliBgCol = findColour( sjf_multislider::backgroundColourId ).withAlpha(0.2f);
    
    //-------------------------------------------------
    addAndMakeVisible( &compSlider );
    compSlider.setSliderStyle( juce::Slider::Rotary );
    compSliderAttachment.reset( new juce::AudioProcessorValueTreeState::SliderAttachment ( valueTreeState, "complexity", compSlider )  );
    compSlider.setTextBoxStyle( juce::Slider::TextBoxBelow, false, compSlider.getWidth(), TEXT_HEIGHT );
    compSlider.setTooltip( "This sets the overall probability of different divisions being included in the rhythmic pattern" );
    compSlider.sendLookAndFeelChange();
    
    //-------------------------------------------------
    addAndMakeVisible( &restSlider );
    restSlider.setSliderStyle( juce::Slider::Rotary );
    restSliderAttachment.reset( new juce::AudioProcessorValueTreeState::SliderAttachment ( valueTreeState, "rests", restSlider )  );
    restSlider.setTextBoxStyle( juce::Slider::TextBoxBelow, false, restSlider.getWidth(), TEXT_HEIGHT );
    restSlider.setTooltip( "This sets the probability of rests being inserted into the rhythmic pattern" );
    restSlider.sendLookAndFeelChange();
    
    //-------------------------------------------------
    addAndMakeVisible( &fillsSlider );
    fillsSlider.setSliderStyle( juce::Slider::Rotary );
    fillsSliderAttachment.reset( new juce::AudioProcessorValueTreeState::SliderAttachment ( valueTreeState, "fills", fillsSlider )  );
    fillsSlider.setTextBoxStyle( juce::Slider::TextBoxBelow, false, fillsSlider.getWidth(), TEXT_HEIGHT );
    fillsSlider.setTooltip( "This sets the probability of additional triggers being inserted into the pattern" );
    fillsSlider.sendLookAndFeelChange();
    
    //-------------------------------------------------
    addAndMakeVisible( &nBeatsNumBox );
//    nBeatsNumBoxAttachment.reset( new juce::AudioProcessorValueTreeState::SliderAttachment( valueTreeState, "nBeats", nBeatsNumBox ) );
    nBeatsNumBox.sendLookAndFeelChange();
    nBeatsNumBox.setRange( 1, 32, 1 );
    nBeatsNumBox.setValue( audioProcessor.getNumBeats() );
    nBeatsNumBox.setNumDecimalPlacesToDisplay( 0 );
    nBeatsNumBox.onDragEnd = [this]
    {
        m_nBeatsDragFlag = true;
    };
    nBeatsNumBox.onValueChange = [this]
    {
        if ( audioProcessor.stateLoaded() )
            return;
        audioProcessor.setNumBeats( nBeatsNumBox.getValue() );
        setPatternMultiTogColours();
    };
    nBeatsNumBox.setTooltip( "This sets the number of beats in the pattern" );
    nBeatsNumBox.sendLookAndFeelChange();
    
    //-------------------------------------------------
    addAndMakeVisible( &divisionComboBox );
    for ( size_t i = 0; i < divNames.size(); i++ )
    {
        divisionComboBox.addItem( divNames[ i ], static_cast< int >(i) + 1 );
    }
    divisionComboBox.setSelectedId( audioProcessor.getTsDenominator() );
    divisionComboBox.onChange = [this]
    {
        if ( audioProcessor.stateLoaded() )
            return;
        audioProcessor.setTsDenominator( divisionComboBox.getSelectedId() );
    };
    divisionComboBox.setTooltip( "This sets the underlying pulse of the pattern" );
    
    //-------------------------------------------------
    addAndMakeVisible( &patternMultiTog );
    patternMultiTog.setNumRowsAndColumns( NUM_VOICES, MAX_NUM_STEPS );
    patternMultiTog.setColour( sjf_multislider::backgroundColourId, multiTogBgCol );
    patternMultiTog.onMouseEvent = [this]
    {
        for ( int i = 0; i < patternMultiTog.getNumRows(); i++ )
            audioProcessor.setPattern( i, patternMultiTog.getRow( NUM_VOICES - 1 - i ) );
        audioProcessor.setNonAutomatableParameterValues();
    };
    for ( int i = 0; i < MAX_NUM_STEPS; i++ )
    {
        auto colour1 = otherLookAndFeel.sliderFillColour;
        if ( i % 4 == 0 )
            patternMultiTog.setColumnColour( i, colour1 );
    }
    patternMultiTog.setTooltip( "This is where you set the pattern" );
    setPattern();
    
    //-------------------------------------------------
    bankNumberAttachment.reset( new juce::AudioProcessorValueTreeState::SliderAttachment( valueTreeState, "patternBank", bankNumber ) );
    bankNumber.onValueChange = [this]
    {
        DBG("changed " << bankNumber.getValue() );
        patternBankMultiTog.setAllToggles( false );
        patternBankMultiTog.setToggleState( 0, bankNumber.getValue(), true );
        patternBankMultiTog.onMouseEvent();
    };
    addAndMakeVisible( &patternBankMultiTog );
    patternBankMultiTog.setNumRowsAndColumns( 1, NUM_BANKS );
    patternBankMultiTog.setIsRadioGroup( true );
    patternBankMultiTog.setRowColour( 0, juce::Colours::grey.withAlpha(0.8f) );
    patternBankMultiTog.setColour( sjf_multitoggle::backgroundColourId, multiTogBgCol );
    patternBankMultiTog.setToggleState( 0, bankNumber.getValue(), true );
    m_selectedBank = bankNumber.getValue();
    patternBankMultiTog.onMouseEvent = [this]
    {
        auto selectedBank = patternBankMultiTog.getPressedButton();
        DBG("selected " << selectedBank );
        if ( juce::ModifierKeys::getCurrentModifiers().isShiftDown() )
            audioProcessor.copyPatternBankContents( m_selectedBank, selectedBank );
        m_selectedBank = selectedBank;
        bankNumber.setValue( m_selectedBank );
    };
    patternBankMultiTog.setTooltip( "This allows you to store and recall different patterns \n\nHold the shift key and press on a new bank number to copy a pattern to a new bank" );
    
    //-------------------------------------------------
    addAndMakeVisible( &ioiProbsSlider );
    ioiProbsSlider.setNumSliders( static_cast< int >( audioProcessor.ioiFactors.size() ) );
    ioiProbsSlider.setColour( sjf_multislider::backgroundColourId, multiSliBgCol );
    ioiProbsSlider.onMouseEvent = [this]
    {
        for ( size_t i = 0; i < ioiProbsSlider.getNumSliders(); i++ )
            audioProcessor.setIOIProbability( audioProcessor.ioiFactors[ i ], ioiProbsSlider.fetch( static_cast<int>(i) ) );
        audioProcessor.setNonAutomatableParameterValues();
        displayChangedIOI();
    };
    for ( int i = 0; i < ioiProbsSlider.getNumSliders(); i++ )
        ioiProbsSlider.setSliderColour( static_cast<int>( i ), sliderColours[ i % sliderColours.size() ].withAlpha( 0.6f ) );
    setIOISliderValues();
    ioiProbsSlider.setTooltip( "This sets the probability of different rhythmic divisions. \nThe value for each rhythmic division is the underlying pulse multiplied by the division value (e.g. if the underlying pulse is 8th notes, then the 0.5 division is 16th notes, 0.6666 is 8th note triplets, etc).\nThe overall probability is then set using the Complexity slider" );
    addAndMakeVisible( &ioiLabel );
    ioiLabel.setColour( juce::Slider::backgroundColourId, juce::Colours::white.withAlpha( 0.0f ) );
    ioiLabel.setEditable( false );
    ioiLabel.setInterceptsMouseClicks( false, false );
    ioiLabel.setJustificationType( juce::Justification::topRight );
    
    //-------------------------------------------------
    addAndMakeVisible( &reverseButton );
    reverseButton.setButtonText( "Reverse" );
    reverseButton.onClick = [this]
    {
        audioProcessor.reversePattern();
        setPattern();
    };
    reverseButton.setTooltip( "This will reverse the pattern" );
    
    //-------------------------------------------------
    addAndMakeVisible( &markovHButton );
    markovHButton.setButtonText( "Markov" );
    markovHButton.onClick = [this]
    {
        audioProcessor.markovHorizontal();
        setPattern();
    };
    markovHButton.setTooltip( "This will generate a variation of the current pattern using a markov chain" );
    
    //-------------------------------------------------
    addAndMakeVisible( &shuffleButton );
    shuffleButton.setButtonText( "Shuffle" );
    shuffleButton.onClick = [this]
    {
        audioProcessor.cellShuffleVariation();
        setPattern();
    };
    shuffleButton.setTooltip( "This will shuffle the currently pattern" );
    
    //-------------------------------------------------
    addAndMakeVisible( &palindromeButton );
    palindromeButton.setButtonText( "Palindrome" );
    palindromeButton.onClick = [this]
    {
        audioProcessor.palindromeVariation();
        nBeatsNumBox.setValue( audioProcessor.getNumBeats() );
        setPattern();
    };
    palindromeButton.setTooltip( "This will create a palindrome of the current pattern" );
    
    //-------------------------------------------------
    
    addAndMakeVisible( &doubleButton );
    doubleButton.setButtonText( "Double" );
    doubleButton.onClick = [this]
    {
        audioProcessor.doublePattern();
        nBeatsNumBox.setValue( audioProcessor.getNumBeats() );
        setPattern();
    };
    doubleButton.setTooltip( "This will double the current pattern" );
    
    //-------------------------------------------------
    addAndMakeVisible( &rotateLeftButton );
    rotateLeftButton.setButtonText( "<--" );
    rotateLeftButton.onClick = [this]
    {
        audioProcessor.rotatePattern( true );
        setPattern();
    };
    rotateLeftButton.setTooltip( "This will rotate the current pattern one step to the left" );
    
    //-------------------------------------------------
    
    addAndMakeVisible( &rotateRightButton );
    rotateRightButton.setButtonText( "-->" );
    rotateRightButton.onClick = [this]
    {
        audioProcessor.rotatePattern( false );
        setPattern();
    };
    rotateRightButton.setTooltip( "This will rotate the current pattern one step to the right" );
    
    //-------------------------------------------------
    addAndMakeVisible( &tooltipsToggle );
    tooltipsToggle.setTooltip( MAIN_TOOLTIP );
    tooltipsToggle.setButtonText( "HINTS" );
    tooltipsToggle.onClick = [this]
    {
        if (tooltipsToggle.getToggleState())
        {
            tooltipLabel.setVisible( true );
            setSize (WIDTH, HEIGHT+tooltipLabel.getHeight());
        }
        else
        {
            tooltipLabel.setVisible( false );
            setSize (WIDTH, HEIGHT);
        }
    };
    
    //-------------------------------------------------
    addAndMakeVisible(tooltipLabel);
    tooltipLabel.setVisible( false );
    tooltipLabel.setColour( juce::Label::backgroundColourId, otherLookAndFeel.backGroundColour.withAlpha( 0.85f ) );
    tooltipLabel.setTooltip( MAIN_TOOLTIP );
    
    for ( size_t i = 0; i < NUM_IOIs; i++ )
        m_ioiProbs[ 0 ] = 0;
     
    //-------------------------------------------------
    addAndMakeVisible( &posDisplay );
    posDisplay.setInterceptsMouseClicks( false, false );
    posDisplay.setBackGroundColour( juce::Colours::white.withAlpha( 0.0f ) );
    posDisplay.setForeGroundColour( juce::Colours::darkred.withAlpha( 0.2f ) );
    
    setSize (WIDTH, HEIGHT);
    startTimer( 50 );
}

Sjf_AAIM_DrumsAudioProcessorEditor::~Sjf_AAIM_DrumsAudioProcessorEditor()
{
    setLookAndFeel( nullptr );
}

//==============================================================================
void Sjf_AAIM_DrumsAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
#ifdef JUCE_DEBUG
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
#else
    juce::Rectangle<int> r = { (int)( WIDTH ), (int)(HEIGHT + tooltipLabel.getHeight()) };
    sjf_makeBackground< 40 >( g, r );
#endif
    
    g.setColour ( juce::Colours::white.withAlpha(0.2f) );
    g.drawImage( AAIM_logo, patternMultiTog.getX(), patternMultiTog.getY(), patternMultiTog.getWidth(), patternMultiTog.getHeight(), 0, 0, AAIM_logo.getWidth(), AAIM_logo.getHeight() );
    
    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ( "sjf_AAIM_Drums", 0, 0, getWidth(), TEXT_HEIGHT, juce::Justification::centred, 1);
    
    g.drawFittedText ( "Complexity", compSlider.getX(), compSlider.getY() - TEXT_HEIGHT, compSlider.getWidth(), TEXT_HEIGHT, juce::Justification::centred, 1);
    g.drawFittedText ( "Rests", restSlider.getX(), restSlider.getY() - TEXT_HEIGHT, restSlider.getWidth(), TEXT_HEIGHT, juce::Justification::centred, 1);
    g.drawFittedText ( "Fills", fillsSlider.getX(), fillsSlider.getY() - TEXT_HEIGHT, fillsSlider.getWidth(), TEXT_HEIGHT, juce::Justification::centred, 1);
    g.drawFittedText ( "Rhythmic Division Probabilities", ioiProbsSlider.getX(), ioiProbsSlider.getY() - TEXT_HEIGHT, ioiProbsSlider.getWidth(), TEXT_HEIGHT, juce::Justification::centred, 1);
    g.drawFittedText ( "Time Signature: ", nBeatsNumBox.getX()-SLIDERSIZE, nBeatsNumBox.getY(), SLIDERSIZE, TEXT_HEIGHT, juce::Justification::right, 1);
    for ( int i = 0; i < NUM_BANKS; i++ )
    {
        auto w = patternBankMultiTog.getWidth()/NUM_BANKS;
        auto x = patternBankMultiTog.getX() + w*i;
        g.drawFittedText ( juce::String(i), x, patternBankMultiTog.getY(), w, TEXT_HEIGHT, juce::Justification::centred, 1);
    }
}

void Sjf_AAIM_DrumsAudioProcessorEditor::resized()
{
    compSlider.setBounds( INDENT, TEXT_HEIGHT*2, SLIDERSIZE, SLIDERSIZE);
    restSlider.setBounds( compSlider.getRight(), compSlider.getY(), SLIDERSIZE, SLIDERSIZE);
    fillsSlider.setBounds( restSlider.getRight(), restSlider.getY(), SLIDERSIZE, SLIDERSIZE);
    ioiProbsSlider.setBounds( fillsSlider.getRight(), fillsSlider.getY(), SLIDERSIZE*5, SLIDERSIZE );
    ioiLabel.setBounds( ioiProbsSlider.getX(), ioiProbsSlider.getY(), ioiProbsSlider.getWidth(), ioiProbsSlider.getHeight() );
    
    
    nBeatsNumBox.setBounds( restSlider.getX(), restSlider.getBottom()+INDENT, SLIDERSIZE/2, TEXT_HEIGHT );
    divisionComboBox.setBounds( nBeatsNumBox.getRight(), nBeatsNumBox.getY(), SLIDERSIZE/2, TEXT_HEIGHT );
    reverseButton.setBounds( divisionComboBox.getRight(), divisionComboBox.getY(), SLIDERSIZE/2, TEXT_HEIGHT );
    markovHButton.setBounds( reverseButton.getRight(), reverseButton.getY(), SLIDERSIZE/2, TEXT_HEIGHT );
    shuffleButton.setBounds( markovHButton.getRight(), markovHButton.getY(), SLIDERSIZE/2, TEXT_HEIGHT );
    palindromeButton.setBounds( shuffleButton.getRight(), shuffleButton.getY(), SLIDERSIZE/2, TEXT_HEIGHT );
    doubleButton.setBounds( palindromeButton.getRight(), palindromeButton.getY(), SLIDERSIZE/2, TEXT_HEIGHT );
    rotateLeftButton.setBounds( doubleButton.getRight(), doubleButton.getY(), SLIDERSIZE/2, TEXT_HEIGHT );
    rotateRightButton.setBounds( rotateLeftButton.getRight(), rotateLeftButton.getY(), SLIDERSIZE/2, TEXT_HEIGHT );
    
    tooltipsToggle.setBounds( ioiProbsSlider.getRight() - SLIDERSIZE, divisionComboBox.getY(), SLIDERSIZE, TEXT_HEIGHT );
    
//    posDisplay.setBounds( compSlider.getX(), nBeatsNumBox.getBottom(), SLIDERSIZE*8, TEXT_HEIGHT );
//    patternMultiTog.setBounds( posDisplay.getX(), posDisplay.getBottom(), SLIDERSIZE*8, SLIDERSIZE*4 );
    patternMultiTog.setBounds( compSlider.getX(), nBeatsNumBox.getBottom(), SLIDERSIZE*8, SLIDERSIZE*4 );
    posDisplay.setBounds( patternMultiTog.getBounds() );
    patternBankMultiTog.setBounds( patternMultiTog.getX(), patternMultiTog.getBottom(), patternMultiTog.getWidth(), TEXT_HEIGHT );
    
    tooltipLabel.setBounds( 0, HEIGHT, WIDTH, TEXT_HEIGHT*4 );
}


void Sjf_AAIM_DrumsAudioProcessorEditor::timerCallback()
{
    if ( audioProcessor.stateLoaded() )
    {
        nBeatsNumBox.setValue( audioProcessor.getNumBeats() );
        divisionComboBox.setSelectedId( audioProcessor.getTsDenominator() );
        setPattern();
        setIOISliderValues();
        setPatternMultiTogColours();
        patternBankMultiTog.setAllToggles( false );
        patternBankMultiTog.setToggleState( 0, bankNumber.getValue(), true );
        audioProcessor.setStateLoadedFalse();
    }
    auto step = audioProcessor.getCurrentStep();
    if ( step >= 0 && step != m_lastStep )
    {
        posDisplay.setCurrentStep( step );
        m_lastStep = step;
    }
    if ( tooltipsToggle.getToggleState() )
        sjf_setTooltipLabel( this, MAIN_TOOLTIP, tooltipLabel );
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
    patternMultiTog.setAllToggles( false );
    for ( int i = 0; i < NUM_VOICES; i++ )
    {
        auto pat = audioProcessor.getPattern( i );
        auto row = NUM_VOICES - 1 - i;
        for ( size_t j = 0; j < pat.size(); j++ )
            patternMultiTog.setToggleState( row, static_cast<int>(j), pat[ j ] );
    }
    displayChangedIOI();
}


void Sjf_AAIM_DrumsAudioProcessorEditor::setPatternMultiTogColours()
{
    for ( int i = 0; i < MAX_NUM_STEPS; i++ )
    {
        auto colour1 = otherLookAndFeel.sliderFillColour;
        if( i >= nBeatsNumBox.getValue() )
            patternMultiTog.setColumnColour( i, juce::Colours::darkgrey );
        else if ( i % 4 == 0 )
            patternMultiTog.setColumnColour( i, colour1 );
        else
            patternMultiTog.setColumnColour( i, findColour( juce::ToggleButton::tickColourId ) );
    }
}


void Sjf_AAIM_DrumsAudioProcessorEditor::displayChangedIOI()
{
    auto probs = audioProcessor.getIOIProbability();
    for ( size_t i = 0; i < probs.size(); i++ )
    {
        if ( m_ioiProbs[ i ] != probs[ i ][ 1 ] )
        {
            m_ioiProbs[ i ] = probs[ i ][ 1 ];
            m_changedIOI = i;
            ioiLabel.setText( "division:"+juce::String( probs[ i ][ 0 ] ) + " chance:" + juce::String( probs[ i ][ 1 ] ), juce::dontSendNotification );
        }
    }
}
