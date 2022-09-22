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

			bool smb = true;
			if (smb) {
				SmbPitchPlugin* p = new SmbPitchPlugin();
				p->process(wf, semitones);
				error = wf->write(argv[2]);
				if (error)
				  wf->printError(error);
			}
			else {
				// ST behaves differently, which I like better anyway
				SoundTouchPlugin* p = new SoundTouchPlugin();
				p->setPitch(semitones);
				Audio* result = p->processx(wf->getData(), wf->getFrames());
				result->write(argv[2]);
			}
		}
		delete wf;
	}
}


void test(int argc, const char** argv)
{
	int semitones = -5;
	if (argc > 1)
	  semitones = atoi(argv[1]);

	SoundTouchPlugin* p = new SoundTouchPlugin();
	for (int i = -12 ; i <= 12 ; i++) {
		printf("********* Rate %d\n", i);
		fflush(stdout);
		p->reset();
		p->setPitch(i);
		p->simulate();
	}
}

void shiftAll(int argc, const char** argv)
{
	if (argc < 2) {
		printf("pitch <infile>\n");
	}
	else {
		WaveFile* wf = new WaveFile();
		int error = wf->read(argv[1]);
		if (error) 
		  wf->printError(error);

		else if (wf->getFrames() > 0) {
			char filename[1024];

			for (int i = -12 ; i <= 12 ; i++) {
				printf("*** Rate %d\n", i);
				fflush(stdout);

				//SoundTouchPlugin* p = new SoundTouchPlugin();
				SmbPitchPlugin* p = new SmbPitchPlugin();
				p->setPitch(i);
				Audio* result = p->processx(wf->getData(), wf->getFrames());
				//Audio* result = p->processy(wf->getData(), wf->getFrames());
				sprintf(filename, "out%d.wav", i);
				result->write(filename);
			}
		}
		delete wf;
	}
}

void shiftAll2(int argc, const char** argv)
{
	if (argc < 2) {
		printf("pitch <infile>\n");
	}
	else {
		WaveFile* wf = new WaveFile();
		int error = wf->read(argv[1]);
		if (error) 
		  wf->printError(error);

		else if (wf->getFrames() > 0) {
			char filename[1024];

			for (int i = -12 ; i <= 12 ; i++) {
				printf("*** Rate %d\n", i);
				fflush(stdout);

				SmbPitchPlugin* p = new SmbPitchPlugin();
				p->process(wf, i);
				sprintf(filename, "out%d.wav", i);
				wf->write(filename);
			}
		}
		delete wf;
	}
}


int main(int argc, const char** argv)
{
	TracePrintLevel = 1;

	//shiftFile(argc, argv);
	// test(argc, argv);
	//shiftAll(argc, argv);
	shiftAll2(argc, argv);

	return 0;
}
