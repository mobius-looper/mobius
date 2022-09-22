
//host.cpp
//attempts to load and make use of a VST plugin

// need this for LoadLibrary, FreeLibrary, GetProcAddress, Sleep
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>
#include <stdio.h>

//#include <iostream.h>
//#include <fstream.h>

#include "aeffectx.h"

const int blocksize=512;
const float samplerate=44100.0f;

long VSTCALLBACK host(AEffect *effect, long opcode, long index, long value, void *ptr, float opt);

void main(int argc, char** argv)
{
	////////////////////////////////////////////////////////////////////////////
	//
	//	Loading a plugin
	//
	////////////////////////////////////////////////////////////////////////////

	//create a pointer for the plugin we're going to load
	AEffect* ptrPlug=NULL;
	bool editor;

	//find and load the DLL and get a pointer to its main function
	//this has a protoype like this: AEffect *main (audioMasterCallback audioMaster)

    if (argc < 2) {
        printf("Host <plugin>\n");
        return;
    }

    char* pluginName = argv[1];
    printf("Loading plugin %s\n", pluginName);
    fflush(stdout);

	HINSTANCE libhandle=LoadLibrary(pluginName);
    
	if (libhandle!=NULL)
	{
        fflush(stdout);
		//DLL was loaded OK
		AEffect* (__cdecl* getNewPlugInstance)(audioMasterCallback);
		getNewPlugInstance=(AEffect*(__cdecl*)(audioMasterCallback))GetProcAddress(libhandle, "main");

		if (getNewPlugInstance!=NULL)
		{
			//main function located OK
			ptrPlug=getNewPlugInstance(host);

			if (ptrPlug!=NULL)
			{
				//plugin instantiated OK
				printf("Plugin was loaded OK\n");

				if (ptrPlug->magic==kEffectMagic)
				{
					printf("It's a valid VST plugin\n");
				}
				else
				{
					printf("Not a VST plugin\n");
					FreeLibrary(libhandle);
					//Sleep(5000);
					return;
				}
			}
			else
			{
				printf("Plugin could not be instantiated\n");
				FreeLibrary(libhandle);
				//Sleep(5000);
				return;
			}
		}
		else
		{
			printf("Plugin main function could not be located\n");
			FreeLibrary(libhandle);
			//Sleep(5000);
			return;
		}
	}
	else
	{
		printf("Plugin DLL could not be loaded\n");
		//Sleep(5000);
		return;
	};

    
	////////////////////////////////////////////////////////////////////////////
	//
	//	Examining the plugin
	//
	////////////////////////////////////////////////////////////////////////////

	if (ptrPlug->dispatcher(ptrPlug,effGetVstVersion,0,0,NULL,0.0f)==2)
	{
		printf("This is a VST 2 plugin\n");
	}
	else
	{
		printf("This is a VST 1 plugin\n");
	}

	//Member variables:
	printf("numPrograms %ld\n", ptrPlug->numPrograms);
	printf("numParams %ld\n", ptrPlug->numParams);
	printf("numInputs %ld\n", ptrPlug->numInputs);
	printf("numOutputs %ld\n", ptrPlug->numOutputs);
	printf("resvd1 %ld\n", ptrPlug->resvd1);
	printf("resvd2 %ld\n", ptrPlug->resvd2);
	printf("initialDelay %ld\n", ptrPlug->initialDelay);
	printf("realQualities %ld\n", ptrPlug->realQualities);
	printf("offQualities %ld\n", ptrPlug->offQualities);
	printf("ioRatio %f\n", ptrPlug->ioRatio);
	printf("object 0x%p\n", ptrPlug->object);
	printf("user 0x%p\n", ptrPlug->user);
	printf("uniqueID %ld\n", ptrPlug->uniqueID);
	printf("version %ld\n", ptrPlug->version);

	//VST 1.0 flags
	if (ptrPlug->flags & effFlagsHasEditor)
	{
		//Meaning: plugin has its own GUI editor
		printf("plug has the effFlagsHasEditor flag\n");
		editor=true;
	}
    
	if (ptrPlug->flags & effFlagsHasClip)
	{
		//Meaning: plugin can provide information to drive a clipping display
		printf("plug has the effFlagsHasClip flag\n");
	}

	if (ptrPlug->flags & effFlagsHasVu)
	{
		//Meaning: plugin can provide information to drive a VU display
		printf("plug has the effFlagsHasVu flag\n");
	}

	if (ptrPlug->flags & effFlagsCanMono)
	{
		//Meaning: plugin can accept mono input

		//NB - if the plugin has two input buffers they can both be the same
		printf("plug has the effFlagsCanMono flag\n");
	}

	if (ptrPlug->flags & effFlagsCanReplacing)
	{
		//Meaning: plugin supports a replacing process
		//The values in any output buffer given to the plug will be overwritten
		//however, due to glitches in a few VST instruments it's advisable to
		//zero these buffers anyway before handing them over.
		printf("plug has the effFlagsCanReplacing flag\n");
	}

	if (ptrPlug->flags & effFlagsProgramChunks)
	{
		//Meaning: plugin receives program and bank information via chunk method
		printf("plug has the effFlagsProgramChunks flag\n");
	}
    
	if (ptrPlug->dispatcher(ptrPlug,effGetVstVersion,0,0,NULL,0.0f)==2)
	{
		//VST 2.0 flags
		if (ptrPlug->flags & effFlagsIsSynth)
		{
			//Meaning: plugin will require MIDI events, and processEvents must be
			//called before process or processReplacing. Also, plugin cannot be
			//used as an insert or send effect and will declare its own number of
			//outputs, which should be assigned by the host as mixer channels
			printf("plug has the effFlagsIsSynth flag\n");
		}

		if (ptrPlug->flags & effFlagsNoSoundInStop)
		{
			//Meaning: if the transport is stopped the host need not call process
			//or processReplacing - or if no input available?
			printf("plug has the effFlagsNoSoundInStop flag\n");
		}

		//also effFlagsExtIsAsync and effFlagsExtHasBuffer - refers to plugin that
		//supports external DSP

		//VST 2.0 Plugin Can Do's
		if (ptrPlug->dispatcher(ptrPlug,effCanDo,0,0,"sendVstEvents",0.0f)>0)
		{
			printf("plug can sendVstEvents\n");
		}

		if (ptrPlug->dispatcher(ptrPlug,effCanDo,0,0,"sendVstMidiEvent",0.0f)>0)
		{
			printf("plug can sendVstMidiEvent\n");
		}

		if (ptrPlug->dispatcher(ptrPlug,effCanDo,0,0,"sendVstTimeInfo",0.0f)>0)
		{
			printf("plug can sendVstTimeInfo\n");
		}

		if (ptrPlug->dispatcher(ptrPlug,effCanDo,0,0,"receiveVstEvents",0.0f)>0)
		{
			printf("plug can receiveVstEvents\n");
		}

		if (ptrPlug->dispatcher(ptrPlug,effCanDo,0,0,"receiveVstMidiEvent",0.0f)>0)
		{
			printf("plug can receiveVstMidiEvent\n");
		}

		if (ptrPlug->dispatcher(ptrPlug,effCanDo,0,0,"receiveVstTimeInfo",0.0f)>0)
		{
			printf("plug can receiveVstTimeInfo\n");
		}

		if (ptrPlug->dispatcher(ptrPlug,effCanDo,0,0,"offline",0.0f)>0)
		{
			printf("plug can offline\n");
		}

		if (ptrPlug->dispatcher(ptrPlug,effCanDo,0,0,"plugAsChannelInsert",0.0f)>0)
		{
			printf("plug can plugAsChannelInsert\n");
		}

		if (ptrPlug->dispatcher(ptrPlug,effCanDo,0,0,"plugAsSend",0.0f)>0)
		{
			printf("plug can plugAsSend\n");
		}

		if (ptrPlug->dispatcher(ptrPlug,effCanDo,0,0,"mixDryWet",0.0f)>0)
		{
			printf("plug can mixDryWet\n");
		}

	}	//end VST2 specific

	////////////////////////////////////////////////////////////////////////////
	//
	//	Using the plugin's C Interface - an example
	//
	////////////////////////////////////////////////////////////////////////////

	long rc=0;

	//try some dispatcher functions...
	ptrPlug->dispatcher(ptrPlug,effOpen,0,0,NULL,0.0f);

	//switch the plugin off (calls Suspend)
	ptrPlug->dispatcher(ptrPlug,effMainsChanged,0,0,NULL,0.0f);

	//set sample rate and block size
	ptrPlug->dispatcher(ptrPlug,effSetSampleRate,0,0,NULL,samplerate);
	ptrPlug->dispatcher(ptrPlug,effSetBlockSize,0,blocksize,NULL,0.0f);

	if ((ptrPlug->dispatcher(ptrPlug,effGetVstVersion,0,0,NULL,0.0f)==2) &&
		(ptrPlug->flags & effFlagsIsSynth))
	{
		//get I/O configuration for synth plugins - they will declare their
		//own output and input channels
		for (int i=0; i<ptrPlug->numInputs+ptrPlug->numOutputs;i++)
		{
			if (i<ptrPlug->numInputs)
			{
				//input pin
				VstPinProperties temp;

				if (ptrPlug->dispatcher(ptrPlug,effGetInputProperties,i,0,&temp,0.0f)==1)
				{

                    printf("Input pin %d label %s\n", i+1, temp.label);

					if (temp.flags & kVstPinIsActive)
					{
						printf("Input pin %d is active\n", i+1);
					}

					if (temp.flags & kVstPinIsStereo)
					{
						// is index even or zero?
						if (i%2==0 || i==0)
						{
							printf("Input pin %d is left channel of a stereo pair\n", i+1);
						}
						else
						{
							printf("Input pin %d is right channel of a stereo pair\n", i+1);
						}
					}
				}
			}
			else
			{
				//output pin
				VstPinProperties temp;

				if (ptrPlug->dispatcher(ptrPlug,effGetOutputProperties,i,0,&temp,0.0f)==1)
				{
					printf("Output pin %d label %s\n", i-ptrPlug->numInputs+1, temp.label);

					if (temp.flags & kVstPinIsActive)
					{
                        printf("Output pin %d is active\n", (i-ptrPlug->numInputs+1));
					}
					else
					{
                        printf("Output pin %d is inactive\n", (i-ptrPlug->numInputs+1));
					}

					if (temp.flags & kVstPinIsStereo)
					{
						// is index even or zero?
						if ((i-ptrPlug->numInputs)%2==0 || (i-ptrPlug->numInputs)==0)
						{
							printf("Output pin %d is left channel of stereo pair\n", (i+1));
						}
						else
						{
							printf("Output pin %d is right channel of stereo pair\n", (i+1));
						}
					}
					else
					{
						printf("Output pin %d is mono\n", (i+1));
					}
				}
			}
		}
	}	//end VST2 specific

	//switch the plugin back on (calls Resume)
	ptrPlug->dispatcher(ptrPlug,effMainsChanged,0,1,NULL,0.0f);

	////////////////////////////////////////////////////////////////////////////

	//set to program zero and fetch back the name
	ptrPlug->dispatcher(ptrPlug,effSetProgram,0,0,NULL,0.0f);

	char strProgramName[24+2];
	ptrPlug->dispatcher(ptrPlug,effGetProgramName,0,0,strProgramName,0.0f);

	printf("Set plug to program zero, name is %s\n", strProgramName);

	//get the parameter strings
	char strName[24];
	char strDisplay[24];
	char strLabel[24];

	ptrPlug->dispatcher(ptrPlug,effGetParamName,0,0,strName,0.0f);
	printf("Parameter name is %s\n", strName);

	ptrPlug->dispatcher(ptrPlug,effGetParamLabel,0,0,strLabel,0.0f);
	printf("Parameter label is %s\n", strLabel);

	ptrPlug->dispatcher(ptrPlug,effGetParamDisplay,0,0,strDisplay,0.0f);
	printf("Parameter display is %s\n", strDisplay);

	//call the set and get parameter functions
	ptrPlug->setParameter(ptrPlug,0,0.7071f);
	float newval=ptrPlug->getParameter(ptrPlug,0);

	printf("Parameter 0 was changed to %f\n", newval);
	ptrPlug->dispatcher(ptrPlug,effGetParamDisplay,0,0,strDisplay,0.0f);
	printf("Parameter display is now %s\n", strDisplay);

	////////////////////////////////////////////////////////////////////////////

	VstEvent* ptrEvents=NULL;
	char* ptrEventBuffer=NULL;
	int nEvents=4;

	if ((ptrPlug->dispatcher(ptrPlug,effGetVstVersion,0,0,NULL,0.0f)==2) &&
	   ((ptrPlug->flags & effFlagsIsSynth) ||
		(ptrPlug->dispatcher(ptrPlug,effCanDo,0,0,"receiveVstEvents",0.0f)>0)))
	{
		//create some MIDI messages to send to the plug if it is a synth or can
		//receive MIDI messages

		//create a block of appropriate size
		int pointersize=sizeof(VstEvent*);
		int bufferSize=sizeof(VstEvents)-(pointersize*2);
		bufferSize+=pointersize*(nEvents);

		//create the buffer
		ptrEventBuffer=new char[bufferSize+1];

		//now, create some memory for the events themselves
		VstMidiEvent* ptrEvents=new VstMidiEvent[nEvents];
		VstMidiEvent* ptrWrite=ptrEvents;

		//Create first event
		ptrWrite->type=kVstMidiType;
		ptrWrite->byteSize=24L;
		ptrWrite->deltaFrames=0L;
		ptrWrite->flags=0L;
		ptrWrite->noteLength=0L;
		ptrWrite->noteOffset=0L;

		ptrWrite->midiData[0]=(char)0x90;	//status & channel
		ptrWrite->midiData[1]=(char)0x3C;	//MIDI byte #2
		ptrWrite->midiData[2]=(char)0xFF;	//MIDI byte #3
		ptrWrite->midiData[3]=(char)0x00;	//MIDI byte #4 - blank

		ptrWrite->detune=0x00;
		ptrWrite->noteOffVelocity=0x00;
		ptrWrite->reserved1=0x00;
		ptrWrite->reserved2=0x00;

		//Create second event
		ptrWrite++;
		ptrWrite->type=kVstMidiType;
		ptrWrite->byteSize=24L;
		ptrWrite->deltaFrames=128L;
		ptrWrite->flags=0L;
		ptrWrite->noteLength=0L;
		ptrWrite->noteOffset=0L;

		ptrWrite->midiData[0]=(char)0x90;	//status & channel
		ptrWrite->midiData[1]=(char)0x3C;	//MIDI byte #2
		ptrWrite->midiData[2]=(char)0x00;	//MIDI byte #3
		ptrWrite->midiData[3]=(char)0x00;	//MIDI byte #4 - blank

		ptrWrite->detune=0x00;
		ptrWrite->noteOffVelocity=0x00;
		ptrWrite->reserved1=0x00;
		ptrWrite->reserved2=0x00;

		//Create third event
		ptrWrite++;
		ptrWrite->type=kVstMidiType;
		ptrWrite->byteSize=24L;
		ptrWrite->deltaFrames=256L;
		ptrWrite->flags=0L;
		ptrWrite->noteLength=0L;
		ptrWrite->noteOffset=0L;

		ptrWrite->midiData[0]=(char)0x90;	//status & channel
		ptrWrite->midiData[1]=(char)0x3C;	//MIDI byte #2
		ptrWrite->midiData[2]=(char)0xFF;	//MIDI byte #3
		ptrWrite->midiData[3]=(char)0x00;	//MIDI byte #4 - blank

		ptrWrite->detune=0x00;
		ptrWrite->noteOffVelocity=0x00;
		ptrWrite->reserved1=0x00;
		ptrWrite->reserved2=0x00;

		//Create fourth event
		ptrWrite++;
		ptrWrite->type=kVstMidiType;
		ptrWrite->byteSize=24L;
		ptrWrite->deltaFrames=384L;
		ptrWrite->flags=0L;
		ptrWrite->noteLength=0L;
		ptrWrite->noteOffset=0L;

		ptrWrite->midiData[0]=(char)0x90;	//status & channel
		ptrWrite->midiData[1]=(char)0x3C;	//MIDI byte #2
		ptrWrite->midiData[2]=(char)0x00;	//MIDI byte #3
		ptrWrite->midiData[3]=(char)0x00;	//MIDI byte #4 - blank

		ptrWrite->detune=0x00;
		ptrWrite->noteOffVelocity=0x00;
		ptrWrite->reserved1=0x00;
		ptrWrite->reserved2=0x00;

		//copy the addresses of our events into the eventlist structure
		VstEvents* ev=(VstEvents*)ptrEventBuffer;
		for (int i=0;i<nEvents;i++)
		{
			ev->events[i]=(VstEvent*)(ptrEvents+i);
		}

		//do the block header
		ev->numEvents=nEvents;
		ev->reserved=0L;

	}

	////////////////////////////////////////////////////////////////////////////
	float** ptrInputBuffers=NULL;
	float** ptrOutputBuffers=NULL;

	if (ptrPlug->numInputs)
	{

		if (ptrPlug->flags & effFlagsCanMono)
		{
			//only one input buffer required, others are just copies?
			//This could be an incorrect interpretation

			ptrInputBuffers=new float*[ptrPlug->numInputs];

			//create the input buffers
			for (int i=0;i<ptrPlug->numInputs;i++)
			{
				if (i==0)
				{
					printf("Input buffer %d created\n", (i+1));
					ptrInputBuffers[i]=new float[blocksize];
				}
				else
				{
					printf("Input buffer %d is a copy of input buffer 1\n", (i+1));
					ptrInputBuffers[i]=ptrInputBuffers[0];
				}
			}
		}
		else
		{
			//Plug requires independent input signals
			ptrInputBuffers=new float*[ptrPlug->numInputs];

			//create the input buffers
			for (int i=0;i<ptrPlug->numInputs;i++)
			{
                printf("Input buffer %d created\n", (i+1));
				ptrInputBuffers[i]=new float[blocksize];
			}
		}
	}

	if (ptrPlug->numOutputs)
	{
		ptrOutputBuffers=new float*[ptrPlug->numOutputs];

		//create the output buffers
		for (int i=0;i<ptrPlug->numOutputs;i++)
		{
            printf("Output buffer %d created\n", (i+1));
			ptrOutputBuffers[i]=new float[blocksize];
		}
	}

	if (ptrPlug->numInputs>0)
	{
		//fill the input buffers with ones to simulate some input having been
		//received

		//We don't have to do this for synths, obviously
		for (int i=0;i<ptrPlug->numInputs;i++)
		{
			for (int j=0;j<blocksize;j++)
			{
				*(ptrInputBuffers[i]+j)=1.0f;
			}
		}
	}

	if (ptrPlug->numOutputs>0)
	{
		//fill the output buffers with ones to simulate some prior output to be
		//accumulated on
		for (int i=0;i<ptrPlug->numOutputs;i++)
		{
			for (int j=0;j<blocksize;j++)
			{
				//*(ptrOutputBuffers[i]+j)=-1.0f;

				//note that VSTXsynth, when replacing, requires the host
				//to clear the buffer first!
				*(ptrOutputBuffers[i]+j)=0.0f;
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////

    FILE* outfile;
    outfile = fopen("hello.txt", "w");

	//ProcessEvents
	if ((ptrPlug->dispatcher(ptrPlug,effGetVstVersion,0,0,NULL,0.0f)==2) &&
	   ((ptrPlug->flags & effFlagsIsSynth) ||
		(ptrPlug->dispatcher(ptrPlug,effCanDo,0,0,"receiveVstEvents",0.0f)>0)))
	{
		if (ptrPlug->dispatcher(ptrPlug,effProcessEvents,0,0,(VstEvents*)ptrEventBuffer,0.0f)==1)
		{
			printf("plug processed events OK and wants more\n");
		}
		else
		{
			printf("plug does not want any more events\n");
		}
	}

	//process (replacing)
	if (ptrPlug->flags & effFlagsCanReplacing)
	{
		printf("Process (replacing)\n");
		ptrPlug->processReplacing(ptrPlug,ptrInputBuffers,ptrOutputBuffers,blocksize);
	}
/*
	//process (accumulating)
	printf("Process (accumulating)\n");
	ptrPlug->process(ptrPlug,ptrInputBuffers,ptrOutputBuffers,sampleframes);
*/

	//Dump output to disk
	for (int i=0;i<ptrPlug->numOutputs;i++)
	{
		for (int j=0;j<blocksize;j++)
		{
			fprintf(outfile, "%d,%d,%f\n", i, j, *(ptrOutputBuffers[i]+j));
		}
	}

	fclose(outfile);

	printf("Press any key to close...\n");
	fflush(stdout);
	fgetc(stdin);

	
	////////////////////////////////////////////////////////////////////////////

	ptrPlug->dispatcher(ptrPlug,effMainsChanged,0,0,NULL,0.0f);	//calls suspend

	////////////////////////////////////////////////////////////////////////////
	
	//delete the MIDI data
	if (ptrEventBuffer!=NULL)
	{
		delete[] ptrEventBuffer;
	}

	if (ptrEvents!=NULL)
	{
		delete[] ptrEvents;
	}

	//delete the input buffers
	if (ptrPlug->numInputs>0)
	{
		if (!(ptrPlug->flags & effFlagsCanMono))
		{
			//independent buffers have been assigned, delete them
			for (int i=0;i<ptrPlug->numInputs;i++)
			{
				delete[] ptrInputBuffers[i];
			}
		}
		else
		{
			//there's only one buffer, delete it
			delete[] ptrInputBuffers[0];
		}

		//remove the pointers to the buffers
		delete[] ptrInputBuffers;
	}


	//delete the output buffers
	if (ptrPlug->numOutputs>0)
	{
		for (int i=0;i<ptrPlug->numOutputs;i++)
		{
			delete[] ptrOutputBuffers[i];
		}

		//remove the pointers to the buffers
		delete[] ptrOutputBuffers;
	}


	//Shut the plugin down and free the library (this deletes the C++ class
	//memory and calls the appropriate destructors...)
	ptrPlug->dispatcher(ptrPlug,effClose,0,0,NULL,0.0f);
	FreeLibrary(libhandle);

	////////////////////////////////////////////////////////////////////////////

	printf("Done!\n");

	//Sleep(10000);
}



//host callback function
//this is called directly by the plug-in!!
//
long VSTCALLBACK host(AEffect *effect, long opcode, long index, long value, void *ptr, float opt)
{
	long retval=0;

	switch (opcode)
	{
		//VST 1.0 opcodes
		case audioMasterVersion:
			//Input values:
			//none

			//Return Value:
			//0 or 1 for old version
			//2 or higher for VST2.0 host?
			printf("plug called audioMasterVersion\n");
			retval=2;
			break;

		case audioMasterAutomate:
			//Input values:
			//<index> parameter that has changed
			//<opt> new value

			//Return value:
			//not tested, always return 0

			//NB - this is called when the plug calls
			//setParameterAutomated

			printf("plug called audioMasterAutomate\n");
			break;

		case audioMasterCurrentId:
			//Input values:
			//none

			//Return Value
			//the unique id of a plug that's currently loading
			//zero is a default value and can be safely returned if not known
			printf("plug called audioMasterCurrentId\n");
			break;

		case audioMasterIdle:
			//Input values:
			//none

			//Return Value
			//not tested, always return 0

			//NB - idle routine should also call effEditIdle for all open editors
			Sleep(1);
			printf("plug called audioMasterIdle\n");
			break;

		case audioMasterPinConnected:
			//Input values:
			//<index> pin to be checked
			//<value> 0=input pin, non-zero value=output pin

			//Return values:
			//0=true, non-zero=false
			printf("plug called audioMasterPinConnected\n");
			break;

		//VST 2.0 opcodes
		case audioMasterWantMidi:
			//Input Values:
			//<value> filter flags (which is currently ignored, no defined flags?)

			//Return Value:
			//not tested, always return 0
			printf("plug called audioMasterWantMidi\n");
			break;

		case audioMasterGetTime:
			//Input Values:
			//<value> should contain a mask indicating which fields are required
			//...from the following list?
			//kVstNanosValid
			//kVstPpqPosValid
			//kVstTempoValid
			//kVstBarsValid
			//kVstCyclePosValid
			//kVstTimeSigValid
			//kVstSmpteValid
			//kVstClockValid

			//Return Value:
			//ptr to populated const VstTimeInfo structure (or 0 if not supported)

			//NB - this structure will have to be held in memory for long enough
			//for the plug to safely make use of it
			printf("plug called audioMasterGetTime\n");
			break;

		case audioMasterProcessEvents:
			//Input Values:
			//<ptr> Pointer to a populated VstEvents structure

			//Return value:
			//0 if error
			//1 if OK
			printf("plug called audioMasterProcessEvents\n");
			break;

		case audioMasterSetTime:
			//IGNORE!
			break;

		case audioMasterTempoAt:
			//Input Values:
			//<value> sample frame location to be checked

			//Return Value:
			//tempo (in bpm * 10000)
			printf("plug called audioMasterTempoAt\n");
			break;

		case audioMasterGetNumAutomatableParameters:
			//Input Values:
			//None

			//Return Value:
			//number of automatable parameters
			//zero is a default value and can be safely returned if not known

			//NB - what exactly does this mean? can the host set a limit to the
			//number of parameters that can be automated?
			printf("plug called audioMasterGetNumAutomatableParameters\n");
			break;

		case audioMasterGetParameterQuantization:
			//Input Values:
			//None

			//Return Value:
			//integer value for +1.0 representation,
			//or 1 if full single float precision is maintained
			//in automation.

			//NB - ***possibly bugged***
			//Steinberg notes say "parameter index in <value> (-1: all, any)"
			//but in aeffectx.cpp no parameters are taken or passed
			printf("plug called audioMasterGetParameterQuantization\n");
			break;

		case audioMasterIOChanged:
			//Input Values:
			//None

			//Return Value:
			//0 if error
			//non-zero value if OK
			printf("plug called audioMasterIOChanged\n");
			break;

		case audioMasterNeedIdle:
			//Input Values:
			//None

			//Return Value:
			//0 if error
			//non-zero value if OK

			//NB plug needs idle calls (outside its editor window)
			//this means that effIdle must be dispatched to the plug
			//during host idle process and not effEditIdle calls only when its
			//editor is open
			//Check despatcher notes for any return codes from effIdle
			printf("plug called audioMasterNeedIdle\n");
			break;

		case audioMasterSizeWindow:
			//Input Values:
			//<index> width
			//<value> height

			//Return Value:
			//0 if error
			//non-zero value if OK
			printf("plug called audioMasterSizeWindow\n");
			break;

		case audioMasterGetSampleRate:
			//Input Values:
			//None

			//Return Value:
			//not tested, always return 0

			//NB - Host must despatch effSetSampleRate to the plug in response
			//to this call
			//Check despatcher notes for any return codes from effSetSampleRate
			printf("plug called audioMasterGetSampleRate\n");
			effect->dispatcher(effect,effSetSampleRate,0,0,NULL,samplerate);
			break;

		case audioMasterGetBlockSize:
			//Input Values:
			//None

			//Return Value:
			//not tested, always return 0

			//NB - Host must despatch effSetBlockSize to the plug in response
			//to this call
			//Check despatcher notes for any return codes from effSetBlockSize
			printf("plug called audioMasterGetBlockSize\n");
			effect->dispatcher(effect,effSetBlockSize,0,blocksize,NULL,0.0f);

			break;

		case audioMasterGetInputLatency:
			//Input Values:
			//None

			//Return Value:
			//input latency (in sampleframes?)
			printf("plug called audioMasterGetInputLatency\n");
			break;

		case audioMasterGetOutputLatency:
			//Input Values:
			//None

			//Return Value:
			//output latency (in sampleframes?)
			printf("plug called audioMasterGetOutputLatency\n");
			break;

		case audioMasterGetPreviousPlug:
			//Input Values:
			//None

			//Return Value:
			//pointer to AEffect structure or NULL if not known?

			//NB - ***possibly bugged***
			//Steinberg notes say "input pin in <value> (-1: first to come)"
			//but in aeffectx.cpp no parameters are taken or passed
			printf("plug called audioMasterGetPreviousPlug\n");
			break;

		case audioMasterGetNextPlug:
			//Input Values:
			//None

			//Return Value:
			//pointer to AEffect structure or NULL if not known?

			//NB - ***possibly bugged***
			//Steinberg notes say "output pin in <value> (-1: first to come)"
			//but in aeffectx.cpp no parameters are taken or passed
			printf("plug called audioMasterGetNextPlug\n");
			break;

		case audioMasterWillReplaceOrAccumulate:
			//Input Values:
			//None

			//Return Value:
			//0: not supported
			//1: replace
			//2: accumulate
			printf("plug called audioMasterWillReplaceOrAccumulate\n");
			break;

		case audioMasterGetCurrentProcessLevel:
			//Input Values:
			//None

			//Return Value:
			//0: not supported,
			//1: currently in user thread (gui)
			//2: currently in audio thread (where process is called)
			//3: currently in 'sequencer' thread (midi, timer etc)
			//4: currently offline processing and thus in user thread
			//other: not defined, but probably pre-empting user thread.
			printf("plug called audioMasterGetCurrentProcessLevel\n");
			break;

		case audioMasterGetAutomationState:
			//Input Values:
			//None

			//Return Value:
			//0: not supported
			//1: off
			//2:read
			//3:write
			//4:read/write
			printf("plug called audioMasterGetAutomationState\n");
			break;

		case audioMasterGetVendorString:
			//Input Values:
			//<ptr> string (max 64 chars) to be populated

			//Return Value:
			//0 if error
			//non-zero value if OK
			printf("plug called audioMasterGetVendorString\n");
			break;

		case audioMasterGetProductString:
			//Input Values:
			//<ptr> string (max 64 chars) to be populated

			//Return Value:
			//0 if error
			//non-zero value if OK
			printf("plug called audioMasterGetProductString\n");
			break;

		case audioMasterGetVendorVersion:
			//Input Values:
			//None

			//Return Value:
			//Vendor specific host version as integer
			printf("plug called audioMasterGetVendorVersion\n");
			break;

		case audioMasterVendorSpecific:
			//Input Values:
			//<index> lArg1
			//<value> lArg2
			//<ptr> ptrArg
			//<opt>	floatArg

			//Return Values:
			//Vendor specific response as integer
			printf("plug called audioMasterVendorSpecific\n");
			break;

		case audioMasterSetIcon:
			//IGNORE
			break;

		case audioMasterCanDo:
			//Input Values:
			//<ptr> predefined "canDo" string

			//Return Value:
			//0 = Not Supported
			//non-zero value if host supports that feature

			//NB - Possible Can Do strings are:
			//"sendVstEvents",
			//"sendVstMidiEvent",
			//"sendVstTimeInfo",
			//"receiveVstEvents",
			//"receiveVstMidiEvent",
			//"receiveVstTimeInfo",
			//"reportConnectionChanges",
			//"acceptIOChanges",
			//"sizeWindow",
			//"asyncProcessing",
			//"offline",
			//"supplyIdle",
			//"supportShell"
			printf("plug called audioMasterCanDo\n");

			if (strcmp((char*)ptr,"sendVstEvents")==0 ||
				strcmp((char*)ptr,"sendVstMidiEvent")==0 ||
				strcmp((char*)ptr,"supplyIdle")==0)
			{
				retval=1;
			}
			else
			{
				retval=0;
			}

			break;

		case audioMasterGetLanguage:
			//Input Values:
			//None

			//Return Value:
			//kVstLangEnglish
			//kVstLangGerman
			//kVstLangFrench
			//kVstLangItalian
			//kVstLangSpanish
			//kVstLangJapanese
			printf("plug called audioMasterGetLanguage\n");
			retval=1;
			break;
/*
		MAC SPECIFIC?

		case audioMasterOpenWindow:
			//Input Values:
			//<ptr> pointer to a VstWindow structure

			//Return Value:
			//0 if error
			//else platform specific ptr
			printf("plug called audioMasterOpenWindow\n");
			break;

		case audioMasterCloseWindow:
			//Input Values:
			//<ptr> pointer to a VstWindow structure

			//Return Value:
			//0 if error
			//Non-zero value if OK
			printf("plug called audioMasterCloseWindow\n");
			break;
*/
		case audioMasterGetDirectory:
			//Input Values:
			//None

			//Return Value:
			//0 if error
			//FSSpec on MAC, else char* as integer

			//NB Refers to which directory, exactly?
			printf("plug called audioMasterGetDirectory\n");
			break;

		case audioMasterUpdateDisplay:
			//Input Values:
			//None

			//Return Value:
			//Unknown
			printf("plug called audioMasterUpdateDisplay\n");
			break;
	}

	return retval;
};

