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
				bool blocking = true;
				if (!blocking) {
					p->process(wf, semitones);
					error = wf->write(argv[2]);
					if (error)
					  wf->printError(error);
				}
				else {
					p->setPitchSemitones(semitones);
					Audio* result = p->processx(wf->getData(), wf->getFrames());
					result->write(argv[2]);
				}
			}
			else {
				// ST behaves differently, which I like better anyway
				SoundTouchPlugin* p = new SoundTouchPlugin();
				p->setPitchSemitones(semitones);
				Audio* result = p->processx(wf->getData(), wf->getFrames());
				result->write(argv[2]);
			}
		}
		delete wf;
	}
}


int main(int argc, const char** argv)
{
	TracePrintLevel = 1;
	
	shiftFile(argc, argv);

	return 0;
}
