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

private:
    
    AAIM_rhythmGen< float > m_rGen;
    std::array< AAIM_patternVary< float >, NUM_VOICES > m_pVary;
    
    juce::AudioPlayHead* playHead;
    juce::AudioPlayHead::PositionInfo positionInfo;
    
    enum beatDivisions
    {
        halfNote = -1, quarterNote = 0, eightNote, sixteenthNote, thirtySecondNote
    };
    
    int m_TSdenominator = eightNote;
    double m_lastRGenPhase = 1;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Sjf_AAIM_DrumsAudioProcessor)
};
