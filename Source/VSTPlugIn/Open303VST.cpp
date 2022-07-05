#include "Open303VST.h"

//-------------------------------------------------------------------------------------------------
AudioEffect* createEffectInstance(audioMasterCallback audioMaster)
{
	return new Open303VST(audioMaster);
}

//===============================2==================================================================
// class Open303VSTProgram:

Open303VSTProgram::Open303VSTProgram ()
{
	// default program values:
  parameters[WAVEFORM]    = (float) linToLin(   0.85,   0.0,     1.0, 0.0,  1.0);
  parameters[TUNING]      = (float) linToLin( 440.0,  400.0,   480.0, 0.0,  1.0);
  parameters[CUTOFF]      = (float) expToLin( 500.0,  314.0,  2394.0, 0.0,  1.0);
  parameters[RESONANCE]   = (float) linToLin(  50.0,    0.0,   100.0, 0.0,  1.0);
  parameters[ENVMOD]      = 0.25f;
  parameters[DECAY]       = (float) expToLin( 400.0,  200.0,  2000.0, 0.0,  1.0);
  parameters[ACCENT]      = 0.5f;
  parameters[VOLUME]      = (float) linToLin(  -6.0,  -60.0,     0.0, 0.0,  1.0);
  parameters[FILTER_TYPE] = (float) indexToNormalizedValue(TeeBeeFilter::LP_18, TeeBeeFilter::NUM_MODES);

#ifdef SHOW_INTERNAL_PARAMETERS
  parameters[AMP_SUSTAIN]        = (float) linToLin( -60.0,  -60.0,     0.0, 0.0,  1.0);
  parameters[TANH_SHAPER_DRIVE]  = (float) linToLin(  36.9,    0.0,    60.0, 0.0,  1.0);
  parameters[TANH_SHAPER_OFFSET] = (float) linToLin(   4.37, -10.0,    10.0, 0.0,  1.0);
  parameters[PRE_FILTER_HPF]     = (float) expToLin(  44.5,   10.0,   500.0, 0.0,  1.0);
  parameters[FEEDBACK_HPF]       = (float) expToLin( 150.0,   10.0,   500.0, 0.0,  1.0);
  parameters[POST_FILTER_HPF]    = (float) expToLin(  24.0,   10.0,   500.0, 0.0,  1.0);
  parameters[SQUARE_PHASE_SHIFT] = (float) linToLin( 189.0,    0.0,   360.0, 0.0,  1.0);
#endif

	vst_strncpy (name, "Init", kVstMaxProgNameLen);
}

//=================================================================================================
// class Open303VST:

//-------------------------------------------------------------------------------------------------
// construction/destruction:

Open303VST::Open303VST(audioMasterCallback audioMaster)
: AudioEffectX(audioMaster, numPrograms, OPEN303_NUM_PARAMETERS)
{
	// initialize programs
	programs = new Open303VSTProgram[numPrograms];
	for(VstInt32 i = 0; i < 16; i++)
		channelPrograms[i] = i;

	if(programs)
		setProgram(0);

	if(audioMaster)
	{
		setNumInputs(0);	     // maybe include input for filter FM by audio input here later (DeviFish does this)...
		setNumOutputs(2);
		canProcessReplacing();
		isSynth();
		setUniqueID('O303');   // warning on GCC
	}

	eventBufferLength = 0;
	eventBuffer       = NULL;
	suspend();

  /*
  // for debugging only
  open303Core.setCutoff(3.138152786059267e+002);
  open303Core.setEnvMod(0.0);
  open303Core.setEnvMod(100.0);
  open303Core.setCutoff(2.394411986817546e+003);
  open303Core.setEnvMod(0.0);
  */
}

Open303VST::~Open303VST ()
{
	if(programs)
		delete[] programs;
}

//-------------------------------------------------------------------------------------------------
// audio- and event processing:

void Open303VST::processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames)
{
  float *out1 = outputs[0]; // output buffer for first channel  (left)
  float *out2 = outputs[1]; // output buffer for second channel (right)

  // if there are no events in this block, we bypass the event handling altogether and can use a
  // much simpler loop:
  if( eventBufferLength <= 0 )
  {
    for(int i=0; i<sampleFrames; i++)
    {
      *out1 = *out2 = (float) open303Core.getSample();
      out1++;
      out2++;
    }
    return;
  }

  // loop for the case when there are events to be considered:
  int eventCounter = 0;
  for(int i=0; i<sampleFrames; i++)
  {
    // check, if at this sample one or more events have occurred:
    while( (eventCounter < eventBufferLength) && (i == eventBuffer[eventCounter].deltaFrames) )
    {
      // yes, an event has occurred now -> process the event, if it's a MIDI message:
      if( eventBuffer[eventCounter].type == kVstMidiType )
      {
        // cast VstEvent to VstMidiEvent with a pointer trick:
        VstEvent     *pVstEvent  = &(eventBuffer[eventCounter]);
        VstMidiEvent *pMidiEvent =  (VstMidiEvent*)(pVstEvent);
        VstMidiEvent midiEvent   =  *pMidiEvent;

        handleEvent(midiEvent);
        eventCounter++;
      }
      else // the event was not of kVstMidiType and is ignored:
        eventCounter++;
    }

    // render audio and increment buffer pointers:
    *out1 = *out2 = (float) open303Core.getSample();
    out1++;
    out2++;
  }

  // delete vst event buffer, it's invalid now:
  if(eventBuffer)
    delete[] eventBuffer;
  eventBuffer = NULL;
  eventBufferLength = 0;
}

VstInt32 Open303VST::processEvents (VstEvents* ev)
{
  // delete old vst event buffer:
  if( eventBuffer != NULL )
  {
    delete[] eventBuffer;
    eventBuffer = NULL;
  }

  //...and create new buffer for the new vst events
  eventBufferLength = ev->numEvents;
  if( eventBufferLength > 0 )
    eventBuffer = new VstEvent[eventBufferLength];

  // copy the events into the event-buffer (actual event processing is done in the audio process
  // method):
  if( eventBuffer != NULL )
  {
    for(int i=0; i<eventBufferLength; i++)
    {
      eventBuffer[i].byteSize    = ev->events[i]->byteSize;
      eventBuffer[i].deltaFrames = ev->events[i]->deltaFrames;
      eventBuffer[i].flags       = ev->events[i]->flags;
      eventBuffer[i].type        = ev->events[i]->type;
      for(int j=0; j<16; j++)
        eventBuffer[i].data[j]   = ev->events[i]->data[j];
    }
  }

  return 1;	 // we want more events
}

//-------------------------------------------------------------------------------------------------
// program handling:

void Open303VST::setProgram(VstInt32 program)
{
	if(program < 0 || program >= numPrograms)
		return;

	Open303VSTProgram *ap = &programs[program];
	curProgram             = program;
  for(int i=0; i<OPEN303_NUM_PARAMETERS; i++)
    setParameter(i, ap->parameters[i]);
}

void Open303VST::setProgramName (char* name)
{
	vst_strncpy (programs[curProgram].name, name, kVstMaxProgNameLen);
}

void Open303VST::getProgramName (char* name)
{
	vst_strncpy (name, programs[curProgram].name, kVstMaxProgNameLen);
}

bool Open303VST::getProgramNameIndexed (VstInt32 category, VstInt32 index, char* text)
{
	if (index < numPrograms)
	{
		vst_strncpy (text, programs[index].name, kVstMaxProgNameLen);
		return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
// parameter handling:

void Open303VST::setParameter (VstInt32 index, float value)
{
  if( index < 0 || index >= OPEN303_NUM_PARAMETERS )
    return;

	Open303VSTProgram *ap = &programs[curProgram];
  ap->parameters[index]  = value;
	switch(index)
	{
  case WAVEFORM:
    open303Core.setWaveform( linToLin(value, 0.0, 1.0,   0.0,      1.0) );
    break;
  case TUNING:
    open303Core.setTuning(   linToLin(value, 0.0, 1.0,  400.0,    480.0) );
    break;
  case CUTOFF:
    open303Core.setCutoff(   linToExp(value, 0.0, 1.0, 314.0,    2394.0) );
    break;
  case RESONANCE:
    open303Core.setResonance(linToLin(value, 0.0, 1.0,   0.0,    100.0) );
    break;
  case ENVMOD:
    open303Core.setEnvMod(   linToLin(value, 0.0, 1.0,    0.0,   100.0)  );
    break;
  case DECAY:
    open303Core.setDecay(    linToExp(value, 0.0, 1.0,  200.0,  2000.0) );
    break;
  case ACCENT:
    open303Core.setAccent(   linToLin(value, 0.0, 1.0,   0.0,    100.0) );
    break;
  case VOLUME:
    open303Core.setVolume(   linToLin(value, 0.0, 1.0, -60.0,      0.0)  );
    break;
  case FILTER_TYPE:
      open303Core.filter.setMode(  normalizedValueToIndex(value, TeeBeeFilter::NUM_MODES) );
    break;

#ifdef SHOW_INTERNAL_PARAMETERS
  case AMP_SUSTAIN:
    open303Core.setAmpSustain(        linToLin(value, 0.0, 1.0, -60.0,      0.0)  );
    break;
  case TANH_SHAPER_DRIVE:
    open303Core.setTanhShaperDrive(   linToLin(value, 0.0, 1.0,   0.0,     60.0)  );
    break;
  case TANH_SHAPER_OFFSET:
    open303Core.setTanhShaperOffset(  linToLin(value, 0.0, 1.0, -10.0,     10.0)  );
    break;
  case PRE_FILTER_HPF:
    open303Core.setPreFilterHighpass( linToExp(value, 0.0, 1.0,  10.0,    500.0)  );
    break;
  case FEEDBACK_HPF:
    open303Core.setFeedbackHighpass(  linToExp(value, 0.0, 1.0,  10.0,    500.0)  );
    break;
  case POST_FILTER_HPF:
    open303Core.setPostFilterHighpass(linToExp(value, 0.0, 1.0,  10.0,    500.0)  );
    break;
  case SQUARE_PHASE_SHIFT:
    open303Core.setSquarePhaseShift(  linToLin(value, 0.0, 1.0,   0.0,    360.0)  );
    break;
#endif

	}
}

float Open303VST::getParameter(VstInt32 index)
{
  if( index < 0 || index >= OPEN303_NUM_PARAMETERS )
    return 0.f;
  else
    return programs[curProgram].parameters[index];
}

void Open303VST::getParameterLabel(VstInt32 index, char* label)
{
  if( index < 0 || index >= OPEN303_NUM_PARAMETERS )
    return;
	switch(index)
	{
		case WAVEFORM:  vst_strncpy(label, "",   kVstMaxParamStrLen); break;
		case TUNING:    vst_strncpy(label, "Hz", kVstMaxParamStrLen); break;
		case CUTOFF:    vst_strncpy(label, "Hz", kVstMaxParamStrLen); break;
		case RESONANCE: vst_strncpy(label, "%",  kVstMaxParamStrLen); break;
		case ENVMOD:    vst_strncpy(label, "%",  kVstMaxParamStrLen); break;
		case DECAY:     vst_strncpy(label, "ms", kVstMaxParamStrLen); break;
		case ACCENT:    vst_strncpy(label, "%",  kVstMaxParamStrLen); break;
		case VOLUME:    vst_strncpy(label, "dB", kVstMaxParamStrLen); break;

#ifdef SHOW_INTERNAL_PARAMETERS
		case AMP_SUSTAIN:        vst_strncpy(label, "dB",  kVstMaxParamStrLen); break;
    case TANH_SHAPER_DRIVE:  vst_strncpy(label, "dB",  kVstMaxParamStrLen); break;
    case TANH_SHAPER_OFFSET: vst_strncpy(label, "",    kVstMaxParamStrLen); break;
		case PRE_FILTER_HPF:     vst_strncpy(label, "Hz",  kVstMaxParamStrLen); break;
		case FEEDBACK_HPF:       vst_strncpy(label, "Hz",  kVstMaxParamStrLen); break;
		case POST_FILTER_HPF:    vst_strncpy(label, "Hz",  kVstMaxParamStrLen); break;
		case SQUARE_PHASE_SHIFT: vst_strncpy(label, "deg", kVstMaxParamStrLen); break;
#endif

    default:        vst_strncpy(label, "",   kVstMaxParamStrLen); break;
	}
}

void Open303VST::getParameterDisplay(VstInt32 index, char* text)
{
	//
  if( index < 0 || index >= OPEN303_NUM_PARAMETERS )
  {
    text[0] = 0;
    return;
  }

	switch (index)
	{
    // \todo: using sprintf is unsafe because it may write beyond the char-arrray boundary
    // ...but float2string from the vst-sdk creates a messed up string - mmmhhhh
  case WAVEFORM:  sprintf(text, "%.2f", open303Core.getWaveform());    break;
  case TUNING:    sprintf(text, "%.2f", open303Core.getTuning());      break;
  case CUTOFF:    sprintf(text, "%.2f", open303Core.getCutoff());      break;
  case RESONANCE: sprintf(text, "%.2f", open303Core.getResonance());   break;
  case ENVMOD:    sprintf(text, "%.2f", open303Core.getEnvMod());      break;
  case DECAY:     sprintf(text, "%.2f", open303Core.getDecay());       break;
  case ACCENT:    sprintf(text, "%.2f", open303Core.getAccent());      break;
  case VOLUME:    sprintf(text, "%.2f", open303Core.getVolume());      break;
  case FILTER_TYPE:
    {
      switch( open303Core.filter.getMode() )
      {
      case TeeBeeFilter::FLAT:     vst_strncpy(text, "Flat",     kVstMaxParamStrLen);  break;
      case TeeBeeFilter::LP_6:     vst_strncpy(text, "LP 6",     kVstMaxParamStrLen);  break;
      case TeeBeeFilter::LP_12:    vst_strncpy(text, "LP 12",    kVstMaxParamStrLen);  break;
      case TeeBeeFilter::LP_18:    vst_strncpy(text, "LP 18",    kVstMaxParamStrLen);  break;
      case TeeBeeFilter::LP_24:    vst_strncpy(text, "LP 24",    kVstMaxParamStrLen);  break;
      case TeeBeeFilter::HP_6:     vst_strncpy(text, "HP 6",     kVstMaxParamStrLen);  break;
      case TeeBeeFilter::HP_12:    vst_strncpy(text, "HP 12",    kVstMaxParamStrLen);  break;
      case TeeBeeFilter::HP_18:    vst_strncpy(text, "HP 18",    kVstMaxParamStrLen);  break;
      case TeeBeeFilter::HP_24:    vst_strncpy(text, "HP 24",    kVstMaxParamStrLen);  break;
      case TeeBeeFilter::BP_12_12: vst_strncpy(text, "BP 12/12", kVstMaxParamStrLen);  break;
      case TeeBeeFilter::BP_6_18:  vst_strncpy(text, "BP 6/18",  kVstMaxParamStrLen);  break;
      case TeeBeeFilter::BP_18_6:  vst_strncpy(text, "BP 18/6",  kVstMaxParamStrLen);  break;
      case TeeBeeFilter::BP_6_12:  vst_strncpy(text, "BP 6/12",  kVstMaxParamStrLen);  break;
      case TeeBeeFilter::BP_12_6:  vst_strncpy(text, "BP 12/6",  kVstMaxParamStrLen);  break;
      case TeeBeeFilter::BP_6_6:   vst_strncpy(text, "BP 6/6",   kVstMaxParamStrLen);  break;
      case TeeBeeFilter::TB_303:   vst_strncpy(text, "TB 303",   kVstMaxParamStrLen);  break;

      default: vst_strncpy(text, "Flat", kVstMaxParamStrLen);
      }
    }                                                                                  break;

#ifdef SHOW_INTERNAL_PARAMETERS
  case AMP_SUSTAIN:        sprintf(text, "%.2f", open303Core.getAmpSustain());         break;
  case TANH_SHAPER_DRIVE:  sprintf(text, "%.2f", open303Core.getTanhShaperDrive());    break;
  case TANH_SHAPER_OFFSET: sprintf(text, "%.2f", open303Core.getTanhShaperOffset());   break;
  case PRE_FILTER_HPF:     sprintf(text, "%.2f", open303Core.getPreFilterHighpass());  break;
  case FEEDBACK_HPF:       sprintf(text, "%.2f", open303Core.getFeedbackHighpass());   break;
  case POST_FILTER_HPF:    sprintf(text, "%.2f", open303Core.getPostFilterHighpass()); break;
  case SQUARE_PHASE_SHIFT: sprintf(text, "%.2f", open303Core.getSquarePhaseShift());   break;
#endif

  default: vst_strncpy(text, "Not Implemented", kVstMaxParamStrLen);
	}
}

void Open303VST::getParameterName (VstInt32 index, char* label)
{
	switch(index)
	{
		case WAVEFORM:         vst_strncpy(label, "Waveform",      kVstMaxParamStrLen);	    break;
		case TUNING:           vst_strncpy(label, "Tuning",        kVstMaxParamStrLen);	    break;
		case CUTOFF:           vst_strncpy(label, "Cutoff",        kVstMaxParamStrLen);	    break;
		case RESONANCE:        vst_strncpy(label, "Resonance",     kVstMaxParamStrLen);	    break;
		case ENVMOD:           vst_strncpy(label, "EnvMod",        kVstMaxParamStrLen);	    break;
		case DECAY:            vst_strncpy(label, "Decay",         kVstMaxParamStrLen);	    break;
		case ACCENT:           vst_strncpy(label, "Accent",        kVstMaxParamStrLen);	    break;
		case VOLUME:           vst_strncpy(label, "Volume",        kVstMaxParamStrLen);	    break;
		case FILTER_TYPE:      vst_strncpy(label, "FilterMode",    kVstMaxParamStrLen);	    break;

#ifdef SHOW_INTERNAL_PARAMETERS
		case AMP_SUSTAIN:           vst_strncpy(label, "AmpSustain",    kVstMaxParamStrLen);	 break;
		case TANH_SHAPER_DRIVE:     vst_strncpy(label, "TanhDrive",     kVstMaxParamStrLen);	 break;
		case TANH_SHAPER_OFFSET:    vst_strncpy(label, "TanhOffset",    kVstMaxParamStrLen);	 break;
		case PRE_FILTER_HPF:        vst_strncpy(label, "Pre HPF",       kVstMaxParamStrLen);	 break;
		case FEEDBACK_HPF:          vst_strncpy(label, "Feedback HPF",  kVstMaxParamStrLen);	 break;
		case POST_FILTER_HPF:       vst_strncpy(label, "Post HPF",      kVstMaxParamStrLen);	 break;
		case SQUARE_PHASE_SHIFT:    vst_strncpy(label, "SquarePhase",   kVstMaxParamStrLen);	 break;
#endif

    default: vst_strncpy(label, "Not Implemented", kVstMaxParamStrLen);
	}
}

//-------------------------------------------------------------------------------------------------
// audio processing parameters:

void Open303VST::setSampleRate (float sampleRate)
{
	AudioEffectX::setSampleRate(sampleRate);
  open303Core.setSampleRate(sampleRate);
}

void Open303VST::setBlockSize (VstInt32 blockSize)
{
	AudioEffectX::setBlockSize(blockSize);
	// you may need to have to do something here...
}

//-------------------------------------------------------------------------------------------------
// callbacks to pass plugin info to the host:

bool Open303VST::getOutputProperties (VstInt32 index, VstPinProperties* properties)
{
	if(index < numOutputs)
	{
		vst_strncpy (properties->label, "Open303VST ", 63);
		char temp[11] = {0};
		int2string (index + 1, temp, 10);
		vst_strncat (properties->label, temp, 63);

		properties->flags = kVstPinIsActive;
		if (index < 2)
			properties->flags |= kVstPinIsStereo;	// make channel 1+2 stereo
		return true;
	}
	return false;
}

bool Open303VST::getEffectName (char* name)
{
	vst_strncpy (name, "Open303", kVstMaxEffectNameLen);
	return true;
}

bool Open303VST::getVendorString (char* text)
{
	vst_strncpy (text, "RS-MET and others", kVstMaxVendorStrLen);
	return true;
}

bool Open303VST::getProductString (char* text)
{
	vst_strncpy (text, "Open303", kVstMaxProductStrLen);
	return true;
}

VstInt32 Open303VST::getVendorVersion ()
{
	return 1000;
}

VstInt32 Open303VST::canDo (char* text)
{
	if (!strcmp (text, "receiveVstEvents"))
		return 1;
	if (!strcmp (text, "receiveVstMidiEvent"))
		return 1;
	return -1;	// explicitly can't do; 0 => don't know
}

//-------------------------------------------------------------------------------------------------
// internal functions:

void Open303VST::handleEvent(VstMidiEvent midiEvent)
{
  char* midiData = midiEvent.midiData;

  // converts all messages to corresponding messages on MIDI channel 1:
  long status = midiData[0] & 0xf0;

  // respond to note-on and note-off on channel 1:
  if( status == 0x90 || status == 0x80 )
  {
    // bitwise AND with 0x7f=01111111 sets the first bit of an 8-bit word to zero, allowing
    // only numbers from 0 to 127 to pass unchanged, higher numbers (128-255) will be mapped
    // into this range by subtracting 128:
    long note     = midiData[1] & 0x7f;
    long velocity = midiData[2] & 0x7f;
    long detune   = midiEvent.detune;

    // respond to note-off on channel 1 (status: 0x80)
    if( status == 0x80 )
      velocity = 0;	// note off by note-off message

    // respond to note-on (note-offs are handled there too by recognizing zero velocity):
    noteOn(note, velocity, detune);
  }

  // respond to all notes off:
  // (status=0xb0: controller on ch 1, midiData[1]=0x7b: control 123: all notes off):
  else if( (status) == 0xb0 && (midiData[1] == 0x7b) )
  {
    for(int i=0; i <= 127; i++)
      noteOn(i, 0, 0);
  }

  // respond to MIDI-Controllers on channel 1:
  else if( status == 0xb0 )
  {
    switch( midiData[1] )
    {
    case   7:  setParameterAutomated(VOLUME,            (float)midiData[2]/127.f);  break;
    case  74: setParameterAutomated( CUTOFF,            (float)midiData[2]/127.f);  break;
    case  71: setParameterAutomated( RESONANCE,         (float)midiData[2]/127.f);  break;
    case  81: setParameterAutomated( ENVMOD,            (float)midiData[2]/127.f);  break;
      // more to come....
    }
  }

  // respond to pitchbend:
  else if (status == 0xe0)
  {
    double pitchBend = convertToPitch(midiData[1], midiData[2]);
    open303Core.setPitchBend(pitchBend);
  }
}

void Open303VST::noteOn(VstInt32 note, VstInt32 velocity, VstInt32 delta)
{
  open303Core.noteOn(note, velocity, 0.0);
}

double Open303VST::convertToPitch(unsigned char highByte,unsigned char lowByte)
{
  unsigned short bendValue;  // value of the wheel (between 0x0000 and 0x3FFF, center: 0x2000(=8192))
  double         pitchValue; // mapped value (into +-12 semitones)

  double range = 12.0;       // pitch wheel range in semitones - maybe made accessible as user parameter

  bendValue   = (unsigned short) lowByte;
  bendValue <<= 7;
  bendValue  |= (unsigned short) highByte;
    // bendValue now holds a value between 0x0000 and 0x3FFF where 0x2000 means: pitch wheel is
    // centered

  pitchValue = (double)bendValue - 8192;   // pitchValue is between -8192 and +8191
  if(pitchValue == -8192)
    pitchValue = -8191;                    // for symmetry
  pitchValue = (pitchValue/8191)*range;    // pitchValue is between -12 and +12

  return pitchValue;
}






