#ifndef Open303VST_h
#define Open303VST_h

#include "public.sdk/source/vst2.x/audioeffectx.h"
#include "../DSPCode/rosic_Open303.h"
using namespace rosic;

//#define SHOW_INTERNAL_PARAMETERS // comment this for builds that should only show 'user'-parameters

enum Open303Parameters
{
  WAVEFORM = 0,
  TUNING,
  CUTOFF,
  RESONANCE,
  ENVMOD,
  DECAY,
  ACCENT,
  VOLUME,

  FILTER_TYPE,

#ifdef SHOW_INTERNAL_PARAMETERS
  AMP_SUSTAIN,
  TANH_SHAPER_DRIVE,
  TANH_SHAPER_OFFSET,
  PRE_FILTER_HPF,
  FEEDBACK_HPF,
  POST_FILTER_HPF,
  SQUARE_PHASE_SHIFT,
#endif

  OPEN303_NUM_PARAMETERS
};

//=================================================================================================
// class Open303VSTProgram:

class Open303VSTProgram
{
  friend class Open303VST;
public:
  Open303VSTProgram();
  ~Open303VSTProgram() {}
private:
  float parameters[OPEN303_NUM_PARAMETERS];
  char  name[kVstMaxProgNameLen+1];
};

//=================================================================================================
// class Open303VST:

class Open303VST : public AudioEffectX
{
public:

  // construction/destruction:
  Open303VST (audioMasterCallback audioMaster);
  ~Open303VST();

  // audio- and event processing:
  virtual void     processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames);
  virtual VstInt32 processEvents   (VstEvents* events);
  virtual void     handleEvent     (VstMidiEvent midiEvent);

  // program handling:
  virtual void     setProgram           (VstInt32 program);
  virtual void     setProgramName       (char* name);
  virtual void     getProgramName       (char* name);
  virtual bool     getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text);

  // parameter handling:
  virtual void     setParameter       (VstInt32 index, float value);
  virtual float    getParameter       (VstInt32 index);
  virtual void     getParameterLabel  (VstInt32 index, char* label);
  virtual void     getParameterDisplay(VstInt32 index, char* text);
  virtual void     getParameterName   (VstInt32 index, char* text);

  // audio processing parameters:
  virtual void     setSampleRate(float    sampleRate);
  virtual void     setBlockSize (VstInt32 blockSize);

  // callbacks to pass plugin info to the host:
  virtual bool     getOutputProperties(VstInt32 index, VstPinProperties* properties);
  virtual bool     getEffectName   (char* name);
  virtual bool     getVendorString (char* text);
  virtual bool     getProductString(char* text);
  virtual VstInt32 getVendorVersion();
  virtual VstInt32 canDo           (char* text);
  virtual VstInt32 getNumMidiInputChannels () { return 1; }
  virtual VstInt32 getNumMidiOutputChannels() { return 0; }

private:

  // internal functions:
  void noteOn(VstInt32 note, VstInt32 velocity, VstInt32 delta);

  /** Converts the data bytes of a MIDI pitchwheel message into a value in semitones. */
  double convertToPitch(unsigned char highByte,unsigned char lowByte);

  static const int numOutputs  = 2;
  static const int numPrograms = 128;

  // program handling:
  Open303VSTProgram* programs;
  VstInt32           channelPrograms[16];

  // MIDI event handling:
	int      eventBufferLength;
	VstEvent *eventBuffer;

  // the embedded core dsp object:
  Open303 open303Core;

};

#endif
