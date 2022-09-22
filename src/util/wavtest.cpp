/*
 * Tests for the WaveFile utility.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "WaveFile.h"

int main(int argc, char *argv[])
{
	int status = 0;
    if (argc < 3) {
		printf("usage: autest <infile> <outfile>\n");
		status = 1;
    }
 	else {
		WaveFile* wf = new WaveFile();
		wf->setDebug(true);
		int error = wf->read(argv[1]);
		if (error) 
		  wf->printError(error);
		else {
            printf("Read file %s\n", argv[1]);
            fflush(stdout);
            int error = wf->write(argv[2]);
            if (error)
              wf->printError(error);
			else
			  printf("Wrote file %s\n", argv[2]);
        }
	}

	return status;
}

/**
 * This is some old test code that was over in the mobius 
 * directory for awhile.  It looks like it does some byte
 * order checking and verifies round-trip without loss.
 */
void oldmain(int argc, char** argv)
{
	if (argc > 1 && !strcmp(argv[1], "test")) {

		unsigned char n1[4] = {0x94, 0x01, 0x4A, 0x3B};
		unsigned char n2[4] = {0x00, 0x80, 0x89, 0x3C};
		unsigned char n3[4] = {0x90, 0x01, 0x4A, 0x3B};

		float f1 = *((float*)n1);
		float f2 = *((float*)n2);
		float f3 = *((float*)n3);
		float sum = f1 + f2;
		float diff = sum - f2;

		printf("%f equals %f ? \n", f1, f3);

		printf("%f plus %f equals %f minus %f equals %f\n",
			   f1, f2, sum, f2, diff);

		unsigned char* sbytes = (unsigned char*)&sum;
		printf("Sum bytes %x %x %x %x\n", 
			   sbytes[0], sbytes[1], sbytes[2], sbytes[3]);

		unsigned char* bytes = (unsigned char*)&diff;
		printf("Result bytes %x %x %x %x\n", 
			   bytes[0], bytes[1], bytes[2], bytes[3]);
	}
	else if (argc < 3) {
		printf("audiff <file1> <file2>\n");
		printf("audiff -reformat <file> [<file2>]\n");
	}
	else if (!strcmp(argv[1], "-reformat")) {
		const char* srcfile = argv[2];
		const char* destfile = srcfile;
		if (argc == 3)
		  printf("Reformatting %s\n", srcfile);
		else {
			destfile = argv[3];
			printf("Reformatting %s to %s\n", srcfile, destfile);
		}
		WaveFile* wav = new WaveFile();
		int error = wav->read(srcfile);
		if (error)
		  wav->printError(error);
		else
		  wav->write(destfile);
		delete wav;
	}
	else {
		WaveFile* wav1 = new WaveFile();
		WaveFile* wav2 = new WaveFile();

		int error = wav1->read(argv[1]);
		if (error) {
			printf("Error reading %s:\n", argv[1]);
			wav1->printError(error);
		}
		else {
			error = wav2->read(argv[2]);
			if (error) {
				printf("Error reading %s:\n", argv[2]);
				wav2->printError(error);
			}
			else if (wav1->getFrames() != wav2->getFrames()) {
				printf("Files differ in number of frames: %ld, %ld\n", 
					   wav1->getFrames(), wav2->getFrames());
			}
			else if (wav1->getChannels() != wav2->getChannels()) {
				printf("Files differ in number of channels: %d, %d\n", 
					   wav1->getChannels(), wav2->getChannels());
			}
			else if (wav1->getChannels() != 2) {
				// could be smarter but don't need to be
				printf("Unable to diff files with other than 2 channels\n");
			}
			else {
				float* data1 = wav1->getData();
				float* data2 = wav2->getData();
				long samples = wav1->getFrames() * 2;

				for (int i = 0 ; i < samples ; i++) {
					if (data1[i] != data2[i]) {
						printf("Files differ at frame %d\n", i / 2);
						break;
					}
				}
			}
		}

		delete wav1;
		delete wav2;
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
