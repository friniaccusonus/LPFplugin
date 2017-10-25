/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DspFilters/Dsp.h"


//==============================================================================
LpfilterAudioProcessor::LpfilterAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       // Sets Input and Output bus to be stereo by default
                       .withInput  ("Input",  AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", AudioChannelSet::stereo(), true)
                     #endif
                       ),
#endif
        // Setup lpfJuce
        lpfJuce(dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(44100.0, defaultFreq))

{
    //Setup filter with DSPFilters lib
    lpfDspLib = new Dsp::FilterDesign<Dsp::Butterworth::Design::LowPass <1>, 2>;
    
    
    // Add parameters
    addParameter(gain = new AudioParameterFloat("gain", "Gain", 0.0f, 1.0f, 0.5f));
    addParameter(frequency = new AudioParameterFloat("frequency", "Hz", defaultFreq, 10000.f, defaultFreq));
}

LpfilterAudioProcessor::~LpfilterAudioProcessor()
{
}

//==============================================================================
const String LpfilterAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool LpfilterAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool LpfilterAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool LpfilterAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double LpfilterAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int LpfilterAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int LpfilterAudioProcessor::getCurrentProgram()
{
    return 0;
}

void LpfilterAudioProcessor::setCurrentProgram (int index)
{
}

const String LpfilterAudioProcessor::getProgramName (int index)
{
    return {};
}

void LpfilterAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void LpfilterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Get the number of channels
    auto channels = static_cast<uint32> (getMainBusNumInputChannels());
    
    // Prepare lpfJuce filter
    dsp::ProcessSpec spec {sampleRate, static_cast<uint32>(samplesPerBlock), channels};
    lpfJuce.prepare(spec);
    
    // Prepare lpfDspLib filter
    paramsDsp[0] = sampleRate; paramsDsp[1] = 1; paramsDsp[2] = defaultFreq;
    lpfDspLib->setParams(paramsDsp);
    
    // Set up custom LPF coefficients
    iirCoef = IIRCoefficients::makeLowPass(sampleRate, *frequency);
    
    if (getTotalNumInputChannels() < 2 )
    {
        tempBuffer.setSize(2, samplesPerBlock);
    }
    
    // Set up previous buffer for custom filter
    prevBuffer.setSize(getTotalNumInputChannels(), samplesPerBlock);
    prevBuffer.clear();
}

void LpfilterAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool LpfilterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const AudioChannelSet& mainInput = layouts.getMainInputChannelSet();
    const AudioChannelSet& mainOutput = layouts.getMainOutputChannelSet();
    
    // This checks if the input layout matches the output layout
    if ( mainInput != mainOutput )
        return false;
    
    // Check for only mono or stereo layout
    if ( mainInput.size() > 2 )
        return false;
    
    return true;
    
}
#endif

void LpfilterAudioProcessor::processBlock (AudioSampleBuffer& ioBuffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;
    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        ioBuffer.clear (i, 0, ioBuffer.getNumSamples());
    
    
    //Update frequency parameter
    updateParameters();
    
    // The filtering process happens here
    process (ioBuffer);
    
    // Apply gain
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        ioBuffer.applyGain (*gain);
    }
}

void LpfilterAudioProcessor::process (AudioSampleBuffer& processBuffer) noexcept
{
    // Define the block that passes into process function
    dsp::AudioBlock<float> block (processBuffer);
    
    // lpfJuce filter processing
   // lpfJuce.process(dsp::ProcessContextReplacing<float> (block));
    
    // lpfDspLib filter processing
    /*
    if (getTotalNumInputChannels() < 2 )
    {
        tempBuffer.copyFrom(0, *tempBuffer.getReadPointer(0), processBuffer.getReadPointer(0), processBuffer.getNumSamples());
        tempBuffer.copyFrom(1, *tempBuffer.getReadPointer(1), processBuffer.getReadPointer(0), processBuffer.getNumSamples());
    
        lpfDspLib->process(tempBuffer.getNumSamples(), tempBuffer.getArrayOfWritePointers());
        
        processBuffer.copyFrom(0, *processBuffer.getWritePointer(0), tempBuffer.getWritePointer(0), tempBuffer.getNumSamples());
        
    }
    else
    {
        lpfDspLib->process(processBuffer.getNumSamples(), processBuffer.getArrayOfWritePointers());
    }
     */
    
    // Custom filter processing
    for (int ch = 0; ch < getTotalNumInputChannels(); ++ch)
    {
        float* const writePtr = processBuffer.getWritePointer(ch);
        float* const prevWritePtr = prevBuffer.getWritePointer(ch);
        const float* prevReadPtr = prevBuffer.getReadPointer(ch);
        const float* readPtr = processBuffer.getReadPointer(ch);
        
        writePtr[0] = -iirCoef.coefficients[3] * prevWritePtr[511] -
                            iirCoef.coefficients[4] * prevWritePtr[510] +
                            iirCoef.coefficients[0] * readPtr[0] +
                            iirCoef.coefficients[1] * prevReadPtr[511] +
                            iirCoef.coefficients[2] * prevReadPtr[510] ;
        
        
        writePtr[1] = -iirCoef.coefficients[3] * writePtr[0] -
                            iirCoef.coefficients[4] * prevWritePtr[511] +
                            iirCoef.coefficients[0] * readPtr[1] +
                            iirCoef.coefficients[1] * readPtr[0] +
                            iirCoef.coefficients[2] * prevReadPtr[511];
        for (int sample = 2; sample < processBuffer.getNumSamples(); ++sample)
        {
            writePtr[sample]  = -iirCoef.coefficients[3] * writePtr[sample-1] -
                                iirCoef.coefficients[4] * writePtr[sample-2] +
                                iirCoef.coefficients[0] * readPtr[sample] +
                                iirCoef.coefficients[1] * readPtr[sample-1] +
                                iirCoef.coefficients[2] * readPtr[sample-2] ;
        }
    }
    
    prevBuffer = processBuffer;
}

void LpfilterAudioProcessor::updateParameters()
{
    // Update lpfJuce
    *lpfJuce.state = *dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(getSampleRate(), *frequency);
    
    // Update lpfDspLib
    paramsDsp[2] = frequency->get();
    lpfDspLib->setParams(paramsDsp);
    
    // Update custom filter coeffiecients
     iirCoef = IIRCoefficients::makeLowPass(getSampleRate(), *frequency);
}

//==============================================================================
bool LpfilterAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* LpfilterAudioProcessor::createEditor()
{
    return new LpfilterAudioProcessorEditor (*this);
}

//==============================================================================
void LpfilterAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void LpfilterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LpfilterAudioProcessor();
}
