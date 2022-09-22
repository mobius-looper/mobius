/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Segment a loop into some number of pieces and shuffle them.
 * 
 * I tried to eliminate the need for ShuffleEventType and
 * just make an EventType and set the name, but unfortunately
 * there is too much baggage in Function and Loop that searches
 * by EventType and makes assumptions when they are the same. 
 * If we start using generic events, then we would need to search
 * on the Function or some other attachment to the EventType rather
 * than constant EventTypes.  Give this more thought...
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Util.h"

#include "Action.h"
#include "Event.h"
#include "Expr.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Segment.h"
#include "Synchronizer.h"

//////////////////////////////////////////////////////////////////////
//
// Constants
//
//////////////////////////////////////////////////////////////////////

/**
 * The maximum number of granules we allow for a shuffle operation.
 * This should be larger than the highest useful value for the
 * "subcycles" parameter which usually determines the number of granules.
 * !! Should be a global parameter?
 */
#define MAX_SHUFFLE_GRANULES 128

//////////////////////////////////////////////////////////////////////
//
// ShuffleEvent
//
//////////////////////////////////////////////////////////////////////

/**
 * Need one so we have a unique type constant, but we use the
 * default implementation that forwards to the Function.
 */
class ShuffleEventType : public EventType {
  public:
	ShuffleEventType();
};

PUBLIC ShuffleEventType::ShuffleEventType()
{
	name = "Shuffle";
}

PUBLIC EventType* ShuffleEvent = new ShuffleEventType();

//////////////////////////////////////////////////////////////////////
//
// ShuffleFunction
//
//////////////////////////////////////////////////////////////////////

class ShuffleFunction : public Function {

  public:
	ShuffleFunction();
	Event* scheduleEvent(Action* action, Loop* l);
	void doEvent(Loop* loop, Event* event);
	void undoEvent(Loop* loop, Event* event);

  private:

	void shuffle(Loop* loop, Layer* layer, Preset::ShuffleMode mode, int granules);
    void shuffle(Loop* loop, Layer* layer, ExValueList* pattern);
    void segmentize(Loop* loop, Layer* layer, int sourceGranules, 
					int resultGranules, int* pattern);
    int getRandomUnused(int* sources, int sourceGranules, int* remaining);
    int processProbabilityPattern(ExValueList* list, int granules, int granule);
    int resolveSegment(ExValue* v, int granules, int granule);
};

PUBLIC Function* Shuffle = new ShuffleFunction();

PUBLIC ShuffleFunction::ShuffleFunction() :
    Function("Shuffle", MSG_FUNC_SHUFFLE)
{
	eventType = ShuffleEvent;
	cancelReturn = true;
	mayCancelMute = true;
	instant = true;
    variableArgs = true;

	// could do SoundCopy then instant shuffle!!
	//switchStack = true;
	//switchStackMutex = true;
}

Event* ShuffleFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* event = NULL;
	
	event = Function::scheduleEvent(action, l);
	if (event != NULL) {

        // if there were script args, transfer them to the event
        if (action->scriptArgs != NULL) {
            event->setArguments(action->scriptArgs);
            // note that we have to NULL this or the interpreter will free it!
            action->scriptArgs = NULL;
        }
	}

	return event;
}

/**
 * Event handler.
 */
void ShuffleFunction::doEvent(Loop* loop, Event* event)
{
    bool shuffled = false;

	// shift immediately so we have only one cycle to deal with
	loop->shift(false);

	Layer* layer = loop->getRecordLayer();

	// TODO: get script argument values through the even to control
	// the number of granules
    ExValueList* list = event->getArguments();
    if (list != NULL && list->size() > 1) {
        // new style, first arg has granules and the rest has the pattern
        // have to have at least one pattern value
        long originalFrames = layer->getFrames();
        shuffle(loop, layer, list);
        shuffled = true;
			
        long newFrames = layer->getFrames();
        if (newFrames != originalFrames) {

            if (newFrames < originalFrames) {
                // If the shuffle pattern decreased the loop size, treat
                // it like an InstantDivide for sync.

                // may have to wrap the loop frame
                long frame = loop->getFrame();
                long wrapped = loop->wrapFrame(frame, newFrames);

                if (wrapped != frame) {
                    Trace(loop, 2, "Shuffle: wrapped loop frame from %ld to %ld\n", 
                          frame, wrapped);
                    loop->setFrame(wrapped);
                    loop->recalculatePlayFrame();
                }
            }

            // let sync know about the resize
            Synchronizer* sync = loop->getSynchronizer();
            sync->loopResize(loop, false);
        }
    }
    else {
        // old style let a single arg override the granule
        // but use shuffle mode from the preset
        // loop size doesn't change so we don't have to mess with sync
        Preset* preset = loop->getPreset();
        int granules = preset->getSubcycles();

        if (list != NULL && list->size() > 0) { 
            ExValue* arg = list->getValue(0);
            int alt = arg->getInt();
            if (alt > 0)
              granules = alt;
        }

        Preset::ShuffleMode mode = preset->getShuffleMode();

        shuffle(loop, layer, mode, granules);
        shuffled = true;
    }

    if (shuffled) {
        // shift and again so we can undo right away
        // !! think more here can have unnecessary layers?
        loop->shift(true);
    }

	loop->checkMuteCancel(event);

	// do we always cancel the previous mode?
	loop->resumePlay();

	// record and play frames do not change
	loop->validate(event);

    // Event doesn't manage this we have to
    event->clearArguments();
}

/**
 * Undo handler.
 * Nothing to do but need to define in order to avoid a trace warning.
 */
void ShuffleFunction::undoEvent(Loop* loop, Event* event)
{
}

/**
 * Break the layer into segments and shuffle them.
 * It is assumed that we have just shifted, there will be a single
 * segment, and we can simply duplicate it.
 *
 * It is assumed that granules may be of different sizes, which complicates
 * the algorithm.  When shuffling, the granule sizes stay in the same position,
 * they are just filled with content from different locations in the loop.
 * This is to maintain the same rhythmic feel imparted by the granule
 * sizes.  It might also be interesting to shuffle the sizes, creating
 * different rhythms.
 *
 * It may also be interesting to have options to duplicate or drop granules.
 *
 */
void ShuffleFunction::shuffle(Loop* loop, Layer* layer, 
							  Preset::ShuffleMode mode, int granules)
{
	Segment* original = layer->getSegments();

	if (original == NULL) {
		Trace(loop, 1, "Shuffle: shuffle with no backing layer!\n");
	}
	else if (original->getNext() != NULL) {
		Trace(loop, 1, "Shuffle: shuffle with more than one segment!\n");
	}
	else if (granules > MAX_SHUFFLE_GRANULES) {
		Trace(loop, 1, "Shuffle: shuffle with too many granules: %ld!\n", (long)granules);
	}
	else if (granules > 1) {
		// Mac is relatively strict/retarded about "for" iteration variable scoping
		int i, psn;

		// Step 1: determine the pull positions
		int positions[MAX_SHUFFLE_GRANULES];
		switch (mode) {
			case Preset::SHUFFLE_REVERSE: {
				psn = 0;
				for (i = granules - 1 ; i >= 0 ; i--)
				  positions[psn++] = i;
			}
			break;
			case Preset::SHUFFLE_SHIFT: {
				for (i = 0 ; i < granules ; i++)
				  positions[i] = i+1;
				positions[granules-1] = 0;
			}
			break;
			case Preset::SHUFFLE_SWAP: {
				// length should be a configurable thing!
				int length = 1;
				int dest = 0;
				int src = dest + length;
				while (src < granules) {
					// don't swap unless we have a full unit
					int next = src + length;
					if (next <= granules) {
						for (i = 0 ; i < length ; i++) {
							positions[dest] = src;
							positions[src++] = dest++;
						}
						// skip over the source frames
						dest = src;
					}
					src += length;
				}
				// remainder stays the same
				while (dest < granules) {
					positions[dest] = dest;
					dest++;
				}
			}
			break;

			case Preset::SHUFFLE_RANDOM: {	
				// array to mark source granules as they are chosen
				int sources[MAX_SHUFFLE_GRANULES];
				for (i = 0 ; i < granules ; i++)
				  sources[i] = 0;

				int remaining = granules;
				int dest = 0;
				while (remaining > 1) {
					int r = Random(0, remaining - 1);
					// find the nth unused source granule
					int source = -1;
					int found = 0;
					for (i = 0 ; i < granules && source == -1 ; i++) {
						if (sources[i] == 0) {
							if (found < r)
							  found++;
							else
							  source = i;
						}
					}
 
					if (source == -1) {
						Trace(loop, 1, "Layer: Shuffle randomization error!\n");
						source = 0;
					}

					sources[source] = 1;		// mark it used
					positions[dest++] = source;
					remaining--;
				}

				// there should be one left
				int last = -1;
				for (i = 0 ; i < granules && last == -1 ; i++) {
					if (sources[i] == 0)
					  last = i;
				}
				if (last == -1) {
					Trace(loop, 1, "Layer: Shuffle randomization error!\n");
					last = 0;
				}
				positions[dest] = last;
			}
			break;
		}
        
        // Original algorithm numbered granules from zero and I didn't
        // want to mess with that. The new segmentize expects them
        // to be numbered from 1 so adjust.
        for (i = 0 ; i < granules ; i++)
          positions[i] = positions[i] + 1;

        // convert the pull pattern into segments
        segmentize(loop, layer, granules, granules, positions);
	}
}

/**
 * Given a "pull pattern" created from one of the shuffle modes,
 * create a list of Segments for each granule and install them.
 */
void ShuffleFunction::segmentize(Loop* loop, Layer* layer, 
								 int sourceGranules, 
								 int resultGranules,
                                 int* pattern)
{
	Segment* original = layer->getSegments();
    int i;

    for (i = 0 ; i < resultGranules ; i++)
      Trace(loop, 2, "Segmentize %d %d\n", (long)i, (long)pattern[i]);

    // Step 1: determine range of the source granules
	// Assuming sizes are all the same, if we wanted to use cue points here
	// it really complicates how these correspond to the cube points
	// in the result loop.  We do however have to catch round off and
	// make sure the last granule contains the entire loop.
	// !! this doesn't make any difference if that segment isn't used.
	// have to handle round off during assembly
    long sourceFrames = layer->getFrames();
    long granuleFrames = sourceFrames / sourceGranules;

    // Step 2: create the segments
    Segment* segments[MAX_SHUFFLE_GRANULES];
    Segment* prev = NULL;
    long offset = 0;
    for (i = 0 ; i < resultGranules ; i++) {

        // note that granules in the pattern array are numbered from 1,
        // negative means reverse, and zero means empty
        int p = pattern[i];
        int absp = (p > 0) ? p : -p;
        int granule = absp - 1;
		if (granule < 0 || granule >= sourceGranules)  {
			// empty or out of range
            segments[i] = NULL;
		}
		else {
            // start by cloning the origianl layer segment
            Segment* s = new Segment(original);
			long start = granule * granuleFrames;
            s->setOffset(offset);
            s->setStartFrame(start);
            s->setFrames(granuleFrames);
            s->setReverse((p < 0));
            s->setFadeLeft(true);
            s->setFadeRight(true);
            segments[i] = s;

            if (prev != NULL) {
                int prevEnd = prev->getStartFrame() + prev->getFrames();
                if (prevEnd == start) {
                    // adjacent on the left
                    // check for direction!
                    s->setFadeLeft(false);
                    prev->setFadeRight(false);
                }
            }
            prev = s;
        }
        offset += granuleFrames;
    }
	
	// if result is an even multiple if the source, round up if necessary
	// to ensure the loop is exactly the same size (or an exact multiple) 
	// to maintain sync

	if ((resultGranules >= sourceGranules) && 
		((resultGranules % sourceGranules) == 0)) {
		int multiples = resultGranules / sourceGranules;
		long desired = sourceFrames * multiples;
		long delta = desired - offset;
		if (delta > 0) {
			Trace(2, "Rounding shuffle segments to add %ld frames\n", delta);
			// ideally we would be smart enough to spread the adjustment
			// over several segments but we're going to pile it onto the end
			// since we can more easily adjust both it's offset and size.
			// Extend on the right if we can, then left.
			Segment* last = segments[resultGranules-1];
			if (last == NULL) {
				// loop ends with empty space, just extend the emptiness
			}
			else {
				long startFrame = last->getStartFrame();
				long frames = last->getFrames();
				long end = startFrame + frames;
				long avail = sourceFrames - end;
				if (avail < 0) {
					// something wrong in segment size calculations
					Trace(1, "Shuffle: Unexpected segment sizes!\n");
				}
				else {
					// assume we'll find it on one or both sides
					frames += delta;

					if (avail < delta) {
						// must be the last segment, extend the end as much as
						// we can (typicaly 1) then extend the front
						startFrame -= (delta - avail);
						if (startFrame < 0) {
							// overflowed both the start and end, shouldn't happen
							Trace(1, "Shuffle: overflow on both ends!\n");
							// reduce the frame count by the overflow
							frames += startFrame;
							startFrame = 0;
						}
					}
				}

				last->setStartFrame(startFrame);
				last->setFrames(frames);

				// whatever happened the final offset (result size)
				// advances by the necessary amount, this may end up
				// putting blank padding at the end
				offset += delta;
			}
		}
	}

	// adjust edge fades for adjacent segments
	// ?? just let compileSegmentFades handle this?
	prev = NULL;
	for (int i = 0 ; i < resultGranules ; i++) {
		Segment* s = segments[i];
		if (prev != NULL && s != NULL) {
			int prevEnd = prev->getStartFrame() + prev->getFrames();
			// need to be smarter with reverse!
			if (prevEnd == s->getStartFrame() && 
				!prev->isReverse() && !s->isReverse()) {

				// adjacent on the left
				s->setFadeLeft(false);
				prev->setFadeRight(false);
			}
		}
		prev = s;
	}

    // fade adjustment on the edges
    Segment* first = segments[0];
	Segment* last = NULL;
    if (first != NULL && last != NULL) {
        long lastFrame = last->getStartFrame() + last->getFrames();
        // sigh, not smart enough with reverse
        if (lastFrame == first->getStartFrame() &&
            !first->isReverse() && !last->isReverse()) {
            last->setFadeRight(false);
            first->setFadeLeft(false);
        }
    }

    // replace the segments
    layer->resetSegments();
    for (i = 0 ; i < resultGranules ; i++) {
        Segment* s = segments[i];
        if (s != NULL)
          layer->addSegment(segments[i]);
    }
	
	// Layer will have a residual mFrames from the source layer, 
	// recalculate this based on the new segment list, this will
	// also set the framesize of the embedded Audio which I think
	// is necessary.  "offset" was left one after the length of the
	// final granule.  Note that you can't use resizeFromSegments here
	// since the final segment may have been empty and suppressed.
	layer->resize(offset);

    // hmm, you would think resetSegments and addSegment would
    // do this but they're also used in contexts where that may
    // not be desired?
    layer->setStructureChanged(true);
}

/**
 * Placeholder values with special meaning we leave in the positions
 * array.  These need to be at least as large as MAX_SHUFFLE_GRANULES
 */
#define GRANULE_RANDOM_UNUSED 1000
#define GRANULE_PREVIOUS 1001
#define GRANULE_END 1002

/**
 * New shuffle with complex patterns.
 * There are two wildcard symbols:
 *
 *   r - select a segment at random
 *   u - select an unused segment at random, if all segments have been used
 *       granule is empty
 *   p - use the previous segment or select one at random
 *   e - ends the assembly of the new layer
 *
 * Both of these may be prefixed with - to indicate reverse.
 * Note that these have to be quoted so they're not treated like
 * variable references to the expression evaluator.
 *
 * If an element of the shuffle pattern is a list, one of the sub elements
 * is selected at random:
 *
 *   (1 2 3) - select segment 1, 2, or 3 at random
 *
 *   (1 "r") - select segment 1 or some random segment with equal probability
 *
 * If the subelements are themselves lists, they must be a pair of values
 * the first the segment and the second the probability of choosing that
 * segment expressed as a floating value from 0 to 1.  
 *
 *   ((1 0.33) (2 0.66))   - a 33% chance of selecting 1 and 66% chance of 2
 *
 * The actual probabilities may be different than what is expressed in 
 * the pattern.  If the total of all probabilities is less than 1.0 then
 * the final probability is rounded up to reach 1.0.  So in the previous
 * example the actual probability would be 0.67.  If the probability
 * total ever exceeds 1.0, it is limited in the element where the total
 * is exceed and any remaining elements have a probability of zero.
 *
 *   ((1 .8) (2 .3) (3 .3))
 *
 * The effective probability of segment 2 is .2 rather than .3 and the
 * effective probability of segment 3 is 0.
 *
 * To implement * we have to make two passes, the first to assign
 * the non-ambiguous or truly random segments, and the second to select
 * from the unchosen segments.  We need a special marker in the
 * positions array to indiciate this, -1 can't be used since that's
 * a valid (reverse segment 1).  We'll assume the value MAX_SHUFFLE_GRANULES
 * means a "random reminaing" selection in a positive direction
 * and MAX_SHUFFLE_GRANULES+1 means this in a negative direction.
 *
 * EXTEND AND CUT
 *
 * If the shuffle pattern has fewer elements than the number of source
 * grains, the pattern is repeated until the new layer has the same
 * number of grains as the source layer.  This can be used as an abbreviation:
 * This pattern cuts the source layer into 8 granules, and builds a new
 * layer with 8 granules selected at random from the source layer.
 * 
 *     8,?
 *
 * This pattern repeats grains 1 and 8 four times.
 *
 *    8,1,8
 *
 * The symbol "e" may be used in a pattern to terminate the assembly of
 * result granules.  This pattern cuts the source layer into 8 granules,
 * then creates a new layer containing only the second grain:
 *
 *     8,2,e
 *
 * If a pattern has more elements than the number of source granules, 
 * the new layer is extended to have the number of pattern granules.
 * This example creates a layer that is twice as long as the source layer,
 * it is effectively the same as an InstantMultiply by 2.
 *
 *    2,1,2,1,2
 *
 */
void ShuffleFunction::shuffle(Loop* loop, Layer* layer, ExValueList* pattern)
{
	Segment* original = layer->getSegments();
	int patternLength = pattern->size() - 1;
    int sourceGranules = 0;

    ExValue* el = pattern->getValue(0);
    if (el != NULL)
      sourceGranules = el->getInt();

	if (original == NULL) {
		Trace(loop, 1, "Shuffle: shuffle with no backing layer!\n");
	}
	else if (original->getNext() != NULL) {
		Trace(loop, 1, "Shuffle: shuffle with more than one segment!\n");
	}
    else if (sourceGranules == 0) {
		Trace(loop, 1, "Shuffle: shuffle with no granules!\n");
    }
    else if (sourceGranules > MAX_SHUFFLE_GRANULES) {
		Trace(loop, 1, "Shuffle: shuffle with too many granules: %ld!\n", 
			  (long)sourceGranules);
    }
    else if (patternLength < 1) {
		Trace(loop, 1, "Shuffle: shuffle must have at least one pattern value!\n");
    }
    else if (patternLength > MAX_SHUFFLE_GRANULES) {
		Trace(loop, 1, "Shuffle: shuffle pattern is too long!\n");
    }
    else {
		// Step 1: determine the pull positions and count the result granules
        int usedSources[MAX_SHUFFLE_GRANULES];
		int resultPattern[MAX_SHUFFLE_GRANULES];

        for (int i = 0 ; i < sourceGranules ; i++)
		  usedSources[i] = 0;

		int resultGranules = sourceGranules;
		if (patternLength > resultGranules)
		  resultGranules = patternLength;

        for (int i = 0 ; i < resultGranules ; i++)
		  resultPattern[i] = 0;

        int patternPsn = 1;

        for (int granule = 0 ; granule < resultGranules ; granule++) {
            
            // numbered from 1 with zero meaning empty and negative 
            // meaning reverse
            int segment = 0;
			bool end = false;
            el = pattern->getValue(patternPsn);
            if (el != NULL) {
                ExType type = el->getType();
                if (type == EX_LIST) {
                    // complex probability pattern
                    segment = processProbabilityPattern(el->getList(), 
                                                        sourceGranules,
														granule);
                }
				else {
					segment = resolveSegment(el, sourceGranules, granule);
				}
			}

			if (segment == GRANULE_END) {
				// special marker to terminate the destination pattern early
				resultGranules = granule;
			}
			else {
				resultPattern[granule] = segment;

				int pos = (segment < 0) ? -segment : segment;
				if (pos < MAX_SHUFFLE_GRANULES) {
					// it's a 1 based granule number, convert to array index
					pos--;
					if (pos >= 0)
					  usedSources[pos] = 1;
				}

				// remember the list starts with the granule count which 
				// is not part of the pattern
				patternPsn++;
				if (patternPsn >= pattern->size())
				  patternPsn = 1;
			}
		}

        // process GRANULE_RANDOM_UNUSED
        int remaining = sourceGranules;
        for (int i = 0 ; i < sourceGranules ; i++) {
            if (usedSources[i] > 0)
              remaining--;
        }

        for (int i = 0 ; i < resultGranules ; i++) {
            int segment = resultPattern[i];
            if (segment == GRANULE_RANDOM_UNUSED ||
                segment == -GRANULE_RANDOM_UNUSED) {

                int actual = getRandomUnused(usedSources, sourceGranules, &remaining);
                if (segment == -GRANULE_RANDOM_UNUSED) 
                  actual = -actual; 

                resultPattern[i] = actual;
            }
        }

        // final pass doing GRANULE_PREVIOUS
        for (int i = 0 ; i < resultGranules ; i++) {
            int segment = resultPattern[i];
            if (segment == GRANULE_PREVIOUS ||
                segment == -GRANULE_PREVIOUS) {

                // should have caught i == 0 above
                int actual = 0;
                if (i > 0) {
                    actual = resultPattern[i - 1];
                    if (segment == -GRANULE_PREVIOUS)
                      actual = -actual;
                }

                resultPattern[i] = actual;
            }
        }

        // convert the pull pattern into segments
        segmentize(loop, layer, sourceGranules, resultGranules, resultPattern);
	}
}

/**
 * Derive a segment identifier from an ExValue in a shuffle pattern.
 * If the element is an EX_LIST it is assumed to be
 * a "probability pattern" and we return the value of the first
 * element.
 */
PRIVATE int ShuffleFunction::resolveSegment(ExValue* value, 
											int sourceGranules, 
                                            int resultGranule)
{
    // numbered from 1 with zero meaning empty and negative 
    // meaning reverse
    int segment = 0;

    ExType type = value->getType();
    if (type == EX_INT) {
        segment = value->getInt();
    }
    else if (type == EX_FLOAT) {
        // shouldn't see these at top level I suppose this
        // could be used to shift the start point, e.g. 1.5
        // is halfway into the first granule but we have no
        // way to go backward if negative means reverse, 
        // just coerce to an int
        segment = value->getInt();
    }
    else if (type == EX_BOOL) {
        // shouldn't see, I guess 1 and off
        segment = value->getInt();
    }
    else if (type == EX_STRING) {
        const char* str = value->getString();
        if (str != NULL) {
            bool negative = false;
            const char* ptr = str;
            char ch = *ptr;

            // we'll support a leading zero if you quote
            // the string but since that's cumbersome it's
            // more likely to see the rr, ru, and rp variants
            if (ch == '-') {
                negative = true;
                ptr++;
                ch = *ptr;
            }

            if (ch == 'r' && strlen(ptr) > 1) {
                negative = true;
                ptr++;
                ch = *ptr;
            }

            if (ch == 'r') {
                segment = Random(1, sourceGranules);
            }
            else if (ch == 'u') {
                segment = GRANULE_RANDOM_UNUSED;
            }
            else if (ch == 'e') {
                segment = GRANULE_END;
				// negation is meaningless
				negative = false;
            }
            else if (ch == 'p') {
                if (resultGranule == 0)
                  segment = Random(1, sourceGranules);
                else
                  segment = GRANULE_PREVIOUS;
            }
            else {
                Trace(1, "Unrecognized shuffle pattern: %s\n", str);
            }

            if (negative)
              segment = -segment;
        }
    }
    else if (type == EX_LIST) {
        // this can't be called with the top-level probability pattern
        // if we see sublists again, it is one element of the
        // probability pattern, return the first value
        ExValueList* list = value->getList();
        if (list != NULL) {
            ExValue* el = list->getValue(0);
            if (el != NULL)
              segment = resolveSegment(el, sourceGranules, resultGranule);
        }
    }
    
    return segment;
}

/**
 * Helper to select from one of the available unused granules, 
 * mark it as used and return the new remaining count.
 * This isn't particularly effecient but we're trying to avoid
 * allocating an temporary search structure like a List.
 * Give the other allocs that happen this isn't such a big deal
 * but we're trying to cut down on these, don't make the problem worse.
 */
PRIVATE int ShuffleFunction::getRandomUnused(int* usedSources, int sourceGranules, 
                                             int* remaining)
{
    int segment = 0;

    int avail = *remaining;
    if (avail <= 0) {
        // Two options: pick any one at random or leave it empty
        // I'm liking empty 
    }
    else {
        int next;
        if (avail == 1)
          next = 0;
        else
          next = Random(0, avail - 1);

        // find the nth unused source granule
        int source = -1;
        int found = 0;
        for (int i = 0 ; i < sourceGranules && source == -1 ; i++) {
            if (usedSources[i] == 0) {
                if (found < next)
                  found++;
                else
                  source = i;
            }
        }
 
        if (source == -1) {
            Trace(1, "Layer: Shuffle randomization error!\n");
            source = 0;
        }

        usedSources[source] = 1;		// mark it used
		
		// source is currently zero based, convert it to one based for return
        segment = source + 1;

        // return the new remainder count
        *remaining = avail - 1;
    }

    return segment;
}

/**
 * Probability lists look like this in the simple case:
 *
 *   (1 2 3)
 *
 * Which selects one of the elements at random.
 * Or this:
 * 
 *   ((1 .5) 2 3)
 *
 * Which selects the first one 50% of the time and the rest 25%
 */

PRIVATE int ShuffleFunction::processProbabilityPattern(ExValueList* list,
                                                       int sourceGranules,
                                                       int resultGranule)
{
    int segment = 0;

    if (list != NULL) {
        int units = list->size();
        if (units == 1) {
            segment = resolveSegment(list->getValue(0), sourceGranules, resultGranule);
        }
        else {
            // calculate the probabilities of each potential
            float probabilities[MAX_SHUFFLE_GRANULES];

            // pass 1, find the hard probabilities
            float cumulativeProbability = 0.0f;
            int unspecified = 0;
            for (int i = 0 ; i < units ; i++) {
                float probability = -1.0f;
                ExValue* v = list->getValue(i);
                if (v != NULL) {
                    if (v->getType() == EX_LIST) {
                        ExValueList* sub = v->getList();
                        if (sub != NULL && sub->size() > 1) {
                            ExValue* pv = sub->getValue(1);
                            if (pv != NULL)
                              probability = pv->getFloat();
                        }
                    }
                }

                probabilities[i] = probability;
                if (probability >= 0.0f)
                  cumulativeProbability += probability;
                else
                  unspecified++;
            }

            // pass 2, carve up the remainder
            if (unspecified > 0) {
                float probability = 0.0f;
                float remainder = 1.0f - cumulativeProbability;
                if (remainder > 0.0f)
                  probability = remainder / (float)unspecified;

                for (int i = 0 ; i < units ; i++) {
                    if (probabilities[i] < 0.0f)
                      probabilities[i] = probability;
                }
            }

            // accumulate probabilities until they exceed this
            float threshold = RandomFloat();
            bool found = false;
            cumulativeProbability = 0.0f;

            for (int i = 0 ; i < units && !found ; i++) {
                cumulativeProbability += probabilities[i];
                if (threshold < cumulativeProbability) {
                    ExValue* v = list->getValue(i);
                    if (v != NULL) {
                        segment = resolveSegment(v, sourceGranules, resultGranule);
                        found = true;
                    }
                }
            }

            if (!found) {
                // There are two possibilities here the Random was
                // at or near 1.0 and the cumulative probabilities
                // were a bit short, e.g. .9 or .99.
                // Or explicit probabilities were specified and they
                // didn't add up to 1.0, e.g. ((1 .25) (2 .25))
                //
                // In the first case you usually want to round up and
                // take the last value.  In the second case you might
                // want to treat this as an empty selection.
                // since you can always encode an empty selection with:
                // ((1 .25) (2 .25) 0) I'm leaning toward taking the
                // last one.

                ExValue* v = list->getValue(units - 1);
                if (v != NULL)
                  segment = resolveSegment(v, sourceGranules, resultGranule);
            }
        }
    }

    return segment;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
