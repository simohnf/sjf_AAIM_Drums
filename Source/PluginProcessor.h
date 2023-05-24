/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../sjf_AAIM_Cplusplus/sjf_AAIM_rhythmGen.h"
#include "../sjf_AAIM_Cplusplus/sjf_AAIM_patternVary.h"
#include "../sjf_AAIM_Cplusplus/sjf_audio/sjf_audioUtilitiesC++.h"

#define NUM_VOICES 16
#define MAX_NUM_STEPS 32
#define NUM_IOIs 26
//==============================================================================
/**
*/
class Sjf_AAIM_DrumsAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    Sjf_AAIM_DrumsAudioProcessor();
    ~Sjf_AAIM_DrumsAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    
    void setPattern( int row, std::vector<bool> pattern );
    const std::vector<bool>& getPattern( int row );
    
    size_t getNumVoices(){ return m_pVary.size(); };
    
    void setIOIProbability( float division, float chanceForThatDivision)
    { m_rGen.setIOIProbability( division, chanceForThatDivision); };
    
    std::vector< std::array< float , 4> > getIOIProbability( )
    { return m_rGen.getIOIProbabilities(); };
    
    bool stateLoaded(){ return m_stateLoadedFlag; }
    void setStateLoadedFalse( ){ m_stateLoadedFlag = false; }
    
    static constexpr std::array< float, NUM_IOIs > ioiFactors
    {
        4., 3.2, 3., 2.666667, 2.285714, 2., 1.6, 1.5, 1.333333, 1.142857, 1., 0.8, 0.75, 0.666667, 0.571429, 0.5, 0.4, 0.375, 0.333333, 0.285714, 0.25, 0.2, 0.1875, 0.166667, 0.142857, 0.125
    };
    
    
    void setNonAutomatableParameterValues();
    
private:
    
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    void setParameters();
    
    
    
    static BusesProperties getBusesLayout()
    {
        // Live doesn't like to load midi-only plugins, so we add an audio output there.
        return juce::PluginHostType().isAbletonLive() ? BusesProperties().withOutput ("out", juce::AudioChannelSet::stereo())
                                                : BusesProperties();
    }
    
    juce::AudioProcessorValueTreeState parameters;
    
    std::vector< std::array < float, 2 > > genIOIlist()
    {
        auto ioiList = std::vector< std::array < float, 2 > >();
        for ( size_t i = 0; i < NUM_IOIs; i++ )
        {
            auto prob = ioiFactors[ i ] == 1 ? 1.0f : 0.0f;
            ioiList.push_back( std::array < float, 2 >{ ioiFactors[ i ], prob } );
        }
        return ioiList;
    }
    
    
    AAIM_rhythmGen< float > m_rGen;
    

    std::array< AAIM_patternVary< float >, NUM_VOICES > m_pVary;
    
    juce::AudioPlayHead* playHead;
    juce::AudioPlayHead::PositionInfo positionInfo;
    
    enum beatDivisions
    {
        halfNote = 1, quarterNote, eightNote, sixteenthNote, thirtySecondNote, sixtyFourthNote
    };
    
    int m_tsDenominator = eightNote, m_midiChannel = 1;
    double m_lastRGenPhase = 1;
    

    
    std::atomic<float>* tsDenominatorParameter = nullptr;
    std::atomic<float>* midiChannelParameter = nullptr;
    std::atomic<float>* complexityParameter = nullptr;
    std::atomic<float>* restsParameter = nullptr;
    std::atomic<float>* fillsParameter = nullptr;
    std::atomic<float>* nBeatsParameter = nullptr;
    
//    juce::Value ioiProbsParameter;
    std::array< juce::Value, NUM_IOIs > ioiDivParameters, ioiProbParameters;
    std::array< juce::Value, NUM_VOICES > patternParameters;
    bool m_stateLoadedFlag = false;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Sjf_AAIM_DrumsAudioProcessor)
};
