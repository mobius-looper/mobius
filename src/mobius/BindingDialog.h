/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * GUI for Mobius based on Qwin.
 *
 */

#ifndef BINDING_DIALOG_H
#define BINDING_DIALOG_H

#include <vector>

#include "KeyCode.h"
#include "Qwin.h"
#include "QwinExt.h"
#include "FontConfig.h"

// Needed for Trigger and other Binding things
#include "Binding.h"

// needed for LoopState
#include "MobiusState.h"

// needed for MobiusListener
#include "MobiusInterface.h"

QWIN_USE_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   							BINDING DIALOG                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Note that while this looks similar to MidiEventListener from
 * MidiListener.h the handler method returns a bool to indiciate
 * whether the event should be propagated to another listener.
 * This is used to redirect some events through the binding dialogs
 * while allowing others to be processed by Mobius.
 */
class UIMidiEventListener {
  public:
	virtual bool midiEvent(class MidiEvent* e) = 0;
};

/**
 * A transient model we use to represent a Binding in the
 * table, with some extra stuff that isn't in the Biding like
 * a consolodated name.  We also maintain a private ResolvedTarget
 * to handle resolution of the target.
 */
class BindingDefinition {
  public:

    BindingDefinition(class Binding* b);
    ~BindingDefinition();

    class Binding* getBinding();
    const char* getName();
    void setName(const char* s);

    void refresh(class MobiusInterface* mobius, class MobiusConfig* c);

  private:

    char* mName;
    class Binding* mBinding;
    class ResolvedTarget* mResolvedTarget;

};

/**
 * The model for the list of bindings.
 */
class BindingTableModel : public AbstractTableModel {
  public:

    BindingTableModel();
    ~BindingTableModel();

    void setBindings(List* l);
    List* getBindings();

    void addBinding(BindingDefinition* def);
    void removeBinding(BindingDefinition* def);

    int getRowCount();
    int getIndex(Binding* b);

    virtual int getColumnCount();
    virtual int getColumnPreferredWidth(int index);
    virtual const char* getColumnName(int index);
    virtual const char* getCellText(int row, int column);

  protected:

    void clear();
    void sort();

    // List of BindingTableRow
    List* mBindings;

};

class BindingDialog : public SimpleDialog {

  public:

    BindingDialog();
	virtual ~BindingDialog();

	void init(Window* parent, class UI* ui, class MobiusInterface* mobius, 
              class MobiusConfig* config);

    virtual const char* getDialogTitle() = 0;
    virtual const char* getBindingsPanelLabel() = 0;
    virtual const char* getSelectorLabel();
    virtual bool isMultipleConfigurations() = 0;
    virtual bool isUpdateButton() = 0;
    virtual BindingTableModel* newTableModel();

    virtual void addTriggerComponents(class FormPanel* form) = 0;
    virtual void updateBinding(class Binding* b);
    virtual void refreshFields();
	virtual void actionPerformed(void* c);

    virtual List* getRelevantBindings(class BindingConfig* config) = 0;
    virtual class Binding* newBinding() = 0;

    virtual void prepareCommit() = 0;
	virtual void closing();

    bool commit();

  protected:
	
    class BindingDefinition* newBindingDefinition(class Binding* b);
    void addConfigurationSelector(class MessageCatalog* cat, Panel* root);
	NumberField* addNumber(FormPanel* form, class Parameter* p, int min, int max);

    BindingDefinition* getSelectedBinding();

    void getScope(int* retTrack, int *retGroup);
    void setScope(class Binding* b);

    void refreshSelector();
    void refreshBindings();

    class MobiusInterface* mMobius;
	class UI* mUI;
	class MobiusConfig* mConfig;
	class BindingConfig* mBindingConfig;

    class BindingTargets* mTargets;

	ComboBox*		mSelector;
	Button* 		mNew;
	Button* 		mDelete;
	Button* 		mRename;
    Text*           mName;
	ComboBox*		mScope;
    BindingTableModel* mTableModel;
	Table* 		    mBindings;
	Button* 		mNewBinding;
	Button* 		mUpdateBinding;
	Button* 		mDeleteBinding;
    Text*           mArguments;

};

/****************************************************************************
 *                                                                          *
 *                           PLUGIN BINDING DIALOG                          *
 *                                                                          *
 ****************************************************************************/

class PluginBindingDialog : public BindingDialog {
  public:

    PluginBindingDialog(Window* parent, class UI* ui, 
                        class MobiusInterface* mobius, 
                        class MobiusConfig* config);
	~PluginBindingDialog();

    const char* getDialogTitle();
    const char* getBindingsPanelLabel();
    bool isMultipleConfigurations();
    bool isUpdateButton();

    void addTriggerComponents(FormPanel* form);
    void updateBinding(Binding* b);
    void prepareCommit();

    List* getRelevantBindings(class BindingConfig* config);
    class Binding* newBinding();

  private:

    class Binding* getBinding(class Binding* list, class Binding* src);

};

/****************************************************************************
 *                                                                          *
 *                           BUTTON BINDING DIALOG                          *
 *                                                                          *
 ****************************************************************************/

class ButtonBindingDialog : public BindingDialog {
  public:

    ButtonBindingDialog(Window* parent, class UI* ui, 
                        class MobiusInterface* mobius, 
                        class MobiusConfig* config);
	~ButtonBindingDialog();

    const char* getDialogTitle();
    const char* getBindingsPanelLabel();
    bool isMultipleConfigurations();
    bool isUpdateButton();

    void addTriggerComponents(FormPanel* form);
    void updateBinding(Binding* b);
    void prepareCommit();

    List* getRelevantBindings(class BindingConfig* config);
    class Binding* newBinding();

  private:

    class Binding* getBinding(class Binding* list, class Binding* src);

};

/****************************************************************************
 *                                                                          *
 *                             KEY BINDING DIALOG                           *
 *                                                                          *
 ****************************************************************************/

class KeyBindingDialog : public BindingDialog, public KeyListener 
{
  public:

    KeyBindingDialog(Window* parent, class UI* ui, 
                     class MobiusInterface* mobius, 
                     class MobiusConfig* config);
	~KeyBindingDialog();

	void keyPressed(class KeyEvent* e);
	void keyReleased(class KeyEvent* e);

    const char* getDialogTitle();
    const char* getBindingsPanelLabel();
    bool isMultipleConfigurations();
    bool isUpdateButton();
    BindingTableModel* newTableModel();

    void addTriggerComponents(FormPanel* form);
    void updateBinding(Binding* b);
    void refreshFields();
    void actionPerformed(void* c);

    List* getRelevantBindings(class BindingConfig* config);
    class Binding* newBinding();

    void prepareCommit();

  private:

    class Binding* getBinding(class Binding* list, class Binding* src);

	Checkbox* 	mKeyCapture;
	Text* 		mKey;

};

/****************************************************************************
 *                                                                          *
 *                            MIDI BINDING DIALOG                           *
 *                                                                          *
 ****************************************************************************/

/**
 * Defined in MidiBindingDialog but also needed by EDPDialog and 
 * maybe others.
 */
extern const char* MidiChannelNames[];

class MidiBindingDialog : public BindingDialog, public UIMidiEventListener {

  public:

    MidiBindingDialog(Window* parent, class UI* ui, MobiusInterface* mobius, 
                      class MobiusConfig* config);
	~MidiBindingDialog();

	bool midiEvent(class MidiEvent* e);

    const char* getDialogTitle();
    const char* getBindingsPanelLabel();
    const char* getSelectorLabel();
    bool isMultipleConfigurations();
    bool isUpdateButton();
    BindingTableModel* newTableModel();

    void addTriggerComponents(FormPanel* form);
    void updateBinding(Binding* b);
    void refreshFields() ;
    void actionPerformed(void* c);

    List* getRelevantBindings(class BindingConfig* config);
    class Binding* newBinding();

    void prepareCommit();
    void closing();

  private:

    Trigger* getSelectedTrigger();
    int getTriggerIndex(Trigger* trig);
    class Binding* getBinding(class Binding* list, class Binding* src);
	void renderMidi(class MidiEvent* e, char* buffer);

	UIMidiEventListener *mSaveListener;

	ComboBox* 		mTrigger;
	ComboBox*	 	mChannel;
	NumberField* 	mValue;
	Checkbox* 		mMidiCapture;
	Text* 			mMidiDisplay;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
