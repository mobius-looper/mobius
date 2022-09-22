#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>

#include "Resampler.h"
#include "WaveFile.h"

/****************************************************************************
 *                                                                          *
 *   					   MOBIUS STREAM SIMULATOR                          *
 *                                                                          *
 ****************************************************************************/

bool NewWay = true;

bool Trace = false;
float Rate;
float InverseRate;
float InputThreshold;
float OutputThreshold;
int InputRemainder;
int OutputRemainder;
int SystemInputLatency;
int SystemOutputLatency;
int InputLatency;
int OutputLatency;
int RecordFrame;
int PlayFrame;
int Block = 0;
int TraceBlock = 0;

int blocks[] = {
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	33,
	0
};

int events[] = {
	0,
};


/****************************************************************************
 *                                                                          *
 *   						   ORIGINAL DESIGN                              *
 *                                                                          *
 ****************************************************************************/

float pretendDecimation(float rate, float threshold, 
						long srcFrames, long destFrames,
						long* advance)
{
	long srcRemaining = srcFrames;
	long destRemaining = destFrames;
	bool overflow = false;
	int counter = 0;
	int dcounter = 0;
	float initialThreshold = threshold;
	float increment = (float)(1.0f - rate);

	if (Trace)
	  printf("Decimation: threshold %f srcFrames %ld destFrames %ld\n", 
			 threshold, srcFrames, destFrames);

	while (srcRemaining > 0 && !overflow) {

		// corresponding source frame
		int rev = (int)((dcounter / rate) + initialThreshold);

		// when counting up from 0 to 1 have to preincrement
		threshold += increment;

		if (threshold < 1.0) {
			// take this one
			if (Trace)
			  printf("%d: %f copy src %d to %d\n",
					 counter, threshold, rev, dcounter);
			dcounter++;
			if (destRemaining == 0) {
				// This isn't supposed to happen with decimation though
				// I suppose we could spill over into the interpolation
				// remainder buffer.
				printf("Decimation overflow!\n");
				overflow = true;
			}
			else {
				destRemaining--;
			}
		}
		else {
			if (Trace)
			  printf("%d: %f skip\n", counter, threshold);
			threshold -= 1.0;
		}

		counter++;
		srcRemaining--;
	}

	if (destRemaining > 0) {
		// this happens occasionally due to float rounding, it just reduces
		// the size of the recorded block

		printf("Decimation underflow\n");
		printf("  initial threshold %f srcFrames %ld destFrames %ld remaining %ld\n", 
			   initialThreshold, srcFrames, destFrames, destRemaining);
		
	}

	*advance = dcounter;
	if (Trace)
	  printf("Decimation: Advance %d ending threshold %f\n", 
			 *advance, threshold);

	return threshold;
}

float pretendInterpolation(float rate, float threshold, 
						   long srcFrames, long destFrames,
						   int *retRemainder,
						   int *retUnderflows)
{
	float initialThreshold = threshold;
	long srcRemaining = srcFrames;
	long destRemaining = destFrames;
	bool overflow = false;
	int underflows = 0;
	int remainder = 0;
	int counter = 0;
	//float increment = (float)(1.0f - rate);
	float increment = rate;

	if (Trace)
	  printf("Interpolation: threshold %f srcFrames %ld destFrames %ld\n", 
			 threshold, srcFrames, destFrames);

	while (srcRemaining > 0) {

		// copy a source frame to the destination
		if (destRemaining == 0) {
			if (overflow) {
				// we filled the remainder too!
				printf("Interpolation remainder overflow!\n");
				break;
			}
			else {
				if (Trace)
				  printf("Add remainder\n");
				overflow = true;
				// this should never go beyond the inverse of the rate
				// minus 1, hmm getting rouding errors on .25 1/rate
				// results in 3.99999 so always allow one more
				//destRemaining = (int)(1.0 / rate) - 1;
				destRemaining = (int)ceil((1.0 / rate) - 1.0);
			}
		}

		threshold += increment;
		if (threshold >= 1.0) {
			// advance
			if (Trace)
			  printf("%d: %f copy src %d to %d and advance\n", 
					 counter, threshold, srcFrames - srcRemaining, counter);
			srcRemaining--;
			threshold -= 1.0;
		}
		else {
			// stay on this one
			if (Trace)
			  printf("%d: %f copy src %d to %d and stay\n", 
					 counter, threshold, srcFrames - srcRemaining, counter);
		}

		counter++;
		destRemaining--;
		if (overflow)
		  remainder++;
	}

	if (destRemaining > 0 && !overflow) {
		// the output buffer was too large, this happens occasionally due
		// to float rouding errors
		if (destRemaining > 1)
		  printf("Interpolation underflow of %d!\n", destRemaining);
		else
		  printf("Interpolation underflow\n");

		printf("  initial threshold %f srcFrames %ld destFrames %ld remaining %ld\n", 
			   initialThreshold, srcFrames, destFrames, destRemaining);
		
		// for the output stream we have to "play" one more
		underflows += destRemaining;
	}

	if (Trace)
	  printf("Interpolation: remainder %d ending threshold %f\n", 
			 remainder, threshold);

	*retRemainder = remainder;
	*retUnderflows = underflows;

	return threshold;
}

long scaleInputFrame1(float rate, float threshold, long frame)
{
	long scaled = frame;
	float irate = (float)(1.0f / rate);

	if (irate > 1.0) {
		// decimation, threshold counts up to one
		float increment = (float)(1.0f - rate);
		float endthresh = (increment * frame) + threshold;
		int skips = (int)endthresh;
		scaled = frame - skips;

		// kludge: there are occasional rounding errors, 
		// threshold=.810972 with rate .297302 conceptually
		// results in a value of 24.000006 which Intel rounds to 24.0000
		// then truncates to int 23.  Try to detect this with a reverse
		// calculation.
	}
	else {
		// interpolation, round up
		scaled = (long) ceil(frame * rate);
	}
	return scaled;
}

/**
 * Given a number of interrupt buffer frames, calculate the minimum
 * number of frames we need to extract from the loop and interpolate in order
 * to reach the number of buffer frames.  
 *
 * The interpolation algorithm is assumed to increment the threshold
 * by the rate for each output frame, when this crosses 1, the next
 * frame from the loop is taken, while it is below 1, the current frame
 * from the loop is duplicated.
 */
long getMinimumInterpolationFrames1(float rate, float threshold, long srcFrames)
{
	// subtle: there is a boundary condition I don't fully understand 
	// but the logic here is that you must always have 1 frame, then
	// do the boundary crossings to obtain 1 less than the desired number
	// of frames.
	float endthresh = 1 + ((srcFrames - 1) * rate) + threshold;

	// so this represents the number of frames "taken" from the loop
	long scaled = (int)endthresh;
	
	return scaled;
}

long scaleOutputFrame1(float rate, float threshold, long frame)
{
	long scaled = frame;
	// float irate = (float)(1.0f / rate);

	if (rate > 1.0) {
	}
	else {
		// interpolation
		scaled = getMinimumInterpolationFrames1(rate, threshold, frame);
	}
	return scaled;
}

void record1(long frames)
{
	if (Rate > 1.0) {
		// speeding up play, slowing down record
	}
	else {
		// slowing down play, speeding up record
		long scaled = scaleInputFrame1(Rate, InputThreshold, frames);
		if (Trace)
		  printf("Scaled %ld input frames to %ld, %d skips\n", 
				 frames, scaled, frames - scaled);

		long advance = 0;
		InputThreshold = pretendDecimation(Rate, InputThreshold, 
										   frames, scaled, &advance);
		RecordFrame += advance;
	}
}

void play1(long frames)
{
	if (Rate > 1.0) {
		// speeding up
	}
	else {
		// slowing down

		// first consume the remainder from the previous block
		if (OutputRemainder > 0) {
			if (Trace)
			  printf("Applying remainder %d from previous block\n", 
					 OutputRemainder);
			frames -= OutputRemainder;
			OutputRemainder = 0;
		}

		long scaled = scaleOutputFrame1(Rate, OutputThreshold, frames);
		int underflows = 0;
		if (Trace)
		  printf("Scaled %ld output frames to %ld, %d insertions\n", 
				 frames, scaled, frames - scaled);
		OutputThreshold = pretendInterpolation(Rate, OutputThreshold, 
											   scaled, frames,
											   &OutputRemainder,
											   &underflows);
		PlayFrame += (scaled + underflows);
	}
}

/****************************************************************************
 *                                                                          *
 *   							  NEW DESIGN                                *
 *                                                                          *
 ****************************************************************************/

float stTranspose(float rate, float threshold, 
				  long srcFrames, long destFrames,
				  long* advance, int* retRemainder)
{
	int remainder = 0;
	long srcFrame = 0;
	long destFrame = 0;
	long lastFrame = srcFrames - 1;

	if (Trace) {	
		if (rate < 1.0) 
		  printf("Decimation: threshold %f srcFrames %ld destFrames %ld\n", 
				 threshold, srcFrames, destFrames);
		else
		  printf("Interpolation: threshold %f srcFrames %ld destFrames %ld\n", 
				 threshold, srcFrames, destFrames);
	}

    // combine last frame from previous block with first frame of this block
	while (threshold <= 1.0f) {
		if (Trace)
		  printf("%d: %f of last plus %f of %d\n",
				 destFrame, 1.0 - threshold, threshold, srcFrame);
		destFrame++;
		threshold += rate;
	}
	threshold -= 1.0f;

    // may have an initial skip if decimating
    while (threshold > 1.0f && srcFrame < srcFrames) {
		if (Trace)
		  printf("%d: Skip %d %f\n", destFrame, srcFrame, threshold);
        threshold -= 1.0f;
		srcFrame++;
    }

	// note that since we're always combining two frames, we don't actually
	// advance to the last input frame, keep it for the next call
    while (srcFrame < lastFrame) {

		if (destFrame < destFrames) {
			if (Trace)
			  printf("%d: %f of %d plus %f of %d\n", 
					 destFrame, 1.0 - threshold, srcFrame,
					 threshold, srcFrame + 1);

			destFrame++;
		}
		else {
			if (Trace)
			  printf("  %f of %d plus %f of %d to remainder\n", 
					 1.0 - threshold, srcFrame,
					 threshold, srcFrame + 1);
			remainder++;
		}

		threshold += rate;

        // once we increment beyond 1, advance to the next source frame
		int count = 0;
        while (threshold > 1.0f && srcFrame < lastFrame) {
			if (count > 0 && Trace)
			  printf("%d: skip %d %f\n", destFrame, srcFrame, threshold);
            threshold -= 1.0f;
			srcFrame++;
			count++;
        }
    }

	// we may not have advanced to last frame, just used it
	// in the final interpolation
	if (srcFrame < lastFrame - 1) {
		printf("Transposition source underflow!\n");
	}
	else if (srcFrame > lastFrame) {
		printf("Transposition source overflow!\n");
	}

	if (destFrame != destFrames) {
		// too many frames in the destination buffer
		printf("Transposition output underflow!\n");
	}

	if (advance != NULL)
	  *advance = destFrame;

	if (retRemainder != NULL)
	  *retRemainder = remainder;
    
	if (Trace) {
		if (rate < 1.0) 
		  printf("Decimation: Advance %d ending threshold %f\n", 
				 destFrame, threshold);
		else
		  printf("Interpolation: ending threshold %f\n", 
				 threshold);
	}

	return threshold;
}

/**
 * Given a number of input frames, calculate the resulting number
 * of frames after rate adjustment.  Rate here must be the inverse
 * of the playback rate.
 */
long stScaleInputFrames(float rate, float threshold, long srcFrames)
{
	long destFrames = 0;
	long srcFrame = 0;
	long lastFrame = srcFrames - 1;

    // combine last frame from previous block with first frame of this block
	while (threshold <= 1.0f) {
		destFrames++;
		threshold += rate;
	}
	threshold -= 1.0f;

    // may have an initial skip
    while (threshold > 1.0f && srcFrame < srcFrames) {
        threshold -= 1.0f;
		srcFrame++;
    }

    while (srcFrame < lastFrame) {
		destFrames++;
        threshold += rate;
        while (threshold > 1.0f && srcFrame < lastFrame) {
            threshold -= 1.0f;
			srcFrame++;
        }
    }

	return destFrames;
}

/**
 * Given a number of output frames, determine how many frames we need
 * to read from the loop and scale to achieve that number.
 * The rate here must be the playback rate.
 */
long stScaleOutputFrames(float rate, float threshold, long destFrames)
{
	long srcFrames = 1;  // always need at least one
	long destFrame = 0;

    // combine last frame from previous block with first frame of this block
	while (threshold <= 1.0f && destFrame < destFrames) {
		destFrame++;
		threshold += rate;
	}
	threshold -= 1.0f;

    // may have an initial skip
    while (threshold > 1.0f) {
        threshold -= 1.0f;
		srcFrames++;
    }

	// from this point on we're combining the current source
	// frame with the next so need an extra
	if (destFrame < destFrames)
	  srcFrames++;

    while (destFrame < destFrames) {
		destFrame++;
        threshold += rate;
		if (destFrame < destFrames) {
			while (threshold > 1.0f) {
				threshold -= 1.0f;
				srcFrames++;
			}
		}
	}

	return srcFrames;
}

void record2(long frames)
{
	// slowing down play, speeding up record
	// first factor out the remainder
	if (InputRemainder > 0) {
		// unlike a play remainder, this *does* advance the RecordFrame
		if (Trace)
		  printf("Applying input remainder %d\n", InputRemainder);
		if (InputRemainder <= frames) {
			frames -= InputRemainder;
			RecordFrame += InputRemainder;
		}
		else {
			RecordFrame += frames;
			InputRemainder = InputRemainder - frames;
			frames = 0;
		}
	}

	if (frames > 0) {
		long scaled = stScaleInputFrames(InverseRate, InputThreshold, frames);
		if (Trace)
		  printf("Scaled %ld input frames to %ld\n", frames, scaled);

		long advance = 0;

		InputThreshold = stTranspose(InverseRate, InputThreshold,
									 frames, scaled, &advance,
									 &InputRemainder);
		RecordFrame += advance;
	}
}

void play2(long block, long frames)
{
	// slowing down
	if (OutputRemainder > 0) {
		// this does not advance the PlayFrame, it is interpolation
		// residue from the last extracted frame
		if (Trace)
		  printf("Applying output remainder %d\n", OutputRemainder);
		if (OutputRemainder <= frames) {
			frames -= OutputRemainder;
		}
		else {
			OutputRemainder = OutputRemainder - frames;
			frames = 0;
		}
	}

	if (frames > 0) {
		long scaled = stScaleOutputFrames(Rate, OutputThreshold, frames);
		int adjust = 0;

		if (Trace)
		  printf("Scaled %ld output frames to %ld\n", frames, scaled);

		// in rare cases we can begin to slowly go out of sync
		// at some rates, probably due to floating point rouding error
		// 3 seems to be the average constant rate, one extra for lookahead
		// on each side, and one for periodic drift corrected quickly

		long expected = RecordFrame + InputLatency + OutputLatency;
		long actual = PlayFrame + scaled;
		if (expected > actual) {
			int delta = expected - actual;
			if (delta > 2) {
				// play frame is lagging, read one extra and ignore it
				adjust = 1;
			}
		}
		else if (expected < actual) {
			int delta = actual - expected;
			if (delta > 2) {
				// play frame is rushing, read one less and duplicate the 
				// last one
				adjust = -1;
			}
		}

		//if (Trace) {
			if (adjust != 0) {
				if (adjust > 0) 
				  printf("Adjustment for lagging play frame in block %ld\n", 
						 block);
				else
				  printf("Adjustment for rushing play frame in block %ld\n",
						 block);
			}
			//}

		if (adjust < 0) {
			// reduce the frame count for transpoisition, and dup the last one
			scaled--;
			frames--;
		}

		OutputThreshold = stTranspose(Rate, OutputThreshold, 
									  scaled, frames, 
									  NULL, &OutputRemainder);

		PlayFrame += scaled;
		if (adjust > 0) {
			// we read one extra but didn't include it in the transposition
			PlayFrame++;
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   						   MOBIUS SIMULATOR                             *
 *                                                                          *
 ****************************************************************************/

void setRate(float rate)
{
	Rate = rate;
	InverseRate = (float)(1.0f / Rate);
	InputLatency = (int)ceil(SystemInputLatency * Rate);
	OutputLatency = (int)ceil(SystemInputLatency * Rate);

	PlayFrame = RecordFrame + InputLatency + OutputLatency;
}

void setRate(int degree)
{
	setRate(Resampler::getScaleRate(degree));
}

/**
 * Initialize the state of the engine.
 */
void initEngine()
{
	float rate;

	// one semitone down
	//rate = .943874f;
	//rate = .3;
	//rate = .5f;
	//rate = .25f;
	//rate = .75;
	// 3 semitones down
	rate = 0.840897f;

	RecordFrame = 0;
	PlayFrame = 0;
	SystemInputLatency = 256;
	SystemOutputLatency = 256;
	if (NewWay) {
		InputThreshold = 1.0f;
		OutputThreshold = 1.0f;
	}
	else {
		InputThreshold = 0.0f;
		// if you start this at 1.0, we begin advancing immediately, at 0.0
		// we begin holding immediately
		OutputThreshold = 0.0f;
	}
	InputRemainder = 0;
	OutputRemainder = 0;

	setRate(rate);
}

void interrupt(long block, long frames)
{
	if (NewWay) {
		record2(frames);
		play2(block, frames);
	}
	else {
		record1(frames);
		play1(frames);
	}
}

/**
 * Simpler fixed rate simulator.
 */
void mobiusSimulator1()
{
	bool dealign = false;
	int nBlocks = 10;
	long blockFrames = 33;

	initEngine();
	Trace = true;

	//setRate(-21);
	//InputThreshold = 0.810972f;
	//setRate(-15);
	//OutputThreshold = 0.206045f;
	setRate(-12);

	//for (int i = 0 ; blocks[i] > 0 ; i++) {
	//long frames = blocks[i];

	printf("Begin simulator rate %f ithreshold %f othreshold %f \n", 
		   Rate, InputThreshold, OutputThreshold);

	for (int i = 0 ; i < nBlocks ; i++) {
		long frames = blockFrames;

		Block = i;

		printf("Block %d *************************************************\n",
			   i);

		interrupt(i, frames);

		printf("End of interrupt: record frame %ld play frame %ld\n",
			   RecordFrame, PlayFrame);

		long pf = RecordFrame + InputLatency + OutputLatency;
		if (pf != PlayFrame) {
			long delta = PlayFrame - pf;
			if (delta < 0) delta = -delta;
			if (delta > 1)
			  printf("Frame cursors out of sync by %d!\n", delta);
			else
			  printf("Cursors dealigned by 1\n");
			dealign = true;
		}
		else if (dealign) {
			printf("Cursors dealigned corrected\n");
			dealign = false;
		}

		//printf("****************************************************************\n");
	}
}

/**
 * Iterate over rates in a two octave range up and down.
 */
void mobiusSimulator2()
{
	int dealign = 0;
	int blocks = 100000;
	long frames = 256;
	float rates[128];
	int rateRange = 48;
	int rateCenter = 24;
	int i;
	//int badblock = 38960;
	int badblock = 0;

	initEngine();

	// initialize rates
	for (i = 0 ; i <= rateRange ; i++)
	  rates[i] = Resampler::getScaleRate(i - rateCenter);

	// for each rate
	//int kludge = rateCenter + 9;
	//rateRange = kludge;
	//for (i = kludge ; i <= rateRange ; i++) {

	for (i = 0 ; i <= rateRange ; i++) {
		setRate(rates[i]);
		bool syncwarn = false;

		printf("Rate %d %f\n", i - rateCenter, Rate);
		fflush(stdout);

		for (int b = 0 ; b < blocks ; b++) {

			if (badblock > 0 && b == badblock) {
				Trace = true;
				printf("Block %d *************************************************\n",
					   b);
				printf("RecordFrame=%ld, PlayFrame=%ld, InputLatency=%d, OutputLatency=%d Expected=%ld\n",
					   RecordFrame, PlayFrame, 
					   InputLatency, OutputLatency,
					   RecordFrame + InputLatency + OutputLatency);
			}

			interrupt(b, frames);

			long pf = RecordFrame + InputLatency + OutputLatency;
			if (pf != PlayFrame) {
				long delta = PlayFrame - pf;
				if (delta < 0) delta = -delta;
				// there is almost always 1 dealgin due to the lookahead,
				// and another due to float roudnign that is usually corrected
				// in the next block
				// hmm, seems to be a +1 potential error for lookahead in 
				// both streams for a max of 3?
				// run with positive rate 9 to see this
				if (delta < 3) {
					if (Trace)
					  printf("Cursors dealigned by %d\n", delta);
				}
				else {
					if (!syncwarn) {
						if (dealign != delta)
						  printf("Frame cursors out of sync by %d after block %d!\n", 
								 delta, b);
						syncwarn = true;
					}
				}
				dealign = delta;
			}
			else if (dealign > 0) {
 				if (Trace && dealign > 1)
				  printf("Cursors dealigned corrected\n");
				dealign = 0;
			}

			if (b == badblock + 3)
			  Trace = false;
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   						  SAMPLE GENERATION                             *
 *                                                                          *
 ****************************************************************************/

/**
 * The frequency factor between two semitones.  This^12 = 2 for one octave.
 */
#define SEMITONE_FACTOR 1.059463f

void makeSines()
{
    Resampler* rs = new Resampler();
    long samples;

    printf("Generating sine wave\n");
    fflush(stdout);

    float* sine = rs->generateSine(2, &samples);
    long frames = samples / 2;
    long bufferSamples = samples * 2;
    float* buffer = new float[bufferSamples];

    printf("Buffer frames %ld\n", frames * 2);
    memset(buffer, 0, bufferSamples * sizeof(float));

    WaveFile* wf = new WaveFile();
    wf->setData(sine);
    wf->setFrames(frames);
    wf->write("sine.wav");
    wf->stealData();    // don't let sine be deleted
    wf->setData(buffer);

    printf("Interpolating by 2\n");
    fflush(stdout);
    rs->interpolate2x(sine, frames, buffer);
    wf->setFrames(frames * 2);
    wf->write("sine2x.wav");
    memset(buffer, 0, bufferSamples * sizeof(float));

    printf("Decimating by 2\n");
    fflush(stdout);
    rs->decimate2x(sine, frames, buffer);
    wf->setFrames(frames / 2);
    wf->write("sinehalf.wav");
    memset(buffer, 0, bufferSamples * sizeof(float));

    // use the SoundTouch algorithm up and down
    char filename[128];

    for (int i = 1 ; i <= 12 ; i++) {
        
        // ST rates above 1.0 speed it up, e.g. 2.0 = 2x
        double rate = pow(SEMITONE_FACTOR, i);

        // but have to invert it to get frame calculation multiplier
        double frameRate = 1.0f / rate;

        long newFrames = (long)(frames * frameRate);
        printf("Transposing up %d, rate %lf, frames %ld\n", i, rate, newFrames);
        fflush(stdout);
        rs->transposeOnce(sine, buffer, frames, (float)rate);
        wf->setFrames(newFrames);
        sprintf(filename, "sine-up-%d.wav", i);
        wf->write(filename);
        memset(buffer, 0, bufferSamples * sizeof(float));

        // invert to slow down
        float f = (float)frameRate;
        frameRate = rate;
        rate = f;
        newFrames = (long)(frames * frameRate);
        printf("Transposing down %d, rate %lf, frames %ld\n", i, rate, newFrames);
        fflush(stdout);
        rs->transposeOnce(sine, buffer, frames, (float)rate);
        wf->setFrames(newFrames);
        sprintf(filename, "sine-down-%d.wav", i);
        wf->write(filename);
        memset(buffer, 0, bufferSamples * sizeof(float));
        
    }

}

void testResampler()
{
	Resampler* rs = new Resampler();
    long samples;
	long adjustedFrames;

    float* sine = rs->generateSine(2, &samples);
    long frames = samples / 2;
    float* buffer = new float[samples * 2];

    WaveFile* wf = new WaveFile();
    wf->setData(sine);
    wf->setFrames(frames);
    wf->write("sine.wav");

	// 2x interpolation
	printf("2x interpolation\n");
	fflush(stdout);
	adjustedFrames = frames * 2;
	rs->setRate(0.5);
	rs->resample(sine, frames, buffer, adjustedFrames);
	
    wf->stealData();    // don't let sine be deleted
    wf->setData(buffer);
    wf->setFrames(adjustedFrames);
    wf->write("sinehalf.wav");

	// 2x decimation
	printf("2x decimation\n");
	fflush(stdout);
	adjustedFrames = frames / 2;
	rs->setRate(2.0);
	rs->resample(sine, frames, buffer, adjustedFrames);

	// it still has buffer
    wf->setFrames(adjustedFrames);
    wf->write("sinedouble.wav");

	// up a 5th
	printf("up 5\n");
	fflush(stdout);
	rs->setScaleRate(7);
	adjustedFrames = (long)(frames / rs->getRate());
	rs->resample(sine, frames, buffer, adjustedFrames);
    wf->setFrames(adjustedFrames);
    wf->write("sineup5.wav");

	// down a 5th
	printf("down 5\n");
	fflush(stdout);
	rs->setScaleRate(-7);
	adjustedFrames = (long)(frames / rs->getRate());
	rs->resample(sine, frames, buffer, adjustedFrames);
    wf->setFrames(adjustedFrames);
    wf->write("sinedown5.wav");

	printf("done\n");
	fflush(stdout);
}

/****************************************************************************
 *                                                                          *
 *   								 MAIN                                   *
 *                                                                          *
 ****************************************************************************/

void quicktest()
{
	float threshold;
	float rate = 0.5f;
	long advance = 0;

	//threshold = stTranspose(rate, 0.0f, 64, 64, &advance);
	threshold = stTranspose(rate, 0.5f, 64, 64, &advance, NULL);
}

void main(int argc, char* argv[]) 
{
	//makeSines();
	//testResampler();
	//quicktest();
	
	//mobiusSimulator1();
	mobiusSimulator2();
}
