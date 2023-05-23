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
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    DBG("AAIM Drums");
    m_rGen.setIOIProbability( 0.5, 1 );
    m_rGen.setComplexity( 0.8 );
    m_rGen.setRests( 0.2 );
    DBG( m_rGen.getRests() << " " << m_rGen.getComplexity() );
    
    for ( size_t i = 0; i < m_pVary.size(); i++ )
    {
        std::string pat;
        for ( size_t j = 0; j < m_pVary[ i ].getNumBeats(); j++ )
        {
            auto trig = rand01() > 0.75 ? true : false;
            m_pVary[ i ].setBeat( j, trig );
            if ( trig ) { pat += "1"; }
            else { pat += "0"; }
        }
        DBG( "m_pVary" << i << " pat " << pat );
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
    juce::ScopedNoDenormals noDenormals;
    auto bufferSize = buffer.getNumSamples();
    // if there is an available playhead
    auto beatDivFactor = std::pow( 2, static_cast< double >(m_TSdenominator) ); // multiple for converting from quarterNotes to other beat types
    
    midiMessages.clear();
    playHead = this->getPlayHead();
    if (playHead != nullptr)
    {
        positionInfo = *playHead->getPosition();
        if( positionInfo.getPpqPosition() )
        {
            if ( positionInfo.getBpm() )
            {
//                auto ppq = *positionInfo.getPpqPosition();
                auto timeInSamps = static_cast<double>(*positionInfo.getTimeInSamples());
//                DBG("timeInSamps " << timeInSamps );
//                DBG("next Time " << (timeInSamps + bufferSize) );
//                auto ppb = ppq * beatDivFactor;
                auto nBeats = static_cast< double >(m_rGen.getNumBeats());
//                auto pos = fastMod4(ppb, nBeats);
                auto bpm = *positionInfo.getBpm();
                auto sr = getSampleRate();
//                auto sampsPerBeat = ( sr * static_cast< double >(60) ) / bpm;
//                DBG("sampsPerBeat " << sampsPerBeat);
                auto pos = ( (bpm * beatDivFactor * timeInSamps) / ( sr * static_cast< double >(60) ) );
//                DBG("pos " << pos);
                auto increment = (static_cast<double>(bpm) * beatDivFactor )/ ( sr * static_cast< double >(60) );
                for ( int i = 0; i < bufferSize; i++ )
                {
                    auto currentBeat = fastMod4< double >( (pos + ( i * increment )), nBeats );
                    auto genOut = m_rGen.runGenerator( currentBeat );
                    if ( genOut[ 0 ] < m_lastRGenPhase*0.5 )
                    {
//                        DBG( "!!!!!! rGen Trigger: CurrentBeat " << currentBeat << " beatsToSync " << genOut[ 4 ] );
                        for ( size_t j = 0; j < m_pVary.size(); j++ )
                        {
                            if ( m_pVary[ j ].triggerBeat( currentBeat, genOut[ 4 ] ) && genOut[ 2 ] > 0 )
                            {
                                auto note = juce::MidiMessage::noteOn( 1, static_cast<int>(j)+36, genOut[ 1 ] );
                                midiMessages.addEvent( note, i );
//                                DBG( "NOTE OUT " << j << " "<< genOut[ 1 ] << " " << genOut[ 2 ] );
                            }
                        }
                    }
                    m_lastRGenPhase = genOut[ 0 ];
                }
            }
        }
//        DBG("");
//        if ( positionInfo.getTimeSignature() )
//        {
//            auto TS = *positionInfo.getTimeSignature();
//            DBG( "Time Signature " << TS.numerator << " / " << TS.denominator );
//        }
    }
    
}

//==============================================================================
bool Sjf_AAIM_DrumsAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* Sjf_AAIM_DrumsAudioProcessor::createEditor()
{
    return new Sjf_AAIM_DrumsAudioProcessorEditor (*this);
}

//==============================================================================
void Sjf_AAIM_DrumsAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void Sjf_AAIM_DrumsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Sjf_AAIM_DrumsAudioProcessor();
}
