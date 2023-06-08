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
    
    midiChannelParameter = parameters.getRawParameterValue( "midiChannel");
    complexityParameter = parameters.getRawParameterValue("complexity");
    restsParameter = parameters.getRawParameterValue("rests");
    fillsParameter = parameters.getRawParameterValue("fills");
    swingParameter = parameters.getRawParameterValue("swing");
    bankNumberParameter = parameters.getRawParameterValue("patternBank");
    internalResetParameter = parameters.getRawParameterValue("internalReset");
    
    for ( size_t i = 0; i < NUM_IOIs; i++ )
    {
        ioiDivParameters[ i ] = parameters.state.getPropertyAsValue( "ioiDiv" + juce::String(i), nullptr, true );
        ioiProbParameters[ i ] = parameters.state.getPropertyAsValue( "ioiProb" + juce::String(i), nullptr, true );
    }
    
    for ( size_t i = 0; i < NUM_BANKS; i++ )
    {
        for ( size_t j = 0; j < NUM_VOICES; j++ )
        {
            patternBanksParameters[ i ][ j ] = parameters.state.getPropertyAsValue( "patternBank" + juce::String( i ) + "Voice" + juce::String( j ), nullptr, true );
        }
        patternBanksNumBeatsParameters[ i ] = parameters.state.getPropertyAsValue( "patternBankNumBeats" + juce::String( i ), nullptr, true );
        divBanksParameters[ i ] = parameters.state.getPropertyAsValue( "divisionBank" + juce::String( i ), nullptr, true );
    }
    
    auto nBeats = m_rGen.getNumBeats();
    for ( size_t i = 0; i < NUM_BANKS; i++ )
    {
        m_nBeatsBanks[ i ] = nBeats;
        m_divBanks[ i ] = eightNote;
        for ( size_t j = 0; j < NUM_VOICES; j++ )
            m_patternBanks[ i ][ j ].reset();
    }

    selectPatternBank();
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
    selectPatternBank();
    setParameters();
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
    //    selectPatternBank();
        setParameters();
    
    auto swing = static_cast< float > ( *swingParameter );
    swing = swing >= 0 ? 1.0f + ( swing * swing ) : 1.0f - ( 0.5f * swing * swing );
    auto swingOnFlag = swing == 1 ? false : true;
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
            auto indx = static_cast< double >( static_cast<int>(m_divBanks[ *bankNumberParameter ]) - 2 );
            auto beatDivFactor = std::pow( 2.0f, indx ); // multiple for converting from quarterNotes to other beat types
            //            auto nBeats = static_cast< double >(m_rGen.getNumBeats());
            auto timeInSamps = static_cast< double >(*positionInfo.getTimeInSamples());
            auto bpm = *positionInfo.getBpm();
            auto sr = getSampleRate();
            // calculate the increment per sample
            auto increment = (static_cast<double>(bpm) * beatDivFactor )/ ( sr * static_cast< double >(60) );
            // convert sample position to position on timeline in the underlying rhythmic division of the drumMachine
            auto hostPos = ( (bpm * beatDivFactor * timeInSamps) / ( sr * static_cast< double >(60) ) );
            auto pos = hostPos;
            auto currentBeat = pos;
            for ( int i = 0; i < bufferSize; i++ )
            {
                currentBeat = pos + ( i * increment );
                currentBeat = swingOnFlag ? applySwingToPosition( currentBeat, swing ) : currentBeat ;
                if ( static_cast<int>( currentBeat ) != m_currentStep )
                {
                    if ( selectPatternBank() )
                        m_internalSyncCompensation = static_cast< bool >( *internalResetParameter ) ? currentBeat : 0;
//                    setParameters();
                    currentBeat = fastMod4< double >( currentBeat - m_internalSyncCompensation, m_nBeatsBanks[ *bankNumberParameter ] );
                    m_currentStep = static_cast< int >( currentBeat );
                }
                else
                {
                    currentBeat = fastMod4< double >( currentBeat - m_internalSyncCompensation, m_nBeatsBanks[ *bankNumberParameter ] );
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
        }
        else
        {
            selectPatternBank();
//            setParameters();
        }
    }
    else
    {
        selectPatternBank();
//        setParameters();
    }
}

//==============================================================================
double Sjf_AAIM_DrumsAudioProcessor::calculateCurrentBeat( double currentBeat, double hostPosition, double increment, size_t sampleIndex, bool swingOnFlag, float swing )
{
    currentBeat = swingOnFlag ? applySwingToPosition( hostPosition + ( sampleIndex * increment ), swing ) : hostPosition + ( sampleIndex * increment ) ;
    currentBeat = fastMod4< double >( currentBeat, m_nBeatsBanks[ *bankNumberParameter ] );
    if ( static_cast<int>( currentBeat ) != m_currentStep )
    {
        selectPatternBank();
        m_currentStep = static_cast< int >( currentBeat );
    }
    return currentBeat;
}

double Sjf_AAIM_DrumsAudioProcessor::applySwingToPosition( double currentBeat, float swing )
{
    auto halfPos = currentBeat * 0.5;
    auto modPos = halfPos - static_cast< int >( halfPos );
    modPos = std::pow( modPos, swing );
    return (static_cast<float>(static_cast< int >( halfPos )) + modPos) * 2.0f;
}


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
            {
                auto pat = m_patternBanks[ i ][ j ].to_ullong();
                auto patDouble = static_cast<double>( pat );
                patternBanksParameters[ i ][ j ].setValue( patDouble );
            }
            patternBanksNumBeatsParameters[ i ].setValue( static_cast<double>( m_nBeatsBanks[ i ] ) );
            divBanksParameters[ i ].setValue( static_cast< double >( m_divBanks[ i ] ) );
        }
    }
    updateHostDisplay();
}

void Sjf_AAIM_DrumsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    m_lastLoadedBank = -1;
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName (parameters.state.getType()))
        {
            parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
            for ( size_t i = 0; i < NUM_BANKS; i++ )
            {
                patternBanksNumBeatsParameters[ i ].referTo( parameters.state.getPropertyAsValue( "patternBankNumBeats" + juce::String( i ), nullptr, true ) );
                m_nBeatsBanks[ i ] = static_cast< long long >( patternBanksNumBeatsParameters[ i ].getValue() );
                
                divBanksParameters[ i ].referTo( parameters.state.getPropertyAsValue( "divisionBank" + juce::String( i ), nullptr, true ) );
                m_divBanks[ i ] = static_cast< long long >( divBanksParameters[ i ].getValue() );
                
                for ( size_t j = 0; j < NUM_VOICES; j++ )
                {
                    patternBanksParameters[ i ][ j ].referTo( parameters.state.getPropertyAsValue( "patternBank" + juce::String( i ) + "Voice" + juce::String( j ), nullptr, true ) );
                    auto val = static_cast<double>(patternBanksParameters[ i ][ j ].getValue());
                    m_patternBanks[ i ][ j ] = val;
                }
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
    }
    selectPatternBank();
    setParameters();
    m_stateLoadedFlag = true;
}
//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout Sjf_AAIM_DrumsAudioProcessor::createParameterLayout()
{
    static constexpr int pIDVersionNumber = 1;
    juce::AudioProcessorValueTreeState::ParameterLayout params;
    params.add( std::make_unique<juce::AudioParameterFloat>( juce::ParameterID{ "complexity", pIDVersionNumber }, "Complexity", 0, 1, 0 ) );
    params.add( std::make_unique<juce::AudioParameterFloat>( juce::ParameterID{ "rests", pIDVersionNumber }, "Rests", 0, 1, 0 ) );
    params.add( std::make_unique<juce::AudioParameterFloat>( juce::ParameterID{ "fills", pIDVersionNumber }, "Fills", 0, 1, 0 ) );
    params.add( std::make_unique<juce::AudioParameterFloat>( juce::ParameterID{ "swing", pIDVersionNumber }, "Swing", -1, 1, 0 ) );
    params.add( std::make_unique<juce::AudioParameterInt>( juce::ParameterID{ "midiChannel", pIDVersionNumber }, "MidiChannel", 1, 16, 1 ) );
    params.add( std::make_unique<juce::AudioParameterInt>( juce::ParameterID{ "patternBank", pIDVersionNumber }, "PatternBank", 0, 15, 0 ) );
    params.add( std::make_unique<juce::AudioParameterBool>( juce::ParameterID{ "internalReset", pIDVersionNumber }, "InternalReset", true ) );
    return params;
}

void Sjf_AAIM_DrumsAudioProcessor::setPattern( int row, std::vector<bool> pattern )
{
    auto nBeats = pattern.size() < m_nBeatsBanks[ *bankNumberParameter ] ? pattern.size() : m_nBeatsBanks[ *bankNumberParameter ] ;
    for ( size_t i = 0; i < nBeats; i++ )
    {
        m_patternBanks[ *bankNumberParameter ][ row ][ i ] = pattern[ i ];
        m_pVary[ row ].setBeat( i, pattern[ i ] );
    }
}

const std::vector<bool>& Sjf_AAIM_DrumsAudioProcessor::getPattern( int row )
{
    return m_pVary[ row ].getPattern();
}
//==============================================================================
void Sjf_AAIM_DrumsAudioProcessor::setParameters()
{
//    selectPatternBank();
    m_rGen.setComplexity( *complexityParameter );
    m_rGen.setRests( *restsParameter );
    for (size_t i = 0; i < m_pVary.size(); i++ )
        m_pVary[ i ].setFills( *fillsParameter );
    m_midiChannel = *midiChannelParameter;
}

bool Sjf_AAIM_DrumsAudioProcessor::selectPatternBank()
{
    if ( m_lastLoadedBank == *bankNumberParameter )
        return false;
    m_rGen.setNumBeats( m_nBeatsBanks[ *bankNumberParameter ] );
    for ( size_t i = 0; i < NUM_VOICES; i++ )
    {
        m_pVary[ i ].setNumBeats( m_nBeatsBanks[ *bankNumberParameter ] );
        for ( size_t j = 0; j < m_nBeatsBanks[ *bankNumberParameter ]; j++ )
            m_pVary[ i ].setBeat( j, m_patternBanks[ *bankNumberParameter ][ i ][ j ] );
    }
    m_lastLoadedBank = *bankNumberParameter;
    m_stateLoadedFlag = true;
    return true;
}

void Sjf_AAIM_DrumsAudioProcessor::copyPatternBankContents( size_t bankToCopyFrom, size_t bankToCopyTo )
{
    for ( size_t i = 0; i < NUM_VOICES; i++ )
        m_patternBanks[ bankToCopyTo ][ i ] = m_patternBanks[ bankToCopyFrom ][ i ];
    m_nBeatsBanks[ bankToCopyTo ] = m_nBeatsBanks[ bankToCopyFrom ];
    m_divBanks[ bankToCopyTo ] = m_divBanks[ bankToCopyFrom ];
}
//==============================================================================
//      ALGORITHMIC VARIATIONS
void Sjf_AAIM_DrumsAudioProcessor::reversePattern()
{
    for ( size_t i = 0; i < m_pVary.size(); i++ )
    {
        auto pat = std::bitset< MAX_NUM_STEPS >( m_pVary[ i ].getPatternLong() );
        
        for ( size_t j = 0; j < m_nBeatsBanks[ *bankNumberParameter ]; j++ )
        {
            auto revStep = m_nBeatsBanks[ *bankNumberParameter ] - j - 1;
            m_pVary[ i ].setBeat( revStep, pat[ j ] );
            m_patternBanks[ *bankNumberParameter ][ i ][ revStep ] = pat[ j ];
        }
    }
}


void Sjf_AAIM_DrumsAudioProcessor::markovHorizontal()
{
    // create a transition table for each voice
    // then pass each into markov chain
    auto nVoices = m_pVary.size();
    for ( size_t i = 0; i < nVoices; i++ )
    {
        auto transitionTable = std::array< std::array < int, 2 >, 2 >{ { { 0, 0 }, { 0, 0 } } };
        auto pat = std::bitset< MAX_NUM_STEPS >( m_pVary[ i ].getPatternLong() );
        for ( size_t j = 0; j < m_nBeatsBanks[ *bankNumberParameter ]; j++ )
        {
            auto bit = pat[ j ] ? 1 : 0;
            auto nextStep = ( j + 1 ) % m_nBeatsBanks[ *bankNumberParameter ];
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
        
        for ( size_t j = 0; j < m_nBeatsBanks[ *bankNumberParameter ]; j++ )
        {
            m_patternBanks[ *bankNumberParameter ][ i ][ j ] = trig;
            m_pVary[ i ].setBeat( j, trig );
            rnd = rand01() * ( transitionTable[ trig ][ 0 ] + transitionTable[ trig ][ 1 ]);
            trig = ( rnd < transitionTable[ trig ][ 0 ] ) ? false : true;
        }
        
    }
}

void Sjf_AAIM_DrumsAudioProcessor::cellShuffleVariation()
{
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
            {
                auto trig = cells[ i ][ j ][ k ];
                m_patternBanks[ *bankNumberParameter ][ k ][ count ] = trig;
                m_pVary[ k ].setBeat( count, trig );
            }
            count += 1;
        }
}

void Sjf_AAIM_DrumsAudioProcessor::palindromeVariation()
{
    auto nBeats = m_nBeatsBanks[ *bankNumberParameter ] * 2;
    nBeats = ( nBeats > MAX_NUM_STEPS ) ? MAX_NUM_STEPS : nBeats;
    m_rGen.setNumBeats( nBeats );
    m_nBeatsBanks[ *bankNumberParameter ] = static_cast<int>(nBeats);
    for ( size_t i = 0; i < m_pVary.size(); i++ )
    {
        m_pVary[ i ].setNumBeats( m_nBeatsBanks[ *bankNumberParameter ] );
        for ( size_t j = 0; j < m_nBeatsBanks[ *bankNumberParameter ]/2; j++ )
        {
            auto step = m_nBeatsBanks[ *bankNumberParameter ] - 1 - j;
            auto trig = m_pVary[ i ].getStep( j );
            m_patternBanks[ *bankNumberParameter ][ i ][ step ] = trig;
            m_pVary[ i ].setBeat( step, trig );
        }
    }
}


void Sjf_AAIM_DrumsAudioProcessor::doublePattern()
{
    auto nBeats = m_nBeatsBanks[ *bankNumberParameter ] * 2;
    nBeats = ( nBeats > MAX_NUM_STEPS ) ? MAX_NUM_STEPS : nBeats;
    m_nBeatsBanks[ *bankNumberParameter ] = static_cast<int>(nBeats);
    for ( size_t i = 0; i < m_pVary.size(); i++ )
    {
        m_pVary[ i ].setNumBeats( nBeats );
        for ( size_t j = 0; j < nBeats/2; j++ )
        {
            auto step = nBeats/2 + j;
            auto trig = m_pVary[ i ].getStep( j );
            m_patternBanks[ *bankNumberParameter ][ i ][ step ] = trig;
            m_pVary[ i ].setBeat( step, trig );
        }
    }
}


void Sjf_AAIM_DrumsAudioProcessor::rotatePattern( bool trueIfLeftFalseIfRight)
{
    for ( size_t i = 0; i < m_pVary.size(); i++ )
    {
        auto pat = std::bitset< MAX_NUM_STEPS >( m_pVary[ i ].getPatternLong() );
        for ( size_t j = 0; j < m_nBeatsBanks[ *bankNumberParameter ]; j++ )
        {
            if ( trueIfLeftFalseIfRight )
            {
                auto rotatedLeft = ( j + m_nBeatsBanks[ *bankNumberParameter ] - 1 ) % m_nBeatsBanks[ *bankNumberParameter ];
                auto trig = pat[ j ];
                m_patternBanks[ *bankNumberParameter ][ i ][ rotatedLeft ] = trig;
                m_pVary[ i ].setBeat( rotatedLeft, trig );
            }
            else
            {
                auto rotatedRight = ( j + 1 ) % m_nBeatsBanks[ *bankNumberParameter ];
                auto trig = pat[ j ];
                m_patternBanks[ *bankNumberParameter ][ i ][ rotatedRight ] = trig;
                m_pVary[ i ].setBeat( rotatedRight, trig );
            }
        }
    }
}


// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Sjf_AAIM_DrumsAudioProcessor();
}
