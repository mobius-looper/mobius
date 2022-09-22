//
// Main for pitch shifting experiments.
// 

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "util.h"
#include "Trace.h"
#include "WaveFile.h"
#include "Audio.h"
#include "Plugin.h"

#define PLUGIN_SMB 1
#define PLUGIN_TOUCH 2
#define PLUGIN_DIRAC 3

void shiftFile(WaveFile* wf, int semitones, const char* basename)
{
	char filename[1024];
	int plugtype = PLUGIN_TOUCH;
	bool blocking = true;
	int error;

	sprintf(filename, "%s%d.wav", basename, semitones);

	switch (plugtype) {

		case PLUGIN_SMB: {
			SmbPitchPlugin* p = new SmbPitchPlugin();
			if (!blocking) {
				p->process(wf, semitones);
				error = wf->write(filename);
				if (error)
				  wf->printError(error);
			}
			else {
				p->setPitchSemitones(semitones);
				Audio* result = p->processToAudio(wf->getData(), 
												  wf->getFrames());
				result->write(filename);
			}
		}
		break;
				
		case PLUGIN_TOUCH: {
			// ST behaves differently, which I like better anyway
			SoundTouchPlugin* p = new SoundTouchPlugin();
			p->setPitchSemitones(semitones);
			Audio* result = p->processToAudio(wf->getData(), 	
											  wf->getFrames());
			result->write(filename);
			p->debug();
		}
		break;

		case PLUGIN_DIRAC: {
#ifdef ENABLE_DIRAC
			DiracPlugin* p = new DiracPlugin();
			if (!blocking) {
				Audio* a = p->process(wf, semitones);
				if (a != NULL) {
					a->write(filename);
					delete a;
				}
			}
			else {
				p->setPitchSemitones(semitones);
				Audio* result = p->processToAudio(wf->getData(), 	
												  wf->getFrames());
				result->write(filename);
			}
#endif
		}
		break;
	}
}

void shiftFile(int argc, const char** argv)
{
	if (argc < 3) {
		printf("pitch <infile> <outfile> <semitones>\n");
	}
	else {
		WaveFile* wf = new WaveFile();
		int error = wf->read(argv[1]);
		if (error) 
		  wf->printError(error);

		else if (wf->getFrames() > 0) {
			int semitones = -5;
			if (argc > 3)
			  semitones = atoi(argv[3]);

			shiftFile(wf, semitones, "pitch");
		}
		delete wf;
	}
}

void shiftSweep(int argc, const char** argv)
{
	if (argc < 3) {
		printf("pitch <infile> <outfile> <semitones>\n");
	}
	else {
		WaveFile* wf = new WaveFile();
		int error = wf->read(argv[1]);
		if (error) 
		  wf->printError(error);

		else if (wf->getFrames() > 0) {

			for (int i = -12 ; i <= 12 ; i++) {
				printf("Shift %d\n", i);
				fflush(stdout);

				shiftFile(wf, i, "pitch");
			}
		}
		delete wf;
	}
}

int main(int argc, const char** argv)
{
	TracePrintLevel = 1;
	
	shiftFile(argc, argv);
	//shiftSweep(argc, argv);

	return 0;
}
