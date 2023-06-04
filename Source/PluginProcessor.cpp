/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Sjf_AAIM_DrumsAudioProcessor::Sjf_AAIM_DrumsAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor ( getBusesLayout()
                       /* BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif */
                       )
, parameters( *this, nullptr, juce::Identifier("sjf_AAIM_Drums"), createParameterLayout() )
#endif
{
    DBG( "AAIM Drums" );
    
    nBeatsParameter = parameters.getRawParameterValue("nBeats");
    tsDenominatorParameter = parameters.getRawParameterValue("tsDenominator");
    midiChannelParameter = parameters.getRawParameterValue( "midiChannel");
    complexityParameter = parameters.getRawParameterValue("complexity");
    restsParameter = parameters.getRawParameterValue("rests");
    fillsParameter = parameters.getRawParameterValue("fills");
    bankNumberParameter = parameters.getRawParameterValue("patternBank");
    
    for( size_t i = 0; i < patternParameters.size(); i++ )
        patternParameters[ i ] = parameters.state.getPropertyAsValue( "pattern" + juce::String(i), nullptr, true );
    for ( size_t i = 0; i < NUM_IOIs; i++ )
    {
        ioiDivParameters[ i ] = parameters.state.getPropertyAsValue( "ioiDiv" + juce::String(i), nullptr, true );
        ioiProbParameters[ i ] = parameters.state.getPropertyAsValue( "ioiProb" + juce::String(i), nullptr, true );
    }
    
    for ( size_t i = 0; i < NUM_BANKS; i++ )
    {
        for ( size_t j = 0; j < NUM_VOICES; j++ )
            patternBanksParameters[ i ][ j ] = parameters.state.getPropertyAsValue( "patternBank" + juce::String( i ) + "Voice" + juce::String( j ), nullptr, true );
        patternBanksNumBeatsParameters[ i ] = parameters.state.getPropertyAsValue( "patternBankNumBeats" + juce::String( i ), nullptr, true );
        divBanksParameters[ i ] = parameters.state.getPropertyAsValue( "divisionBank" + juce::String( i ), nullptr, true );
    }
    
    
    for ( size_t i = 0; i < NUM_BANKS; i++ )
    {
        m_nBeatsBanks[ i ] = m_rGen.getNumBeats();
        m_divBanks[ i ] = eightNote;
        for ( size_t j = 0; j < NUM_VOICES; j++ )
            m_patternBanks[ i ][ j ].reset();
    }

    setParameters();
}

Sjf_AAIM_DrumsAudioProcessor::~Sjf_AAIM_DrumsAudioProcessor()
{
}

//==============================================================================
const juce::String Sjf_AAIM_DrumsAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool Sjf_AAIM_DrumsAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool Sjf_AAIM_DrumsAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool Sjf_AAIM_DrumsAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double Sjf_AAIM_DrumsAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int Sjf_AAIM_DrumsAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int Sjf_AAIM_DrumsAudioProcessor::getCurrentProgram()
{
    return 0;
}

void Sjf_AAIM_DrumsAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String Sjf_AAIM_DrumsAudioProcessor::getProgramName (int index)
{
    return {};
}

void Sjf_AAIM_DrumsAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void Sjf_AAIM_DrumsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void Sjf_AAIM_DrumsAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Sjf_AAIM_DrumsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void Sjf_AAIM_DrumsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    setParameters();
    juce::ScopedNoDenormals noDenormals;
    buffer.clear(); // remove any noise in buffer...
    auto bufferSize = buffer.getNumSamples();
    
    midiMessages.clear(); // clear midi messages
    playHead = this->getPlayHead();
    // if there is an available playhead
    if (playHead != nullptr )
    {
        positionInfo = *playHead->getPosition();
        if ( positionInfo.getIsPlaying() && positionInfo.getBpm() && positionInfo.getTimeInSamples() )
        {
            auto beatDivFactor = std::pow( 2, static_cast< double >(m_tsDenominator - 2) ); // multiple for converting from quarterNotes to other beat types
            auto nBeats = static_cast< double >(m_rGen.getNumBeats());
            auto timeInSamps = static_cast< double >(*positionInfo.getTimeInSamples());
            auto bpm = *positionInfo.getBpm();
            auto sr = getSampleRate();
            // convert sample position to position on timeline in the underlying rhythmic division of the drumMachine
            auto pos = ( (bpm * beatDivFactor * timeInSamps) / ( sr * static_cast< double >(60) ) );
            // calculate the increment per sample
            auto increment = (static_cast<double>(bpm) * beatDivFactor )/ ( sr * static_cast< double >(60) );
            auto currentBeat = pos;
            for ( int i = 0; i < bufferSize; i++ )
            {
                currentBeat = fastMod4< double >( ( pos + ( i * increment )), nBeats );
                if ( static_cast<int>( currentBeat ) != m_currentStep )
                {
                    setPatternBankSelection();
                    m_currentStep = static_cast< int >( currentBeat );
                }
                auto genOut = m_rGen.runGenerator( currentBeat );
                if ( genOut[ 0 ] < m_lastRGenPhase*0.5 ) // just a debounce check, it's possible to go backwards, but it has to go a good way
                {
                    for ( size_t j = 0; j < m_pVary.size(); j++ )
                    {
                        auto noteOff = juce::MidiMessage::noteOff( m_midiChannel, static_cast< int >(j)+36, 0.0f );
                        midiMessages.addEvent( noteOff, i );
                        // check if current beat is a rest, check if voice should output trigger
                        if ( genOut[ 2 ] > 0 && m_pVary[ j ].triggerBeat( currentBeat, genOut[ 4 ] ) )
                        {
                            auto note = juce::MidiMessage::noteOn( m_midiChannel, static_cast< int >(j)+36, genOut[ 1 ] );
                            midiMessages.addEvent( note, i );
                        }
                    }
                }
                m_lastRGenPhase = genOut[ 0 ];
            }
//            m_currentStep = static_cast< int >( currentBeat );
        }
    }
}

//==============================================================================
bool Sjf_AAIM_DrumsAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* Sjf_AAIM_DrumsAudioProcessor::createEditor()
{
    return new Sjf_AAIM_DrumsAudioProcessorEditor ( *this, parameters ); 
}

//==============================================================================
void Sjf_AAIM_DrumsAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    setNonAutomatableParameterValues();
    auto state = parameters.copyState();
    
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void Sjf_AAIM_DrumsAudioProcessor::setNonAutomatableParameterValues()
{
    for ( size_t i = 0; i < NUM_VOICES; i++ )
    {
        juce::String str;
        auto pat = m_pVary[ i ].getPattern();
        for ( size_t j = 0; j < pat.size(); j++ )
        {
            str += pat[ j ] ? juce::String( 1 ) : juce::String( 0 );
        }
        patternParameters[ i ].setValue( str );
    }
    // set IOI divs and probabilities
    {
        auto IOIs = m_rGen.getIOIProbabilities();
        for ( size_t i = 0; i < IOIs.size(); i++ )
        {
            ioiDivParameters[ i ].setValue( IOIs[ i ][ 0 ] );
            ioiProbParameters[ i ].setValue( IOIs[ i ][ 1 ] );
        }
        
        for ( size_t i = 0; i < NUM_BANKS; i++ )
        {
            for ( size_t j = 0; j < NUM_VOICES; j++ )
                patternBanksParameters[ i ][ j ].setValue( static_cast<double>( m_patternBanks[ i ][ j ].to_ullong()) );
            patternBanksNumBeatsParameters[ i ].setValue( static_cast<double>( m_nBeatsBanks[ i ] ) );
            divBanksParameters[ i ].setValue( static_cast< double >( m_divBanks[ i ] ) );
        }
    }
    updateHostDisplay();
}

void Sjf_AAIM_DrumsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName (parameters.state.getType()))
        {
            parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
            
            for ( size_t i = 0; i < NUM_VOICES; i++ )
            {
                patternParameters[ i ].referTo( parameters.state.getPropertyAsValue ( "pattern"+juce::String(i), nullptr, true ) );
                juce::String str ( patternParameters[ i ].getValue() );
                for ( int j = 0; j < str.length(); j++ )
                {
                    auto substr = juce::String::charToString(str[ j ]).getIntValue();
                    auto trig =   substr > 0 ? true : false;
                    m_pVary[ i ].setBeat( j, trig );
                }
            }

            for ( size_t i = 0; i < NUM_BANKS; i++ )
            {
                for ( size_t j = 0; j < NUM_VOICES; j++ )
                {
                    patternBanksParameters[ i ][ j ].referTo( parameters.state.getPropertyAsValue( "patternBank" + juce::String( i ) + "Voice" + juce::String( j ), nullptr, true ) );
                    m_patternBanks[ i ][ j ] = static_cast< long long >( patternBanksParameters[ i ][ j ].getValue() );
                }
                patternBanksNumBeatsParameters[ i ].referTo( parameters.state.getPropertyAsValue( "patternBankNumBeats" + juce::String( i ), nullptr, true ) );
                m_nBeatsBanks[ i ] = static_cast< long long >( patternBanksNumBeatsParameters[ i ].getValue() );
                
                divBanksParameters[ i ].referTo( parameters.state.getPropertyAsValue( "divisionBank" + juce::String( i ), nullptr, true ) );
                m_divBanks[ i ] = static_cast< long long >( divBanksParameters[ i ].getValue() );
            }
            
            for ( size_t i = 0; i < NUM_IOIs; i++ )
            {
                ioiDivParameters[ i ].referTo( parameters.state.getPropertyAsValue( "ioiDiv"+juce::String(i), nullptr, true ) );
                ioiProbParameters[ i ].referTo( parameters.state.getPropertyAsValue( "ioiProb"+juce::String(i), nullptr, true ) );
                auto div = ioiDivParameters[ i ].getValue();
                auto prob = ioiProbParameters[ i ].getValue();
                m_rGen.setIOIProbability( div, prob );
            }
        }
        selectPatternBank( *bankNumberParameter );
        m_patternBank = *bankNumberParameter;
        setParameters();
        m_stateLoadedFlag = true;
    }
}
//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout Sjf_AAIM_DrumsAudioProcessor::createParameterLayout()
{
    static constexpr int pIDVersionNumber = 1;
    juce::AudioProcessorValueTreeState::ParameterLayout params;
    params.add( std::make_unique<juce::AudioParameterInt>( juce::ParameterID{ "nBeats", pIDVersionNumber }, "NBeats", 1, MAX_NUM_STEPS, 8 ) );
    params.add( std::make_unique<juce::AudioParameterFloat>( juce::ParameterID{ "complexity", pIDVersionNumber }, "Complexity", 0, 1, 0 ) );
    params.add( std::make_unique<juce::AudioParameterFloat>( juce::ParameterID{ "rests", pIDVersionNumber }, "Rests", 0, 1, 0 ) );
    params.add( std::make_unique<juce::AudioParameterFloat>( juce::ParameterID{ "fills", pIDVersionNumber }, "Fills", 0, 1, 0 ) );
    params.add( std::make_unique<juce::AudioParameterInt>( juce::ParameterID{ "tsDenominator", pIDVersionNumber }, "TsDenominator", halfNote, sixtyFourthNote, eightNote ) );
    params.add( std::make_unique<juce::AudioParameterInt>( juce::ParameterID{ "midiChannel", pIDVersionNumber }, "MidiChannel", 1, 16, 1 ) );
    params.add( std::make_unique<juce::AudioParameterInt>( juce::ParameterID{ "patternBank", pIDVersionNumber }, "PatternBank", 0, 15, 0 ) );
    return params;
}

void Sjf_AAIM_DrumsAudioProcessor::setPattern( int row, std::vector<bool> pattern )
{
    for ( size_t i = 0; i < pattern.size(); i++ )
        m_pVary[ row ].setBeat( i, pattern[ i ] );
}

const std::vector<bool>& Sjf_AAIM_DrumsAudioProcessor::getPattern( int row )
{
    return m_pVary[ row ].getPattern();
}
//==============================================================================
void Sjf_AAIM_DrumsAudioProcessor::setParameters()
{
    setPatternBankSelection();
    m_rGen.setComplexity( *complexityParameter );
    m_rGen.setRests( *restsParameter );
    m_rGen.setNumBeats( *nBeatsParameter );
    for (size_t i = 0; i < m_pVary.size(); i++ )
    {
        m_pVary[ i ].setNumBeats( m_rGen.getNumBeats() );
        m_pVary[ i ].setFills( *fillsParameter );
    }
    m_tsDenominator = *tsDenominatorParameter;
    m_midiChannel = *midiChannelParameter;
}

void Sjf_AAIM_DrumsAudioProcessor::setPatternBankSelection()
{
    if ( !m_hasEditorFlag )
    {
        if ( m_patternBank != *bankNumberParameter )
            m_patternBankState = loadButDontSave;
        else
            m_patternBankState = doNothing;
    }
    switch ( m_patternBankState )
    {
        case ( doNothing ):
            m_patternBankState = doNothing;
            break;
        case ( reloadBank ):
            DBG( "Reload " << m_patternBank );
            selectPatternBank( m_patternBank );
            m_patternBank = *bankNumberParameter;
            m_patternBankState = doNothing;
            break;
        case ( saveAndLoadNewBank ):
            DBG( "Save " << m_patternBank << " & Load " << *bankNumberParameter );
            if ( m_patternBank != *bankNumberParameter )
            {
                DBG("NOT EQUAL SAVE");
                setPatternBankContents( m_patternBank );
            }
            m_patternBank = *bankNumberParameter;
            selectPatternBank( m_patternBank );
            m_patternBankState = doNothing;
            m_stateLoadedFlag = true;
            break;
        case ( copyToNewBank ):
            DBG( "Copy " << m_patternBank << " to " << *bankNumberParameter );
            copyPatternBankContents( m_patternBank, *bankNumberParameter );
            selectPatternBank( *bankNumberParameter );
            m_patternBank = *bankNumberParameter;
            m_patternBankState = doNothing;
            m_stateLoadedFlag = true;
            break;
        case ( loadButDontSave ):
            DBG( "Load but don't save " << *bankNumberParameter );
            selectPatternBank( *bankNumberParameter );
            m_patternBank = *bankNumberParameter;
            m_patternBankState = doNothing;
            m_stateLoadedFlag = true;
            break;
        default:
            m_patternBankState = doNothing;
            break;
    }
    
//    if ( m_patternBank != *bankNumberParameter)
//    {
//        if ( m_bankSaveFlag )
//        {
//            setPatternBankContents( m_patternBank );
//            m_bankSaveFlag = false;
//        }
//        selectPatternBank( *bankNumberParameter );
//        m_patternBank = *bankNumberParameter;
//        m_stateLoadedFlag = true;
//    }
}

void Sjf_AAIM_DrumsAudioProcessor::setPatternBankContents( size_t bankNumber )
{
    for ( size_t i = 0; i < NUM_VOICES; i++ )
        m_patternBanks[ bankNumber ][ i ] = m_pVary[ i ].getPatternLong();
    m_nBeatsBanks[ bankNumber ] = *nBeatsParameter;
    m_divBanks[ bankNumber ] = *tsDenominatorParameter;
}

void Sjf_AAIM_DrumsAudioProcessor::selectPatternBank( size_t bankNumber )
{
    auto div = m_divBanks[ bankNumber ];
    parameters.state.setProperty( "tsDenominator", static_cast<int>(div), nullptr );
//    parameters.state
    //    *tsDenominatorParameter = div;
    m_tsDenominator = static_cast<int>(div);
    auto nBeats = m_nBeatsBanks[ bankNumber ];
    parameters.state.setProperty( "nBeats", static_cast<int>(nBeats), nullptr );
//    *nBeatsParameter = nBeats;
    m_rGen.setNumBeats( nBeats );
    for ( size_t i = 0; i < NUM_VOICES; i++ )
    {
        m_pVary[ i ].setNumBeats( nBeats );
        for ( size_t j = 0; j < nBeats; j++ )
            m_pVary[ i ].setBeat( j, m_patternBanks[ bankNumber ][ i ][ j ] );
    }
    updateHostDisplay();
    m_stateLoadedFlag = true;
}

void Sjf_AAIM_DrumsAudioProcessor::copyPatternBankContents( size_t bankToCopyFrom, size_t bankToCopyTo )
{
    if ( bankToCopyFrom < 0 || bankToCopyTo < 0 || bankToCopyFrom > NUM_BANKS-1 || bankToCopyTo > NUM_BANKS-1 )
        return;
    for ( size_t i = 0; i < NUM_VOICES; i++ )
        m_patternBanks[ bankToCopyTo ][ i ] = m_patternBanks[ bankToCopyFrom ][ i ];
    m_nBeatsBanks[ bankToCopyTo ] = m_nBeatsBanks[ bankToCopyFrom ];
    m_divBanks[ bankToCopyTo ] = m_divBanks[ bankToCopyFrom ];
}
//==============================================================================
//      ALGORITHMIC VARIATIONS
void Sjf_AAIM_DrumsAudioProcessor::reversePattern()
{
    auto nBeats = m_rGen.getNumBeats();
    for ( size_t i = 0; i < m_pVary.size(); i++ )
    {
        auto pat = std::bitset< MAX_NUM_STEPS >( m_pVary[ i ].getPatternLong() );
        
        for ( size_t j = 0; j < nBeats; j++ )
        {
            m_pVary[ i ].setBeat( nBeats - j - 1, pat[ j ] );
        }
    }
}


void Sjf_AAIM_DrumsAudioProcessor::markovHorizontal()
{
    // create a transition table for each voice
    // then pass each into markov chain
    auto nBeats = m_rGen.getNumBeats();
    auto nVoices = m_pVary.size();
    for ( size_t i = 0; i < nVoices; i++ )
    {
        auto transitionTable = std::array< std::array < int, 2 >, 2 >{ { { 0, 0 }, { 0, 0 } } };
        auto pat = std::bitset< MAX_NUM_STEPS >( m_pVary[ i ].getPatternLong() );
        for ( size_t j = 0; j < nBeats; j++ )
        {
            auto bit = pat[ j ] ? 1 : 0;
            auto nextStep = ( j + 1 ) % nBeats;
            auto nextBit = pat[ nextStep ] ? 1 : 0;
            transitionTable[ bit ][ nextBit ] += 1;
        }
        auto newPat = std::bitset< MAX_NUM_STEPS >();
        newPat.reset();
        auto totals = std::array < int, 2 >{ { 0, 0 } };
        totals[ 0 ] = transitionTable[ 0 ][ 0 ] + transitionTable[ 1 ][ 0 ];
        totals[ 1 ] = transitionTable[ 0 ][ 1 ] + transitionTable[ 1 ][ 1 ];
        auto rnd = rand01() * (totals[ 0 ] + totals[ 1 ]);
        auto trig = ( rnd < totals[ 0 ] ) ? false : true;
        
        for ( size_t j = 0; j < nBeats; j++ )
        {
            m_pVary[ i ].setBeat( j, trig );
            rnd = rand01() * ( transitionTable[ trig ][ 0 ] + transitionTable[ trig ][ 1 ]);
            trig = ( rnd < transitionTable[ trig ][ 0 ] ) ? false : true;
        }
        
    }
}

//void Sjf_AAIM_DrumsAudioProcessor::markovVertical()
//{
//
//}

void Sjf_AAIM_DrumsAudioProcessor::cellShuffleVariation()
{
//    auto nBeats = m_rGen.getNumBeats();
//    auto nVoices = m_pVary.size();
    auto indis = m_rGen.getBaseindispensability();
    std::vector < size_t > steps{ };
    auto count = 1ul;
    for ( size_t i = 1; i < indis.size() -1; i++ )
    {
        if ( indis[ i ] > indis[ i - 1 ] && indis[ i ] > indis[ i + 1 ] )
        {
            steps.push_back( count );
            count = 1;
        }
        else
        {
            count += 1;
        }
    }
    steps.push_back( count + 1 );
    std::vector< std::vector< std::bitset< NUM_VOICES > > > cells;
    cells.reserve( steps.size() );
    count = 0;
    for ( size_t i = 0; i < steps.size(); i++ )
    {
        std::vector< std::bitset< NUM_VOICES > > cell;
        cell.reserve( steps[ i ] );
        for ( size_t j = 0; j < steps[ i ]; j++ )
        {
            std::bitset< NUM_VOICES > step;
            step.reset();
            for ( size_t k = 0; k < NUM_VOICES; k++ )
            {
                step[ k ] = m_pVary[ k ].getStep( count );
            }
            cell.emplace_back( step );
            count += 1;
        }
        cells.emplace_back( cell );
    }
    // shuffle cells
    auto rd = std::random_device{};
    auto rng = std::default_random_engine{ rd() };
    std::shuffle(std::begin( cells ), std::end( cells ), rng);
    
    count = 0;
    for ( size_t i = 0; i < cells.size(); i++ )
        for ( size_t j = 0; j < cells[ i ].size(); j++ )
        {
            for ( size_t k = 0; k < NUM_VOICES; k++ )
                m_pVary[ k ].setBeat( count, cells[ i ][ j ][ k ] );
            count += 1;
        }
}

void Sjf_AAIM_DrumsAudioProcessor::palindromeVariation()
{
    auto nBeats = m_rGen.getNumBeats() * 2;
    nBeats = ( nBeats > MAX_NUM_STEPS ) ? MAX_NUM_STEPS : nBeats;
    m_rGen.setNumBeats( nBeats );
    *nBeatsParameter = nBeats;
    for ( size_t i = 0; i < m_pVary.size(); i++ )
    {
        m_pVary[ i ].setNumBeats( nBeats );
        for ( size_t j = 0; j < nBeats/2; j++ )
        {
            m_pVary[ i ].setBeat( nBeats - 1 - j, m_pVary[ i ].getStep( j ) );
        }
    }
}


void Sjf_AAIM_DrumsAudioProcessor::doublePattern()
{
    auto nBeats = m_rGen.getNumBeats() * 2;
    nBeats = ( nBeats > MAX_NUM_STEPS ) ? MAX_NUM_STEPS : nBeats;
    m_rGen.setNumBeats( nBeats );
    *nBeatsParameter = nBeats;
    for ( size_t i = 0; i < m_pVary.size(); i++ )
    {
        m_pVary[ i ].setNumBeats( nBeats );
        for ( size_t j = 0; j < nBeats/2; j++ )
        {
            m_pVary[ i ].setBeat( nBeats/2 + j, m_pVary[ i ].getStep( j ) );
        }
    }
}


void Sjf_AAIM_DrumsAudioProcessor::rotatePattern( bool trueIfLeftFalseIfRight)
{
    auto nBeats = m_rGen.getNumBeats();
    for ( size_t i = 0; i < m_pVary.size(); i++ )
    {
        auto pat = std::bitset< MAX_NUM_STEPS >( m_pVary[ i ].getPatternLong() );
        for ( size_t j = 0; j < nBeats; j++ )
        {
            if ( trueIfLeftFalseIfRight )
            {
                auto rotatedLeft = ( j + nBeats - 1 ) % nBeats;
                m_pVary[ i ].setBeat( rotatedLeft, pat[ j ] );
            }
            else
            {
                auto rotatedRight = ( j + 1 ) % nBeats;
                m_pVary[ i ].setBeat( rotatedRight, pat[ j ] );
            }
        }
    }
}


// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Sjf_AAIM_DrumsAudioProcessor();
}
