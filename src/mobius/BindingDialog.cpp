/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Generic dialog for binding.
 * This is subclassed for specific binding types like MIDI and key 
 * bindings, so while the Binding model is generic the dialogs
 * will be taylored for each triggers.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <vector>
#include <algorithm>

#include "Util.h"
#include "Trace.h"
#include "MessageCatalog.h"
#include "Qwin.h"

#include "MidiByte.h"
#include "MidiEvent.h"

#include "Action.h"
#include "Messages.h"
#include "MobiusConfig.h"
#include "Function.h"
#include "Event.h"
#include "Binding.h"
#include "Parameter.h"
#include "Preset.h"
#include "Setup.h"
#include "UI.h"

#include "BindingDialog.h"

// sigh...nested modal dialogs don't work on Mac
//#define USE_RENAME_DIALOG 1

// so we can sort
using namespace std;

//////////////////////////////////////////////////////////////////////
//
// BindingTarget
//
//////////////////////////////////////////////////////////////////////

/**
 * Helper class used to maintain an ordered list of parameters.
 */
class BindingTarget {
  public:

    Target* target;
    Function* function;
    Parameter* parameter;
    UIControl* uiControl;
    Bindable* bindable;
    const char* name;
    char displayName[128];

    BindingTarget(Function* f) {
        target = TargetFunction;
        function = f;
        name = f->getName();
        sprintf(displayName, "%s", f->getDisplayName());
    }

    BindingTarget(Parameter* p) {
        target = TargetParameter;
        parameter = p;
        name = p->getName();
        sprintf(displayName, "%s", p->getDisplayName());
    }

    BindingTarget(UIControl* c) {
        target = TargetUIControl;
        uiControl = c;
        name = c->getName();
        sprintf(displayName, "%s", c->getDisplayName());
    }

    BindingTarget(Target* t, Bindable* b, const char* prefix) {
        target = t;
        bindable = b;
        name = b->getName();
        sprintf(displayName, "%s:%s", prefix, b->getName());
    }

};

/**
 * Return whether first element is greater than the second.
 */
bool BIgreater (BindingTarget* o1, BindingTarget* o2)
{
    return (strcmp(o1->displayName, o2->displayName) < 0);
}

//////////////////////////////////////////////////////////////////////
//
// BindingTargetList
//
//////////////////////////////////////////////////////////////////////

class BindingTargetList {
  public:

    BindingTargetList(Target* target);
    ~BindingTargetList();

    Target* getTarget() {
        return mTarget;
    }

    void setScript(bool b) {
        mScript = b;
    }

    bool isScript() {
        return mScript;
    }

    void setControl(bool b) {
        mControl = b;
    }

    bool isControl() {
        return mControl;
    }

    std::vector<BindingTarget*>* getBindingTargets(MobiusInterface* mobius);

    BindingTarget* getBindingTarget(int index);

    int getIndex(Target* t, const char* name);

  private:

    void init();

    Target* mTarget;
    bool mScript;
    bool mControl;
    vector<BindingTarget*>* mElements;

};

void BindingTargetList::init()
{
    mTarget = NULL;
    mScript = false;
    mControl = false;
    mElements = NULL;
}

BindingTargetList::BindingTargetList(Target* target) 
{
    init();
    mTarget = target;
}

BindingTargetList::~BindingTargetList()
{
    vector<BindingTarget*>::iterator it;
    for (it = mElements->begin() ; it != mElements->end() ; it++) {
        BindingTarget* el = *it;
        delete el;
    }
    delete mElements;
}

BindingTarget* BindingTargetList::getBindingTarget(int index)
{
    BindingTarget* t = NULL;
    if (mElements != NULL && index >= 0 && (unsigned int)index < mElements->size())
      t = mElements->at(index);
    return t;
}

int BindingTargetList::getIndex(Target* target, const char* name)
{
    int index = -1;

    for (unsigned int i = 0 ; i < mElements->size() ; i++) {
        BindingTarget* t = mElements->at(i);
        if (t->target == target && StringEqual(name, t->name)) {
            index = i;
            break;
        }
    }
    return index;
}

vector<BindingTarget*>* BindingTargetList::getBindingTargets(MobiusInterface* mobius)
{
    if (mElements == NULL) {
        vector<BindingTarget*>* v = new vector<BindingTarget*>;

        if (mTarget == TargetFunction && !mScript) {
            Function** functions = mobius->getFunctions();
            for (int i =  0 ; functions[i] != NULL ; i++) {
                Function* f = functions[i];
                if (f->eventType != RunScriptEvent && !f->scriptOnly)
                  v->push_back(new BindingTarget(f));
            }

            // assume for now these all fit with functions, don't want
            // to waste a tab on them
            UIControl** uicontrols = mobius->getUIControls();
            if (uicontrols != NULL) {
                for (int i =  0 ; uicontrols[i] != NULL ; i++) {
                    UIControl* c = uicontrols[i];
                    v->push_back(new BindingTarget(c));
                }
            }
        }
        else if (mTarget == TargetFunction && mScript) {
            Function** functions = mobius->getFunctions();
            for (int i =  0 ; functions[i] != NULL ; i++) {
                Function* f = functions[i];
                if (f->eventType == RunScriptEvent)
                  v->push_back(new BindingTarget(f));
            }
        }
        else if (mTarget == TargetParameter && mControl) {
            Parameter** parameters = mobius->getParameters();
            for (int i = 0 ; parameters[i] != NULL ; i++) {
                Parameter* p = parameters[i];
                if (p->bindable && p->control)
                  v->push_back(new BindingTarget(p));
            }
        }
        else if (mTarget == TargetParameter && !mControl) {
            Parameter** parameters = mobius->getParameters();
            for (int i = 0 ; parameters[i] != NULL ; i++) {
                Parameter* p = parameters[i];
                if (p->bindable && !p->control) 
                  v->push_back(new BindingTarget(p));
            }
        }
        else {
            // config objects
            // could find a way to get the prefix key from the Bindable?
            MessageCatalog* catalog = mobius->getMessageCatalog();
            // note that the owning BindignDialog will be managing
            // a clone of the MobiusConfig but we can't get that here,
            // it's okay since this dialog won't be modifying the list
            // of config objects
            MobiusConfig* config = mobius->getConfiguration();

            const char* prefix = catalog->get(MSG_WORD_PRESET);
            for (Preset* preset = config->getPresets() ; preset != NULL ; 
                 preset = preset->getNext()) {
                v->push_back(new BindingTarget(TargetPreset, preset, prefix));
            }

            prefix = catalog->get(MSG_WORD_SETUP);
            for (Setup* setup = config->getSetups() ; setup != NULL ; 
                 setup = setup->getNext()) {
                v->push_back(new BindingTarget(TargetSetup, setup, prefix));
            }

            // first cannot be selected, only the secondary ones
            BindingConfig* bconfigs = config->getBindingConfigs();
            if (bconfigs != NULL) {
                prefix = "Bindings";
                // NOTE: We're not showing the first "Common Bindigns" but
                // without that there is no way to cancel a binding overlay
                // once selected.  Having a binding for "Common Bindings"
                // is confusing because it makes it look like it is 
                // optional.
                for (BindingConfig* bconfig = bconfigs->getNext() ; 
                     bconfig != NULL ; bconfig = bconfig->getNext()) {
                    v->push_back(new BindingTarget(TargetBindings, bconfig, prefix));
                }
            }
        }

        sort(v->begin(), v->end(), BIgreater);

        mElements = v;
    }

    return mElements;
}

//////////////////////////////////////////////////////////////////////
//
// BindingTargets
//
//////////////////////////////////////////////////////////////////////

/**
 * This encapsulates a tabbed pane that displays all the possible
 * binding targets, and provides methods to select one and retrive
 * the current selection.
 */
class BindingTargets {
  public:

    BindingTargets();
    ~BindingTargets();

    TabbedPane* getTabbedPane(MobiusInterface* mobius);

    BindingTarget* getSelectedTarget();
    void setSelectedTarget(Target* t, const char* name);

  private:

    BindingTargetList* getList(int index);
    ListBox* getListBox(int index);
    bool setSelectedTarget(int tab, Target* t, const char* name);

    TabbedPane* mTabs;
    List* mLists;

};

BindingTargets::BindingTargets()
{
    mTabs = NULL;
    mLists = NULL;
}

BindingTargets::~BindingTargets()
{
    if (mLists != NULL) {
        for (int i = 0 ; i < mLists->size() ; i++) {
            BindingTargetList* l = (BindingTargetList*)(mLists->get(i));
            delete l;
        }
        delete mLists;
    }
}

TabbedPane* BindingTargets::getTabbedPane(MobiusInterface* mobius)
{
    if (mTabs == NULL) {
        mTabs = new TabbedPane();
        mLists = new List();

        // non-script functions
        mLists->add(new BindingTargetList(TargetFunction));

        // script functions
        BindingTargetList* btl = new BindingTargetList(TargetFunction);
        btl->setScript(true);
        mLists->add(btl);

        // control parameters
        btl = new BindingTargetList(TargetParameter);
        btl->setControl(true);
        mLists->add(btl);

        // non-control parameters
        mLists->add(new BindingTargetList(TargetParameter));

        // config objects
        mLists->add(new BindingTargetList(NULL));

        for (int i = 0 ; i < mLists->size() ; i++) {
            BindingTargetList* list = (BindingTargetList*)(mLists->get(i));
            
            const char* name = NULL;
            Target* target = list->getTarget();

            if (target == TargetFunction) {
                if (list->isScript())
                  name = "Scripts";
                else
                  name = "Functions";
            }
            else if (target == TargetParameter) {
                if (list->isControl())
                  name = "Controls";
                else
                  name = "Parameters";
            }
            else
              name = "Configurations";

            Panel* p = new Panel();
            p->setName(name);
            p->setLayout(new BorderLayout());

            ListBox* lbox = new ListBox();
            lbox->setColumns(15);
            lbox->setRows(10);

            std::vector<BindingTarget*>* elements = 
                list->getBindingTargets(mobius);

            std::vector<BindingTarget*>::iterator it;
            for (it = elements->begin() ; it != elements->end() ; it++) {
                BindingTarget* el = *it;
                lbox->addValue(el->displayName);
            }

            lbox->clearSelection();

            p->add(lbox, BORDER_LAYOUT_CENTER);

            mTabs->add(p);
        }
    }

    return mTabs;
}

/**
 * Get one of the binding target lists by index.
 */
BindingTargetList* BindingTargets::getList(int index)
{
    return (BindingTargetList*)(mLists->get(index));
}

/**
 * Get the Listbox component used to display one of hte binding lists.
 */
ListBox* BindingTargets::getListBox(int index)
{
    ListBox* box = NULL;
    if (mTabs != NULL) {
        Panel* p = (Panel*)(mTabs->getComponent(index));
        if (p != NULL) {
            // always the first child
            box = (ListBox*)(p->getComponents());
        }
    }
    return box;
}

/**
 * Return the currently selected BindingTarget.
 * This has some assumptiona about how the TabbedPane was built.
 */
BindingTarget* BindingTargets::getSelectedTarget()
{
    BindingTarget* target = NULL;
    if (mTabs != NULL) {
        int index = mTabs->getSelectedIndex();
        BindingTargetList* list = getList(index);
        if (list != NULL) {
            ListBox* box = getListBox(index);
            if (box != NULL)
              target = list->getBindingTarget(box->getSelectedIndex());
        }
    }
    return target;
}

/**
 * This one is ugly because we've got to remember the order of the
 * BindingTargetLists created by getTabbedPane.
 */
void BindingTargets::setSelectedTarget(Target* t, const char* name)
{
    if (mTabs != NULL && mLists != NULL) {

        if (t == TargetFunction || t == TargetUIControl) {
            // first one functions, second scripts
            if (!setSelectedTarget(0, t, name)) {
                if (!setSelectedTarget(1, t, name))
                  Trace(1, "Unable to find function target: %s\n", name);
            }
        }
        else if (t == TargetParameter) {
            if (!setSelectedTarget(2, t, name)) {
                if (!setSelectedTarget(3, t, name))
                  Trace(1, "Unable to find parameter target: %s\n", name);
            }
        }
        else {
            if (!setSelectedTarget(4, t, name))
              Trace(1, "Unable to find configuration target: %s\n", name);
        }
    }
}

bool BindingTargets::setSelectedTarget(int tab, Target* t, const char* name)
{
    bool found = false;
    BindingTargetList* list = getList(tab);
    int index = list->getIndex(t, name);
    if (index >= 0) {
        ListBox* box = getListBox(tab);
        box->setSelectedIndex(index);
        mTabs->setSelectedIndex(tab);
        mTabs->invalidate();
        found = true;
    }
    return found;
}

//////////////////////////////////////////////////////////////////////
//
// BindingDefinition
//
//////////////////////////////////////////////////////////////////////

BindingDefinition::BindingDefinition(Binding* b)
{
    mName = NULL;
    mBinding = b;
    mResolvedTarget = NULL;
}

BindingDefinition::~BindingDefinition()
{
    // mResolvedTarget is interned
    delete mName;
}

Binding* BindingDefinition::getBinding()
{
    return mBinding;
}

const char* BindingDefinition::getName()
{
    return mName;
}

void BindingDefinition::setName(const char* s)
{
    delete mName;
    mName = CopyString(s);
}

/**
 * Calculate derived fields after a change to the wrapped Binding.
 *
 * We need a Mobius here to resolve references to UIControls and
 * the *edited* MobiusConfig being maintained by BindingDialog.
 *
 * Also take the opportunity to upgrade old aliased names to the
 * official name, otherwise setSelectedTarget won't work.
 */
void BindingDefinition::refresh(MobiusInterface* mobius, MobiusConfig* config)
{
    mResolvedTarget = NULL;

    if (mBinding == NULL) {
        setName(NULL);
    }
    else {
        // resolution machinery is built in here
        mResolvedTarget = mobius->resolveTarget(mBinding);

        char buffer[128];

        // could be using mResolvedTarget for this
        strcpy(buffer, "");
        int track = mBinding->getTrack();
        int group = mBinding->getGroup();

        if (track > 0)
          sprintf(buffer, "%d ", track);
        else if (group > 0)
          sprintf(buffer, "%c ", (char)('A' + (group - 1)));

        const char* name;
        const char* type;

        if (mResolvedTarget != NULL) {
            name = mResolvedTarget->getDisplayName();
            type = mResolvedTarget->getTypeDisplayName();

            // upgrade old alias names
            if (!StringEqual(mResolvedTarget->getName(), mBinding->getName())) {
                //printf("Upgrading target name %s to %s\n", 
                //mBinding->getName(), mResolvedTarget->getName());
                mBinding->setName(mResolvedTarget->getName());
            }
        }
        else {
            // should have weeded these out by now
            Target* target = mBinding->getTarget();
            if (target != NULL)
              type = target->getDisplayName();
            name = mBinding->getName();
        }

        if (type != NULL) {
            strcat(buffer, type);
            strcat(buffer, ":");
        }

        if (name != NULL)
          strcat(buffer, name);
        
        if (mResolvedTarget == NULL)
          strcat(buffer, " UNRESOLVED!");

        setName(buffer);
    }
}

//////////////////////////////////////////////////////////////////////
//
// BindingTableModel
//
//////////////////////////////////////////////////////////////////////

BindingTableModel::BindingTableModel()
{
    mBindings = NULL;
}

BindingTableModel::~BindingTableModel()
{
    clear();
}

void BindingTableModel::clear()
{
    if (mBindings != NULL) {
        for (int i = 0 ; i < mBindings->size() ; i++) {
            BindingDefinition* def = (BindingDefinition*)(mBindings->get(i));
            delete def;
        }
        delete mBindings;
    }
    mBindings = NULL;
}

/**
 * We own the list and the BindingDefinitions but not the
 * wrapped Bindings.
 */
void BindingTableModel::setBindings(List* l)
{
    if (mBindings != l) {
        clear();
        mBindings = l;
        sort();
    }
}

List* BindingTableModel::getBindings()
{
    return mBindings;
}

void BindingTableModel::addBinding(BindingDefinition* def)
{
    if (mBindings == NULL)
      mBindings = new List();
    mBindings->add(def);
    sort();
}

void BindingTableModel::removeBinding(BindingDefinition* def)
{
    if (mBindings != NULL)
      mBindings->remove(def);
}

int BindingTableModel::getRowCount()
{
    return (mBindings != NULL) ? mBindings->size() : 0;
}

int BindingTableModel::getColumnCount()
{
    return 2;
}

int BindingTableModel::getColumnPreferredWidth(int index)
{
    return (index == 0) ? 30 : 20;
}

const char* BindingTableModel::getColumnName(int index)
{
    return (index == 0) ? "Target" : "Arguments";
}

/**
 * Default model only has one two columns for name and args.
 * Subclasses overload this to add other columns.
 */
const char* BindingTableModel::getCellText(int row, int column)
{
    const char* text = NULL;
    if (mBindings != NULL) {
        BindingDefinition* def = (BindingDefinition*)(mBindings->get(row));
        if (def != NULL) {
            if (column == 0) {
                text = def->getName();
            }
            else {
                Binding* b = def->getBinding();
                text = b->getArgs();
            }
        }
    }
    return text;
}

/**
 * std sorter definition for BindingDefinition.
 * Return whether first element is greater than the second.
 */
bool BindingDefinitionSorter(BindingDefinition* o1, BindingDefinition* o2)
{
    bool greater = false;

    BindingDefinition* def1 = (BindingDefinition*)o1;
    BindingDefinition* def2 = (BindingDefinition*)o2;
    
    // TODO: other options for sorting
    const char* name1 = def1->getName();
    const char* name2 = def2->getName();

    if (name1 != NULL && name2 != NULL)
      greater = (strcmp(name1, name2) < 0);

    return greater;
}

/**
 * Sort the binding list to make it easier to see things
 * in long lists.  We're sorting on the consolodated name that includes
 * both the type prefix and the scope prefix.  Allow an option to 
 * sort only by target name?
 */
PRIVATE void BindingTableModel::sort()
{
    if (mBindings != NULL && mBindings->size() > 0) {

        vector<BindingDefinition*>* v = new vector<BindingDefinition*>;
        for (int i = 0 ; i < mBindings->size() ; i++) {
            BindingDefinition* def = (BindingDefinition*)(mBindings->get(i));
            v->push_back(def);
        }

        ::sort(v->begin(), v->end(), BindingDefinitionSorter);

        // now rebuild the list from the sorted vector
        List* sorted = new List();
        vector<BindingDefinition*>::iterator it;
        for (it = v->begin() ; it != v->end() ; it++)
          sorted->add(*it);
        delete mBindings;
        mBindings = sorted;
    }
}

/**
 * Locate the index posistion of a Binding in the model.
 * This is used after we've added a new binding so we can mark it
 * as selected in the table.  
 */
PUBLIC int BindingTableModel::getIndex(Binding* b)
{
    int index = -1;
    if (mBindings != NULL) {
        for (int i = 0 ; i < mBindings->size() ; i++) {
            BindingDefinition* def = (BindingDefinition*)mBindings->get(i);
            Binding* b2 = def->getBinding();

            // should have this as a util method in Binding?
            if (b->getTrigger() == b2->getTrigger() &&
                b->getValue() == b2->getValue() &&
                b->getChannel() == b2->getChannel() &&
                b->getTarget() == b2->getTarget() &&
                StringEqual(b->getName(), b2->getName()) &&
                StringEqual(b->getArgs(), b2->getArgs()) &&
                b->getTrack() == b2->getTrack() &&
                b->getGroup() == b2->getGroup()) {
                index = i;
                break;
            }
        }
    }
    return index;
}

//////////////////////////////////////////////////////////////////////
//
// BindingDialog
//
//////////////////////////////////////////////////////////////////////

PUBLIC BindingDialog::BindingDialog()
{
	mUI = NULL;
    mMobius = NULL;
    mConfig = NULL;
    mBindingConfig = NULL;
    mTargets = NULL;
    mSelector = NULL;
    mNew = NULL;
    mDelete = NULL;
    mRename = NULL;
    mName = NULL;
    mScope = NULL;
    mTableModel = NULL;
    mBindings = NULL;
    mNewBinding = NULL;
    mUpdateBinding = NULL;
    mDeleteBinding = NULL;
    mArguments = NULL;
}

PUBLIC BindingDialog::~BindingDialog()
{
    // mConfig is owned by the caller
    delete mTargets;
}

/**
 * !! The only reason we need UI is for the redirection
 * of MIDI events, find a better way!
 */
PUBLIC void BindingDialog::init(Window* parent, 
                                UI* ui,
                                MobiusInterface* mobius,
                                MobiusConfig* config)
{

    // this is only necessary for MidiBindingDialog that wants to 
    // overload the MIDI listener
    mUI = ui;
    mMobius = mobius;
    mConfig = config;

	setParent(parent);
	setModal(true);
	setIcon("Mobius");
	setTitle(getDialogTitle());
	setInsets(20, 20, 20, 0);

    // the concept of base/overlay isn't done very well yet, 
    // need to be able to select things from the selector for editing
    // but not necessarily make them then active overlay!!

    if (isMultipleConfigurations())
      mBindingConfig = mConfig->getOverlayBindingConfig();

	if (mBindingConfig == NULL) {
        mBindingConfig = mConfig->getBaseBindingConfig();
        if (mBindingConfig == NULL) {
            mBindingConfig = new BindingConfig();
            mConfig->addBindingConfig(mBindingConfig);
        }
    }
    mConfig->generateNames();

	Panel* root = getPanel();
	VerticalLayout* vl = new VerticalLayout();
	vl->setCenterX(true);
	root->setLayout(vl);

    MessageCatalog* cat = mMobius->getMessageCatalog();

    // optional configuration selector, override in subclasses
    if (isMultipleConfigurations())
      addConfigurationSelector(cat, root);

    // bindings on the left, targets and triggers on the right
    Panel* main = new Panel();
    root->add(main);
    main->setLayout(new HorizontalLayout(12));

	Panel* bindings = new Panel();
	main->add(bindings);
	bindings->setLayout(new VerticalLayout());

    const char* label = getBindingsPanelLabel();
    if (label == NULL)
      label = cat->get(MSG_DLG_MIDI_BINDINGS);

    bindings->add(new Label(label));
    mTableModel = newTableModel();
	mBindings = new Table(mTableModel);
    // this determines the height of the dialog, if it isn't
    // tall enough to fit the entire scope selector, then the scope
    // selector scrolls which is confusing, 20 is too small on Windows,
	// on Mac it is plenty big
    mBindings->setVisibleRows(25);
	mBindings->addActionListener(this);
	bindings->add(mBindings);
    
	Panel* actions = new Panel();
	bindings->add(actions);
	actions->setLayout(new HorizontalLayout(10));
	mNewBinding = new Button(cat->get(MSG_DLG_NEW));
	mNewBinding->setFont(Font::getFont("Arial", 0, 8));
	mNewBinding->addActionListener(this);
	actions->add(mNewBinding);
    if (isUpdateButton()) {
        mUpdateBinding = new Button("Update");
        mUpdateBinding->setFont(Font::getFont("Arial", 0, 8));
        mUpdateBinding->addActionListener(this);
        actions->add(mUpdateBinding);
    }
	mDeleteBinding = new Button(cat->get(MSG_DLG_DELETE));
	mDeleteBinding->setFont(Font::getFont("Arial", 0, 8));
	mDeleteBinding->addActionListener(this);
	actions->add(mDeleteBinding);

    // targets on top, triggers on the bottom
    Panel* targtrig = new Panel();
    main->add(targtrig);
    targtrig->setLayout(new VerticalLayout());

    mTargets = new BindingTargets();
    targtrig->add(mTargets->getTabbedPane(mMobius));

    // triggers
	targtrig->add(new Strut(0, 10));

	FormPanel* triggers = new FormPanel();
	targtrig->add(triggers);

    // scope
    StringList* scopes = new StringList();
    scopes->add("Global");
    for (int i = 0 ; i < mConfig->getTracks() ; i++) {
        char buf[128];
        sprintf(buf, "Track %d", i+1);
        scopes->add(buf);
    }
    for (int i = 0 ; i < mConfig->getTrackGroups() ; i++) {
        char buf[128];
        sprintf(buf, "Group %c", 'A' + i);
        scopes->add(buf);
    }

    mScope = new ComboBox(scopes);
	mScope->setSelectedIndex(0);
	// make it the same size as mType
	mScope->setColumns(7);
    triggers->add(cat->get(MSG_DLG_MIDI_CONTROL_SCOPE), mScope);

    // let the subclass add some
    addTriggerComponents(triggers);

    // then the common stuff
    mArguments = new Text();
    mArguments->setColumns(15);
    triggers->add(cat->get(MSG_DLG_BINDING_ARGUMENTS), mArguments);

    refreshSelector();
    refreshBindings();
	refreshFields();
}

/**
 * May be overloaded to build different table models with different columns.
 */
PUBLIC BindingTableModel* BindingDialog::newTableModel()
{
    return new BindingTableModel();
}

PUBLIC const char* BindingDialog::getSelectorLabel()
{
    // shouldn't evern user this, subclasses that have multiple
    // configurations will overload this method
    return "Selected Configuration";
}

/**
 * Called by subclasses to create a BindingDefinition for a Binding
 * they select from the configuration.
 */
PUBLIC BindingDefinition* BindingDialog::newBindingDefinition(Binding* b)
{
    BindingDefinition* def = new BindingDefinition(b);
    def->refresh(mMobius, mConfig);
    return def;
}

/**
 * If enabled, add a set of configuration selectors at the top.
 */
PUBLIC void BindingDialog::addConfigurationSelector(MessageCatalog* cat,
                                                    Panel* root)

{
	FormPanel* form = new FormPanel();
	form->setAlign(FORM_LAYOUT_RIGHT);
	root->add(form);

	Panel* p = new Panel();
	p->setLayout(new HorizontalLayout(10));
	mSelector = new ComboBox();
	mSelector->setColumns(20);
	mSelector->addActionListener(this);
	p->add(mSelector);
	mNew = new Button(cat->get(MSG_DLG_NEW));
	mNew->setFont(Font::getFont("Arial", 0, 8));
	mNew->addActionListener(this);
	p->add(mNew);
	mDelete = new Button(cat->get(MSG_DLG_DELETE));
	mDelete->setFont(Font::getFont("Arial", 0, 8));
	mDelete->addActionListener(this);
	p->add(mDelete);
	mRename = new Button(cat->get(MSG_DLG_RENAME));
	mRename->setFont(Font::getFont("Arial", 0, 8));
	mRename->addActionListener(this);
#ifdef USE_RENAME_DIALOG
	p->add(mRename);
#endif
	form->add(getSelectorLabel(), p);

#ifndef USE_RENAME_DIALOG
	p = new Panel();
	p->setLayout(new HorizontalLayout(8));
    mName = new Text();
	mName->addActionListener(this);
    p->add(mName);
    p->add(mRename);
    form->add(cat->get(MSG_DLG_NAME), p);
#endif

	root->add(new Strut(0, 10));
	root->add(new Divider(500));
	root->add(new Strut(0, 10));
}

PRIVATE NumberField* BindingDialog::addNumber(FormPanel* form, 
												  Parameter* p, 
												  int min, int max)
{
	return form->addNumber(this, p->getDisplayName(), min, max);
}

//////////////////////////////////////////////////////////////////////
//  
// Refresh
//
//////////////////////////////////////////////////////////////////////

/**
 * Return the currently selected binding.
 */
PRIVATE BindingDefinition* BindingDialog::getSelectedBinding()
{
	BindingDefinition* def = NULL;

    int index = mBindings->getSelectedIndex();
    if (index >= 0) {
        List* bindings = mTableModel->getBindings();
        if (bindings != NULL)
          def = (BindingDefinition*)(bindings->get(index));
    }
    return def;
}

/**
 * Update the currently selected binding based on the current
 * values of the editing fields.
 * 
 * This may be overloaded in the subclass if it added extra
 * things to the trigger form.  If so it must call up here
 * to handle the common updates.
 */
PRIVATE void BindingDialog::updateBinding(Binding* b)
{
    // scope
    int track, group;
    getScope(&track, &group);
    b->setTrack(track);
    b->setGroup(group);

    // arguments
    if (mArguments != NULL)
      b->setArgs(mArguments->getValue());

    // target
    BindingTarget* bt = mTargets->getSelectedTarget();
    if (bt != NULL) {
        b->setTarget(bt->target);
        b->setName(bt->name);
    }

}

/**
 * Convert the scope selector into a track or group index.
 */
PRIVATE void BindingDialog::getScope(int* retTrack, int *retGroup)
{
	int track = 0;
	int group = 0;

    if (mScope != NULL) {
        int index = mScope->getSelectedIndex();
        if (index > 0) {
            // one for each track plus "Global"
            int firstGroupIndex = mConfig->getTracks() + 1;

            if (index < firstGroupIndex)
              track = index;
            else
              group = (index - firstGroupIndex) + 1;
        }
    }

	*retTrack = track;
	*retGroup = group;
}

/**
 * Convert the track and group options of a binding into an index
 * into the scope selector.
 */
PRIVATE void BindingDialog::setScope(Binding* b)
{
    if (mScope != NULL) {
        int scope = 0;
        if (b != NULL) {
            int track = b->getTrack();
            int group = b->getGroup();

            // one for each track plus "Global"
            int firstGroupIndex = mConfig->getTracks() + 1;

            if (track > 0) {
                // unfortunately the selector isn't dynamic
                // so we can't address anything beyond 8
                if (track >= firstGroupIndex)
                  scope = firstGroupIndex - 1;
                else
                  scope = track;
            }
            else if (group > 0) {
                scope = firstGroupIndex + (group - 1);
            }
        }
        mScope->setSelectedIndex(scope);
    }
}

/**
 * Initialize a combo box for selecting configurations.
 * Name them if they don't already have names.
 */
PRIVATE void BindingDialog::refreshSelector()
{
    if (mSelector != NULL) {
        mConfig->generateNames();
        mSelector->setValues(NULL);
        for (BindingConfig* p = mConfig->getBindingConfigs() ; 
             p != NULL ; p = p->getNext())
          mSelector->addValue(p->getName());

        mSelector->setSelectedValue(mBindingConfig->getName());
    }
}

/**
 * Refresh editing fields to reflect the currently selected binding.
 * May be overloaded in the subclass to refresh binding-specific fields,
 * if so it must call back up to this one.
 */
PRIVATE void BindingDialog::refreshFields()
{
    if (mName != NULL)
      mName->setValue(mBindingConfig->getName());

    BindingDefinition* def = getSelectedBinding();
    Binding* b = (def != NULL) ? def->getBinding() : NULL;

	if (mTargets != NULL) {
		if (b != NULL)
		  mTargets->setSelectedTarget(b->getTarget(), b->getName());
		else {
			// hmm, force it back to the first tab or just
			// leave where it is?
		}
	}

    if (mArguments != NULL) {
        if (b != NULL)
          mArguments->setValue(b->getArgs());
        else
          mArguments->setValue(NULL);
    }

    setScope(b);
}

/**
 * Refresh the binding table.
 */
PRIVATE void BindingDialog::refreshBindings()
{
    // overloaded in the subclass
    List* bindings = getRelevantBindings(mBindingConfig);
    mTableModel->setBindings(bindings);

    // force a rebuild to reflect changes made to the model
    mBindings->rebuild();

    // need this too?
    mBindings->invalidate();
}

//////////////////////////////////////////////////////////////////////
//
// Actions
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by SimpleDialog when the Ok button is pressed.
 */
PUBLIC bool BindingDialog::commit()
{
    // subclass overload 
    prepareCommit();
    return true;
}

PUBLIC void BindingDialog::closing()
{
	SimpleDialog::closing();
}

PUBLIC void BindingDialog::actionPerformed(void* c)
{
	if (c == mNew) {
        // Other config objects start with a clone of the current one,
        // but here we have a base+overlay and we never want the overlays
        // to start with the base bindings.  Maybe we should still
        // clone if we're creating an overlay from another overlay?
		//BindingConfig* neu = mBindingConfig->clone();
		BindingConfig* neu = new BindingConfig();
		// null the name so we generate a new one
		neu->setName(NULL);

		mConfig->addBindingConfig(neu);
        // make it current when committd
		mConfig->setOverlayBindingConfig(neu);
		mBindingConfig = neu;
		mConfig->generateNames();
        refreshSelector();
        refreshBindings();
		refreshFields();
	}
	else if (c == mDelete) {
        // The first configuration is the "global" config and cannot
        // be deleted.
		BindingConfig* configs = mConfig->getBindingConfigs();
        if (configs == mBindingConfig) {
            MessageCatalog* cat = mMobius->getMessageCatalog();
			MessageDialog::showError(getParentWindow(), 
									 cat->get(MSG_DLG_ERROR),
                                     "You cannot delete the global binding configuration");

        }
        else {
			BindingConfig* next = mBindingConfig->getNext();
			if (next == NULL) {
				for (next = configs ; 
					 next != NULL && next->getNext() != mBindingConfig ; 
					 next = next->getNext());
				if (next == NULL)
				  next = configs;
			}
			mConfig->removeBindingConfig(mBindingConfig);
			delete mBindingConfig;
            // should we go the next or reset back to the base?
			mConfig->setOverlayBindingConfig(next);
			mBindingConfig = next;
            refreshSelector();
            refreshBindings();
			refreshFields();
		}
	}
	else if (c == mRename) {
        // can't rename the first one
		BindingConfig* configs = mConfig->getBindingConfigs();
        if (mBindingConfig == mConfig->getBindingConfigs()) {
            MessageCatalog* cat = mMobius->getMessageCatalog();
			MessageDialog::showError(getParentWindow(), 
									 cat->get(MSG_DLG_ERROR),
                                     "You cannot rename the global binding configuration");
            mName->setValue(MIDI_COMMON_BINDINGS_NAME);
        }
        else {
#ifdef USE_RENAME_DIALOG
            RenameDialog* d = new RenameDialog(this, "Rename", "Name", 
                                               mBindingConfig->getName());
            d->show();
            if (!d->isCanceled()) {
                const char* newName = d->getValue();
                if (newName != NULL && strlen(newName) > 0) {
                    // should also trim whitespace!
                    if (!StringEqual(mBindingConfig->getName(), newName)) {
                        mBindingConfig->setName(newName);
                        refreshSelector();
                    }
                }
            }
            delete d;
#else
		mBindingConfig->setName(mName->getValue());
		refreshSelector();
#endif
        }
	}
	else if (c == mSelector) {
		BindingConfig* c = mConfig->getBindingConfig(mSelector->getValue());
		if (c != NULL) {
			mBindingConfig = c;
			mConfig->setOverlayBindingConfig(c);
            refreshBindings();
			refreshFields();
		}
	}
	else if (c == mName) {
        // make them click Rename
		//mBindingConfig->setName(mName->getValue());
		//refreshSelector();
	}
	else if (c == mNewBinding) {
		// ignore if target not selected, 
		// should let the subclasses decide if there is enough
		// spefied to create a binding?
		BindingTarget* bt = mTargets->getSelectedTarget();
		if (bt != NULL) {
			// subclass gets to do this to set the trigger
			Binding* b = newBinding();
			updateBinding(b);
			mBindingConfig->addBinding(b);
			refreshBindings();
            // now that we sort we can't assume it will be the last one
            int index = mTableModel->getIndex(b);
			mBindings->setSelectedIndex(index);
		}
	}
	else if (c == mUpdateBinding) {
        BindingDefinition* def = getSelectedBinding();
        if (def != NULL) {
            updateBinding(def->getBinding());
            def->refresh(mMobius, mConfig);
            refreshBindings();
        }
	}
	else if (c == mDeleteBinding) {
        BindingDefinition* def = getSelectedBinding();
        if (def != NULL) {
            Binding* b = def->getBinding();
            mBindingConfig->removeBinding(b);
            // this will get rid of all current BindingDefinitions
            refreshBindings();
            // this is now orhpaned
            delete b;
        }
	}
    else if (c == mBindings) {
        refreshFields();
    }
	else {
		// must be one of the SimpleDialog buttons
		SimpleDialog::actionPerformed(c);
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
