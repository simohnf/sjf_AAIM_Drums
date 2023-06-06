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
#include <algorithm>    // std::shuffle
#include <vector>       // std::vector
#include <random>       // std::default_random_engine

#define NUM_VOICES 16
#define MAX_NUM_STEPS 32
#define NUM_IOIs 26
#define NUM_BANKS 16
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
    
    
    void setIOIProbability( float division, float chanceForThatDivision)
    { m_rGen.setIOIProbability( division, chanceForThatDivision); };
    
    std::vector< std::array< float , 4> > getIOIProbability( ) { return m_rGen.getIOIProbabilities(); }
    
    bool stateLoaded(){ return m_stateLoadedFlag; }
    void setStateLoadedFalse( ){ m_stateLoadedFlag = false; }
    
    static constexpr std::array< float, NUM_IOIs > ioiFactors
    {
        4., 3.2, 3., 2.666667, 2.285714, 2., 1.6, 1.5, 1.333333, 1.142857, 1., 0.8, 0.75, 0.666667, 0.571429, 0.5, 0.4, 0.375, 0.333333, 0.285714, 0.25, 0.2, 0.1875, 0.166667, 0.142857, 0.125
    };
    
    
    void setNonAutomatableParameterValues();
    
    int getCurrentStep(){ return m_currentStep; }
    
    void copyPatternBankContents( size_t bankToCopyFrom, size_t bankToCopyTo );
    
    void selectPatternBank();
    
    void reversePattern();
    
    void markovHorizontal();
    
    void cellShuffleVariation();
    
    void palindromeVariation();
    
    void doublePattern();
    
    void rotatePattern( bool trueIfLeftFalseIfRight);
    
    void setNumBeats( int nBeats )
    {
        m_nBeatsBanks[ *bankNumberParameter ] = nBeats;
        m_rGen.setNumBeats( m_nBeatsBanks[ *bankNumberParameter ] );
        for ( size_t i = 0; i < NUM_VOICES; i++ )
            m_pVary[ i ].setNumBeats( m_nBeatsBanks[ *bankNumberParameter ] );
    }
    size_t getNumBeats(){ return m_nBeatsBanks[ *bankNumberParameter ]; }
    
    void setTsDenominator( int tsDenominator ){ m_divBanks[ *bankNumberParameter ] = tsDenominator; }
    int getTsDenominator(){ return static_cast<int>( m_divBanks[ *bankNumberParameter ] ); }
    
private:
    
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    void setParameters();
    
//    void setPatternBankSelection();
    
    static BusesProperties getBusesLayout()
    {
        // Live doesn't like to load midi-only plugins, so we add an audio output there.
        return juce::PluginHostType().isAbletonLive() ? BusesProperties().withOutput ("out", juce::AudioChannelSet::stereo())
                                                : BusesProperties();
    }
    
    juce::AudioProcessorValueTreeState parameters;
    
    AAIM_rhythmGen< float > m_rGen;

    std::array< AAIM_patternVary< float >, NUM_VOICES > m_pVary;
    
    juce::AudioPlayHead* playHead;
    juce::AudioPlayHead::PositionInfo positionInfo;
    
    enum beatDivisions
    {
        halfNote = 1, quarterNote, eightNote, sixteenthNote, thirtySecondNote, sixtyFourthNote
    };
    
    int m_midiChannel = 1, m_currentStep = -1, m_lastLoadedBank = -1;
    double m_lastRGenPhase = 1;
    

    // REWRITE SO ALL CHANGES ARE AUTOMATICALLY IN PATTERN BANK... THIS COULD BE A PAIN FOR UNDO, BUT NOT THE END OF THE WORLD
    std::atomic<float>* midiChannelParameter = nullptr;
    std::atomic<float>* complexityParameter = nullptr;
    std::atomic<float>* restsParameter = nullptr;
    std::atomic<float>* fillsParameter = nullptr;
    std::atomic<float>* bankNumberParameter = nullptr;
    
    
    
    std::array< juce::Value, NUM_IOIs > ioiDivParameters, ioiProbParameters;
//    std::array< juce::Value, NUM_VOICES > patternParameters;
    std::array< std::array< juce::Value, NUM_VOICES >, NUM_BANKS > patternBanksParameters;
    std::array< juce::Value, NUM_BANKS > patternBanksNumBeatsParameters, divBanksParameters;
    std::array< std::array< std::bitset< MAX_NUM_STEPS >, NUM_VOICES >, NUM_BANKS > m_patternBanks;
    std::array< size_t, NUM_BANKS > m_nBeatsBanks, m_divBanks;
    bool m_stateLoadedFlag = false;
//    size_t m_patternBank = 0;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Sjf_AAIM_DrumsAudioProcessor)
};
