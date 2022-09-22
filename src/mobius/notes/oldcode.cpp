/****************************************************************************
 *                                                                          *
 *   						   RATE EXPERIMENTS                             *
 *                                                                          *
 ****************************************************************************/

/**
 * First implementation, assumes initial threshold of 1.0
 */
float pretendDecimation1(float rate, float threshold, 
						long srcFrames, long destFrames)
{
	long srcRemaining = srcFrames;
	long destRemaining = destFrames;
	bool overflow = false;
	int counter = 0;

	while (srcRemaining > 0 && !overflow) {

		if (threshold >= 1.0) {
			printf("%d: %f take\n", counter, threshold);
			// take this one
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
			threshold -= 1.0;
		}
		else
		  printf("%d: %f skip\n", counter, threshold);

		threshold += rate;
		counter++;
		srcRemaining--;
	}

	if (destRemaining > 0)
	  printf("Decimation underflow!\n");

	return threshold;
}

float pretendDecimation2(float rate, float threshold, 
						 long srcFrames, long destFrames,
						 long* advance)
{
	long srcRemaining = srcFrames;
	long destRemaining = destFrames;
	bool overflow = false;
	int counter = 0;
	int dcounter = 0;
	float initialThreshold = threshold;
	float increment = (float)(1.0 - rate);

	printf("Decimation: threshold %f srcFrames %ld destFrames %ld\n", 
		   threshold, srcFrames, destFrames);

	while (srcRemaining > 0 && !overflow) {

		if (threshold < 1.0) {
			int rev = (int)((dcounter / rate) + initialThreshold);
			printf("%d: %f take %d src %d\n", counter, threshold, dcounter,
				   rev);
			dcounter++;
			// take this one
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
			printf("%d: %f skip\n", counter, threshold);
			threshold -= 1.0;
		}

		threshold += increment;
		counter++;
		srcRemaining--;
	}

	if (destRemaining > 0)
	  printf("Decimation underflow!\n");

	*advance = dcounter;
	printf("Decimation: Advance %d ending threshold %f\n", 
		   *advance, threshold);

	return threshold;
}

float pretendInterpolation1(float rate, float threshold, 
							long srcFrames, long destFrames,
							int *retRemainder)
{
	long srcRemaining = srcFrames;
	long destRemaining = destFrames;
	bool overflow = false;
	int remainder = 0;
	int counter = 0;
	float increment = (float)(1.0 - rate);

	printf("Interpolation: threshold %f srcFrames %ld destFrames %ld\n", 
		   threshold, srcFrames, destFrames);

	while (srcRemaining > 0) {
		if (destRemaining == 0) {
			if (overflow) {
				// we filled the remainder too!
				printf("Interpolation remainder overflow!\n");
				break;
			}
			else {
				printf("Add remainder\n");
				overflow = true;
				remainder++;
				// if this goes over 1 there it will gradulally increase?
				destRemaining = 1;
			}
		}

		destRemaining--;

		threshold -= rate;
		if (threshold <= 0.0) {
			threshold += 1.0;
			printf("%d: %f move\n", counter, threshold);
			srcRemaining--;
		}
		else {
			printf("%d: %f stay\n", counter, threshold);
		}
		counter++;
	}

	if (destRemaining > 0 && !overflow) {
		// the output buffer was too large, miscalculation somewhere!
		printf("Interpolation underflow!\n");
	}

	printf("Interpolation: remainder %d ending threshold %f\n", 
		   remainder, threshold);

	*retRemainder = remainder;

	return threshold;
}

