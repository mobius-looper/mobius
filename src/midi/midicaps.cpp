/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Dumps information about Multimedia devices.
 *
 */

#include <stdio.h>
#include <memory.h>

#include <windows.h>
#include <mmsystem.h>

#include "Port.h"

/****************************************************************************
 *                                                                          *
 *   					   DIRECT WINDOWS INTERFACE                         *
 *                                                                          *
 ****************************************************************************/

PRIVATE void dump_incaps(int id)
{
	MIDIINCAPS 	mic;
	int 		stat;

	stat = midiInGetDevCaps(id, &mic, sizeof(mic));
	if (stat != MMSYSERR_NOERROR)
	  printf("Error reading device capabilities for %d\n", id);
	else {
		printf("Id              : %d\n", id);			  
		printf("Name            : %s\n", mic.szPname);
		printf("Manufacturer id : %d\n", mic.wMid);
		printf("Product id      : %d\n", mic.wPid);
		printf("Version         : %d.%d\n",
			   HIBYTE(mic.vDriverVersion), 
			   LOBYTE(mic.vDriverVersion));
	}
}

PRIVATE void dump_outcaps(int id)
{
	MIDIOUTCAPS moc;
	int 		stat;

	stat = midiOutGetDevCaps(id, &moc, sizeof(moc));
	if (stat != MMSYSERR_NOERROR)
	  printf("Error reading device capabilities for %d\n", id);
	else {
		printf("Id              : %d\n", id);
		printf("Name            : %s\n", moc.szPname);
		printf("Manufacturer id : %d\n", moc.wMid);
		printf("Product id      : %d\n", moc.wPid);
		printf("Version         : %d.%d\n",
			   HIBYTE(moc.vDriverVersion), 
			   LOBYTE(moc.vDriverVersion));
		printf("Technology      : ");
		switch (moc.wTechnology) {
			case MOD_MIDIPORT:
				printf("output port");
				break;
			case MOD_SYNTH:
				printf("generic internal synth");
				break;
			case MOD_SQSYNTH: 
				printf("square wave internal synth");
				break;
			case MOD_FMSYNTH:
				printf("FM internal synth");
				break;
			case MOD_MAPPER:
				printf("MIDI mapper");
				break;
			default:
				printf("unknown");
		}
		printf("\n");
		printf("Voices          : %d\n", (int)moc.wVoices);
		printf("Notes           : %d\n", (int)moc.wNotes);
		printf("Channel mask    : %d\n", (int)moc.wChannelMask);
		printf("Driver support  : ");
		if (moc.dwSupport & MIDICAPS_VOLUME)
		  printf("volume ");
		if (moc.dwSupport & MIDICAPS_LRVOLUME)
		  printf("LRvolume ");
		if (moc.dwSupport & MIDICAPS_CACHE)
		  printf("cache ");
		printf("\n");
	}
}

INTERFACE void midi_stats(void)
{
	int 		ndevs, i;

	// find the number of input devices installed
	ndevs = midiInGetNumDevs();
	if (ndevs == 0) {
		printf("No MIDI Input devices installed\n");
	}
	else {
		printf("\n%d INPUT DEVICES\n\n", ndevs);
		for (i = 0 ; i < ndevs ; i++) {
			if (i)
			  printf("-------------------------------------\n");
			dump_incaps(i);
		}
		printf("-------------------------------------\n");
	}

	// do the same for output devices
	ndevs = midiOutGetNumDevs();
	if (ndevs == 0) {
		printf("No MIDI Ouput devices installed\n");
	}
	else {
		printf("\n%d OUTPUT DEVICES\n\n", ndevs);
		for (i = 0 ; i < ndevs ; i++) {
			if (i)
			  printf("-------------------------------------\n");
			dump_outcaps(i);
		}
		printf("-------------------------------------\n");
		dump_outcaps(MIDI_MAPPER);
	}

	// get timer caps too
	{
		TIMECAPS tc;
		int rc;
		rc = timeGetDevCaps(&tc, sizeof(tc));
		if (rc != MMSYSERR_NOERROR)	
		  printf("Unable to determine timer capabilities\n");
		else {
			printf("\nTimer Capabilities\n\n");
			printf("Mimumum period : %d\n", tc.wPeriodMin);
			printf("Maximum period : %d\n", tc.wPeriodMax);
			printf("Time is        : %d\n", (int)timeGetTime());
		}
	}

}

/****************************************************************************
 *                                                                          *
 *   								 MAIN                                   *
 *                                                                          *
 ****************************************************************************/


void main(int arg, char *argv)
{
	midi_stats();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
