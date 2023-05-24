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
    DBG("AAIM Drums");
    
//    m_rGen.setIOIList( genIOIlist() );
    

    tsDenominatorParameter = parameters.getRawParameterValue("rhythmicDivision");
    midiChannelParameter = parameters.getRawParameterValue( "midiChannel");
    complexityParameter = parameters.getRawParameterValue("complexity");
    restsParameter = parameters.getRawParameterValue("rests");
    fillsParameter = parameters.getRawParameterValue("fills");
    nBeatsParameter = parameters.getRawParameterValue("nBeats");
    
    for( size_t i = 0; i < patternParameters.size(); i++ )
        patternParameters[ i ] = parameters.state.getPropertyAsValue( "pattern"+juce::String(i), nullptr, true );
//    ioiProbsParameter = parameters.state.getPropertyAsValue("ioiProbs", nullptr, true );
    for ( size_t i = 0; i < NUM_IOIs; i++ )
    {
        ioiDivParameters[ i ] = parameters.state.getPropertyAsValue( "ioiDiv"+juce::String(i), nullptr, true );
        ioiProbParameters[ i ] = parameters.state.getPropertyAsValue( "ioiProb"+juce::String(i), nullptr, true );
    }
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
            for ( int i = 0; i < bufferSize; i++ )
            {
                auto currentBeat = fastMod4< double >( ( pos + ( i * increment )), nBeats );
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
    }
}

void Sjf_AAIM_DrumsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName (parameters.state.getType()))
        {
            parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
            setParameters();
            for ( size_t i = 0; i < NUM_VOICES; i++ )
            {
                patternParameters[ i ].referTo( parameters.state.getPropertyAsValue ( "pattern"+juce::String(i), nullptr, true ) );
                juce::String str ( patternParameters[ i ].getValue() );
                auto step = 0ul;
                while ( str.length() > 1 )
                {
                    auto substr = str.substring(0, 1);
                    auto trig =  substr.getFloatValue() > 0 ? true : false;
                    m_pVary[ i ].setBeat( step, trig );
                    step += 1;
                    str = str.substring( 1, str.length() );
                }
            }

            
            for ( size_t i = 0; i < NUM_IOIs; i++ )
            {
                ioiDivParameters[ i ].referTo( parameters.state.getPropertyAsValue( "ioiDiv"+juce::String(i), nullptr, true ) );
                ioiProbParameters[ i ].referTo( parameters.state.getPropertyAsValue( "ioiProb"+juce::String(i), nullptr, true ) );
                auto div = ioiDivParameters[ i ].getValue();
                auto prob = ioiProbParameters[ i ].getValue();
                m_rGen.setIOIProbability( div, prob );
                //            ioiDivParameters[ i ] = parameters.state.getPropertyAsValue( "ioiDiv"+juce::String(i), nullptr, true );
                //            ioiProbParameters[ i ] = parameters.state.getPropertyAsValue( "ioiProb"+juce::String(i), nullptr, true );
            }
        }
    }
    
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
    params.add( std::make_unique<juce::AudioParameterInt>( juce::ParameterID{ "rhythmicDivision", pIDVersionNumber }, "RhythmicDivision", halfNote, sixtyFourthNote, eightNote ) );
    params.add( std::make_unique<juce::AudioParameterInt>( juce::ParameterID{ "midiChannel", pIDVersionNumber }, "MidiChannel", 1, 16, 1 ) );
    params.add( std::make_unique<juce::AudioParameterInt>( juce::ParameterID{ "nBeats", pIDVersionNumber }, "NBeats", 1, MAX_NUM_STEPS, 8 ) );
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
    m_rGen.setComplexity( *complexityParameter );
    m_rGen.setRests( *restsParameter );
    m_rGen.setNumBeats( *nBeatsParameter );
    for (size_t i = 0; i < m_pVary.size(); i++ )
    {
        m_pVary[ i ].setNumBeats( m_rGen.getNumBeats() );
        m_pVary[ i ].setFills( *fillsParameter );
    }
    m_tsDenominator = *tsDenominatorParameter;
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Sjf_AAIM_DrumsAudioProcessor();
}
