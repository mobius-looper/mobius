/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Dialog for selection of the Audio devices.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Util.h"
#include "MessageCatalog.h"
#include "Qwin.h"

#include "AudioInterface.h"

#include "Messages.h"
#include "MobiusConfig.h"
#include "Parameter.h"

#include "UI.h"

#define DEFAULT_ASIO_LATENCY_MSEC 5
#define DEFAULT_MME_LATENCY_MSEC 100

/**
 * The maximum value we'll allow for the latency overrides.
 * Since this is a function of buffer size, and you almost never
 * have buffer sizes more than a few thousand, prevent anything
 * stupid from happening.
 *
 * The number here is a full second of 48K audio which should be
 * more than enough.
 */
#define MAX_LATENCY 48000


PUBLIC AudioDialog::AudioDialog(Window* parent, MobiusInterface* m, 
								MobiusConfig* config)
{
	const char* apiNames[] = {NULL, NULL, NULL};
	char buffer[1024];
	MessageCatalog* cat = m->getMessageCatalog();

	apiNames[0] = cat->get(MSG_DLG_AUDIO_MME);
	apiNames[1] = cat->get(MSG_DLG_AUDIO_ASIO);

	setParent(parent);
	setModal(true);
	setTitle(cat->get(MSG_DLG_AUDIO_TITLE));
	setInsets(20, 20, 20, 0);

	mMobius = m;
	mConfig = config;
	mDevices = NULL;

	const char* input = config->getAudioInput();
	const char* output = config->getAudioOutput();

	Panel* root = getPanel();
	TabbedPane* tabs = new TabbedPane();
	root->add(tabs);

    Panel* p = new Panel("Standalone");
    p->setLayout(new VerticalLayout());
    tabs->add(p);

    // this will only be included on Windows
	mASIO = new ListBox();
	mASIO->addActionListener(this);

    // On Mac these will show CoreAudio devices, on Windows MME devices
    // which no one uses any more.  Should we just get rid of these on Window?

	mInputs = new ListBox();
	mInputs->addActionListener(this);
	mOutputs = new ListBox();
	mOutputs->addActionListener(this);

    int asios = 0;
	bool asioSelected = false;
    MobiusContext* mc = m->getContext();
	AudioInterface* ai = mc->getAudioInterface();
	mDevices = ai->getDevices();
	if (mDevices != NULL) {
		int inputs = 0;
		int outputs = 0;
		for (int i = 0 ; mDevices[i] != NULL ; i++) {
			AudioDevice* d = mDevices[i];
			if (d->getApi() == API_ASIO) {
				mASIO->addValue(d->getName());
				if (input != NULL && !strcmp(input, d->getName())) {
					mASIO->setSelectedIndex(asios);
					asioSelected = true;
				}
				asios++;
			}
			else {
				if (d->isInput()) {
					mInputs->addValue(d->getName());
					if (input != NULL && !strcmp(input, d->getName()))
					  mInputs->setSelectedIndex(inputs);
					inputs++;
				}
				if (d->isOutput()) {
					mOutputs->addValue(d->getName());
					if (output != NULL && !strcmp(output, d->getName()))
					  mOutputs->setSelectedIndex(outputs);
					outputs++;
				}
			}
		}
	}

#ifdef _WIN32
    if (asios > 0) {
        p->add(new Label(cat->get(MSG_DLG_AUDIO_ASIO_TITLE)));
        p->add(mASIO);
        p->add(new Strut(0, 10));
    }
#endif

	p->add(new Label(cat->get(MSG_DLG_AUDIO_INPUT)));
	p->add(mInputs);
	p->add(new Strut(0, 10));
	p->add(new Label(cat->get(MSG_DLG_AUDIO_OUTPUT)));
	p->add(mOutputs);

	FormPanel* form = new FormPanel();
	p->add(new Strut(0, 10));
	p->add(form);

    mSampleRate = form->addCombo(this, SampleRateParameter->getDisplayName(),
                                 SampleRateParameter->values);
    mSampleRate->setValue((int)mConfig->getSampleRate());

	mLatencyMsec = 
		form->addNumber(this, cat->get(MSG_DLG_AUDIO_SUGGESTED), 0, 1000);
	mLatencyMsec->setHideNull(true);
	
	Panel* hp = new Panel();
	hp->setLayout(new HorizontalLayout());
	mInputLatency = new NumberField(0, MAX_LATENCY);
	mInputLatency->setHideNull(true);
	hp->add(mInputLatency);
	sprintf(buffer, cat->get(MSG_DLG_AUDIO_CURRENT),
			m->getReportedInputLatency());
	hp->add(new Label(buffer));
	form->add(cat->get(MSG_DLG_AUDIO_OVERRIDE_INPUT), hp);

	hp = new Panel();
	hp->setLayout(new HorizontalLayout());
	mOutputLatency = new NumberField(0, MAX_LATENCY);
	mOutputLatency->setHideNull(true);
	hp->add(mOutputLatency);
	sprintf(buffer, cat->get(MSG_DLG_AUDIO_CURRENT),
			m->getReportedOutputLatency());
	hp->add(new Label(buffer));
	form->add(cat->get(MSG_DLG_AUDIO_OVERRIDE_OUTPUT), hp);

	mCalibrate = new Button(cat->get(MSG_DLG_AUDIO_CALIBRATE));
	mCalibrate->addActionListener(this);
	form->add("", mCalibrate);
	form->add("", new Strut(0, 30));

	mLatencyMsec->setValue(mConfig->getSuggestedLatencyMsec());
	mInputLatency->setValue(mConfig->getInputLatency());
	mOutputLatency->setValue(mConfig->getOutputLatency());

    // 
    // VST
    //

    p = new Panel("Plugin");
    p->setLayout(new VerticalLayout());
    tabs->add(p);

}




PUBLIC void AudioDialog::actionPerformed(void* src)
{
	if (src == mInputs) {
        if (mASIO != NULL)
          mASIO->setSelectedIndex(-1);
		// TODO: try to set initial suggested latency?
	}
	else if (src == mOutputs) {
        if (mASIO != NULL)
          mASIO->setSelectedIndex(-1);
		// TODO: try to set initial suggested latency?
	}
	else if (mASIO != NULL && src == mASIO) {
		mInputs->setSelectedIndex(-1);
		mOutputs->setSelectedIndex(-1);
		// TODO: try to set initial suggested latency?
	}
	else if (src == mCalibrate) {

		CalibrationDialog* cd = new CalibrationDialog(this, mMobius, mConfig);
		cd->show();
		if (!cd->isCanceled()) {

			CalibrationResult* result = cd->getResult();

			if (result->timeout) {
				MessageCatalog* cat = mMobius->getMessageCatalog();
				const char* title = cat->get(MSG_DLG_AUDIO_CALIBRATE_ERROR);
				const char* msg = cat->get(MSG_DLG_AUDIO_CALIBRATE_TIMEOUT);
				MessageDialog::showError(this, title, msg);
			}
			else {
				int expected = mMobius->getEffectiveInputLatency() + mMobius->getEffectiveOutputLatency();
				int delta = result->latency - expected;
				int hdelta = delta / 2;
				int input = mMobius->getEffectiveInputLatency() + hdelta;
				int output = mMobius->getEffectiveOutputLatency() + hdelta;
		
				CalibrationResultDialog* rd = 
					new CalibrationResultDialog(this, result->latency,
												input, output);

				rd->show();
				if (!rd->isCanceled()) {
					mInputLatency->setValue(input);
					mOutputLatency->setValue(output);
				}
				delete rd;
			}
		}
		delete cd;
	}
	else
	  SimpleDialog::actionPerformed(src);
}

PUBLIC AudioDialog::~AudioDialog()
{
}

PUBLIC bool AudioDialog::commit()
{
    const char* device = NULL;

    // prefer ASIO on windows
    if (mASIO != NULL)
      device = mASIO->getSelectedValue();

	if (device != NULL) {
		mConfig->setAudioInput(device);
		mConfig->setAudioOutput(device);
	}
	else {
		mConfig->setAudioInput(mInputs->getSelectedValue());
		mConfig->setAudioOutput(mOutputs->getSelectedValue());
	}

    mConfig->setSampleRate((AudioSampleRate)mSampleRate->getSelectedIndex());
	mConfig->setSuggestedLatencyMsec(mLatencyMsec->getValue());
    mConfig->setInputLatency(mInputLatency->getValue());
	mConfig->setOutputLatency(mOutputLatency->getValue());

	return true;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


