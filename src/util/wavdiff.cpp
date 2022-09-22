/*
 * Wave file difference utility.
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
		printf("usage: wavdiff <file1> <file2>\n");
		status = 1;
    }
 	else {
		WaveFile* wf = new WaveFile();
		int error = wf->read(argv[1]);
		if (error) {
            printf("Error reading %s\n", argv[1]);
            wf->printError(error);
        }
		else {
            WaveFile* wf2 = new WaveFile();
            error = wf2->read(argv[2]);
            if (error) {
                printf("Error reading %s\n", argv[2]);
                wf->printError(error);
            }
            else {
                if (wf->getFrames() != wf2->getFrames()) 
                  printf("Files differ in size: %ld %ld\n", 
                         wf->getFrames(), wf2->getFrames());

                float* data1 = wf->getData();
                float *data2 = wf2->getData();

                int max = wf->getFrames();
                if (wf2->getFrames() < max)
                  max = wf2->getFrames();

                for (long i = 0 ; i < max ; i += 2) {

                    if (data1[i] != data2[i] ||
                        data1[i+1] != data2[i+1]) {
                        printf("Files differ at frame %ld\n", i / 2);
                        break;
                    }
                }
            }
        }
    }

	return status;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
