/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A simple wrapper around the built-in Open Dialog
 * Need to add more control.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "QwinExt.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                               SYSTEM DIALOG                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC SystemDialog::SystemDialog(Window* parent)
{
	mParent = parent;
	mTitle = NULL;
	mCanceled = false;
}

PUBLIC SystemDialog::~SystemDialog()
{
	delete mTitle;
}

PUBLIC Window* SystemDialog::getParent()
{
    return mParent;
}

PUBLIC void SystemDialog::setTitle(const char* s) 
{
	delete mTitle;
	mTitle = CopyString(s);
}

PUBLIC const char* SystemDialog::getTitle()
{
    return mTitle;
}

PUBLIC void SystemDialog::setCanceled(bool b)
{
    mCanceled = b;
}

PUBLIC bool SystemDialog::isCanceled()
{
	return mCanceled;
}

/****************************************************************************
 *                                                                          *
 *   							 OPEN DIALOG                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC OpenDialog::OpenDialog(Window* parent) :
    SystemDialog(parent)
{
	mFilter = NULL;
	mFile = NULL;
	mInitialDirectory = NULL;
	mSave = false;
    mAllowDirectories = false;
    mAllowMultiple = false;
}

PUBLIC OpenDialog::~OpenDialog()
{
	delete mFilter;
	delete mFile;
	delete mInitialDirectory;
}

PUBLIC void OpenDialog::setFilter(const char* s)
{
	delete mFilter;
	mFilter = CopyString(s);
}

PUBLIC const char* OpenDialog::getFilter()
{
    return mFilter;
}

PUBLIC void OpenDialog::setInitialDirectory(const char* s)
{
	delete mInitialDirectory;
	mInitialDirectory = CopyString(s);
}

PUBLIC const char* OpenDialog::getInitialDirectory()
{
    return mInitialDirectory;
}

PUBLIC void OpenDialog::setFile(const char* s)
{
	delete mFile;	
	mFile = CopyString(s);
}

PUBLIC const char* OpenDialog::getFile()
{
	return mFile;
}

/**
 * When this is true, we open a "save" dialog rather than an "open" dialog.
 */
PUBLIC void OpenDialog::setSave(bool b)
{
	mSave = b;
}

PUBLIC bool OpenDialog::isSave()
{
    return mSave;
}

PUBLIC void OpenDialog::setAllowDirectories(bool b)
{
	mAllowDirectories = b;
}

PUBLIC bool OpenDialog::isAllowDirectories()
{
    return mAllowDirectories;
}

PUBLIC void OpenDialog::setAllowMultiple(bool b)
{
	mAllowMultiple = b;
}

PUBLIC bool OpenDialog::isAllowMultiple()
{
    return mAllowMultiple;
}

PUBLIC bool OpenDialog::show()
{
	mCanceled = false;

    SystemDialogUI* ui = UIManager::getOpenDialogUI(this);

    // this will turn around and call our setFile and setCanceled methods,
    // don't really like that but it isn't worth duplicating
    ui->show();
    delete ui;

	return !mCanceled;
}

QWIN_END_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   							   WINDOWS                                  *
 *                                                                          *
 ****************************************************************************/

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include "UIWindows.h"
QWIN_BEGIN_NAMESPACE

PUBLIC WindowsOpenDialog::WindowsOpenDialog(OpenDialog* d)
{
	mDialog = d;
}

PUBLIC WindowsOpenDialog::~WindowsOpenDialog()
{
}

/**
 * Unlike Mac, a windows open dialog cannot select both files and
 * directories, you hav to make different dialogs.   One post
 * suggested using a callback to track CDN_FOLDERCHANGE or th OK button
 * and send CDM_GETFOLDERPATH but I'm not sure how to close th dialog?
 */
PUBLIC void WindowsOpenDialog::show()
{
    HWND parent = NULL;
    Window* pw = mDialog->getParent();
    if (pw != NULL)
      parent = WindowsComponent::getHandle(pw);

    boolean useFolderBrowser = mDialog->isAllowDirectories();

    if (useFolderBrowser) {
        BROWSEINFO bi;
        char name[MAX_PATH + 8];

        ZeroMemory(&bi, sizeof(BROWSEINFO));
        bi.hwndOwner = parent;
        bi.pszDisplayName = name;
        bi.lpszTitle = mDialog->getTitle();

        // BIF_NEWDIALOGSTYLE will return a resizeable window, but
        // we need the new platform SDK headers...
        bi.ulFlags = BIF_RETURNONLYFSDIRS;
        
        // oh GOD I hate COM
        CoInitialize(NULL);
        LPITEMIDLIST items = SHBrowseForFolder(&bi);

        if (items == NULL) {
            mDialog->setCanceled(true);
        }
        else {
            // name currently has the "display name" which is just
            // the leaf name, have to get the full path from the item list
            SHGetPathFromIDList(items, name);
            mDialog->setFile(name);
        }

        // supposedly we're supposed to free the LPITEMIDLIST but
        // fuck if I know how
    }
    else {
        bool status = false;
        char filebuf[1024];
        char filterbuf[1024];
        OPENFILENAME ofn;

        filebuf[0] = 0;
        filterbuf[0] = 0;

        ZeroMemory(&ofn, sizeof(OPENFILENAME));
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = parent;
        ofn.lpstrTitle = mDialog->getTitle();
        ofn.lpstrFile = filebuf;
        ofn.nMaxFile = sizeof(filebuf);

        // this can be passed in, but if NULL 2K & XP will
        // remember the last location, presumably in the registry
        // that's handy, but I don't know how to test for that
        ofn.lpstrInitialDir = mDialog->getInitialDirectory();

        const char* filter = mDialog->getFilter();
        if (filter == NULL)
          ofn.lpstrFilter = "All\0*.*\0Text\0*.TXT\0";
        else {
            getWindowsFilter(filter, filterbuf);
            // !! who owns this?
            ofn.lpstrFilter = filterbuf;
        }

        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;

        // OFN_DONTADDTORECENT keeps what you select out of the
        // "My Recent Documents" list in the start menu
        // sadly it isn't defined in Vstudio 6
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NONETWORKBUTTON;

        // TODO: Obey the AllowMultiple flag, but we don't have a model
        // for returning them!!

        if (!mDialog->isSave()) {
            // what about PATHMUSTEXIST?
            ofn.Flags |= OFN_FILEMUSTEXIST;
        }
        else {
            ofn.Flags |= OFN_OVERWRITEPROMPT;
        }

        // these is always modal
        if (mDialog->isSave())
          status = (GetSaveFileName(&ofn) == TRUE);
        else
          status = (GetOpenFileName(&ofn) == TRUE);

        // if no extension, supply one
        // !! do this all the time?
        if (!strstr(filebuf, ".")) {
            char extension[32];

            // indexes lpstrFilter starting at 1, zero means the custom filter
            int filter = (int)(ofn.nFilterIndex);
            getExtension(ofn.lpstrFilter, filter, extension);
		
            if (strlen(extension)) {
                strcat(filebuf, ".");
                strcat(filebuf, extension);
            }
        }

        if (!status)
          mDialog->setCanceled(true);
        else
          mDialog->setFile(filebuf);
    }
}

PRIVATE void WindowsOpenDialog::getWindowsFilter(const char* src, char* dest)
{
	if (src != NULL) {
		int len = strlen(src);
		for (int i = 0 ; i < len ; i++) {
			if (src[i] == '|')
			  dest[i] = 0;
			else
			  dest[i] = src[i];
		}
		dest[len] = 0;
		// have to terminate with a double null
		if (dest[len-1] != 0)
		  dest[len+1] = 0;
	}
}

/**
 * Given a filter string formatted according to the OPENFILENAME
 * convention, extract the filter extension with the given index.
 * Index zero is the first filter.  Return the extension including
 * the dot.
 */
PRIVATE void WindowsOpenDialog::getExtension(const char* filter, int index, 
                                               char* extension)
{
	const char* ext = NULL;
    strcpy(extension, "");

    // filter items are indexed from 1
    // each item is a tuple of display name and extension
    int skip = (index * 2) - 1;

    ext = filter;
    for (int i = 0 ; i < skip ; i++) {
        while (*ext) ext++;
        ext++;
        if (*ext == 0) {
            // ran off the end, the index must be bad
            ext = NULL;
            break;
        }
    }

    if (ext != NULL) {
        // trim off the leading *.
        // there is some ambiguity about whether *. is required
        while (*ext && *ext != '.') ext++;
        if (*ext) ext++;

        // we're now on the extension
        // you can have several extensions seperated by semicolons
        // e.g. *.txt;*.doc, we only return the first one
        const char* last = ext;
        while (*last && *last != ';') last++;

        int len = (int)((unsigned long)last - (unsigned long)ext);
        strncpy(extension, ext, len);
		extension[len] = 0;
    }

}

QWIN_END_NAMESPACE
#endif // _WIN32

//////////////////////////////////////////////////////////////////////
//
// OSX
//
//////////////////////////////////////////////////////////////////////

#ifdef OSX
#include "Thread.h"
#include "MacUtil.h"
#include <Carbon/Carbon.h>
#include "UIMac.h"

QWIN_BEGIN_NAMESPACE

PUBLIC MacOpenDialog::MacOpenDialog(OpenDialog* d)
{
	mDialog = d;
}

PUBLIC MacOpenDialog::~MacOpenDialog()
{
}

void OpenDialogNavEventProc(NavEventCallbackMessage callBackSelector,
							NavCBRecPtr callBackParms,
							void *callBackUserData )
{
	if (callBackUserData != NULL) {
		MacOpenDialog* dialog = (MacOpenDialog*)callBackUserData;
		dialog->callback(callBackSelector, callBackParms);
	}
}

void MacOpenDialog::callback(NavEventCallbackMessage callBackSelector,
							 NavCBRecPtr callBackParms)
{
	switch (callBackSelector) {

		case kNavCBUserAction: {

			NavReplyRecord  reply;
			OSStatus status = NavDialogGetReply(callBackParms->context, &reply);

			// this one is special
			if (status == userCanceledErr) {
				mDialog->setCanceled(true);
			}
			else if (CheckStatus(status, "MacOpenDialog:NavDialogGetReply")) {

				NavUserAction userAction = NavDialogGetUserAction(callBackParms->context);
				switch (userAction) {
					case kNavUserActionSaveAs: {
						// validRecord true if dialog closed with Return or Enter
						// or by clicking the default button on an Open or Save dialog
						if (reply.validRecord) {
							// in typically bizzaro Mac fashion, the selection spec has the
							// directory name and the saveFileName has the leaf file, they
							// have to be combined
							// reply.replacing true if we're replacing an existing file	
							FSRef ref;
							OSErr err = extractFSRef(&reply, &ref);
							if (CheckErr(err, "OpenDialog:extractFSRef")) {
								unsigned char dirname[1024 * 2];
								OSStatus status = FSRefMakePath(&ref, dirname, sizeof(dirname));
								if (CheckStatus(status, "MacOpenDialog::FSRefMakePath")) {

									char* filename = NULL;
									CFStringRef file = reply.saveFileName;
									if (file != NULL)
									  filename = GetCString(file);
									
									// ugh, how many buffers does it take to make a path
									char path[1024 * 2];
									sprintf(path, "%s/%s", dirname, filename);
									delete filename;

									mDialog->setFile(path);
								}
							}
						}
					}
					break;
					case kNavUserActionOpen:
					case kNavUserActionChoose: {
						if (reply.validRecord) {
							//printf("OpenDialog: Open !\n");
							FSRef ref;
							OSErr err = extractFSRef(&reply, &ref);
							if (CheckErr(err, "OpenDialog:extractFSRef")) {
								unsigned char filename[1024 * 2];
								OSStatus status = FSRefMakePath(&ref, filename, sizeof(filename));
								if (CheckStatus(status, "MacOpenDialog::FSRefMakePath")) {
									mDialog->setFile((const char*)filename);
								}
							}
						}
					}
					break;
					case kNavUserActionCancel:
						//printf("OpenDialog: Cancel !\n");
						break;
					case kNavUserActionNewFolder:
						//printf("OpenDialog: New Folder !\n");
						// do WE have to make it?
						break;
				}
				CheckStatus(NavDisposeReply(&reply), "MacOpenDialog:NavDisposeReply");
            }
		}
		break;

		case kNavCBTerminate: {
			// documentation tests the mysterious MyDialogRef
            if (mHandle != NULL) {
				// this is what was in one of the examples, looks wrong
				//NavDialogDispose(callBackParms->context);
				NavDialogDispose((NavDialogRef)mHandle);
			}
		}
		break;
	}
}

/**
 * Extracts a single FSRef from a NavReplyRecord.
 * Taken from an example at:
 * http://developer.apple.com/samplecode/FSCopyObject/listing7.html
 *
 */
OSErr MacOpenDialog::extractFSRef(const NavReplyRecord *reply, FSRef *item)
{
	FSSpec fss;
	SInt32 itemCount;
	DescType junkType;
	AEKeyword junkKeyword;
	Size junkSize;
	OSErr osErr;
  
	osErr = AECountItems(&reply->selection, &itemCount);
	if( itemCount != 1 )  /* we only work with one object at a time */
	  osErr = paramErr;

	if( osErr == noErr )
	  osErr = AEGetNthPtr(&reply->selection, 1, typeFSS, &junkKeyword, &junkType, &fss, sizeof(fss), &junkSize);
 
	if( osErr == noErr ) {
		// jsl - not sure what mycheck did but probably traced
		//mycheck(junkType == typeFSS);
		//mycheck(junkSize == sizeof(FSSpec));

		if (junkType != typeFSS)
		  printf("MacOpenDialog::extractFSRef mismatched types!\n");

		if (junkSize != sizeof(FSSpec))
		  printf("MacOpenDialog::extractFSRef mismatched size!\n");

		/* We call FSMakeFSSpec because sometimes Nav is braindead    */
		/* and gives us an invalid FSSpec (where the name is empty).  */
		/* While FSpMakeFSRef seems to handle that (and the file system  */
		/* engineers assure me that that will keep working (at least  */
		/* on traditional Mac OS) because of the potential for breaking  */
		/* existing applications), I'm still wary of doing this so    */
		/* I regularise the FSSpec by feeding it through FSMakeFSSpec.  */
    
		if( fss.name[0] == 0 )
		  osErr = FSMakeFSSpec(fss.vRefNum, fss.parID, fss.name, &fss);

		if( osErr == noErr )
		  osErr = FSpMakeFSRef(&fss, item);
	}

	return osErr;
}

PUBLIC void MacOpenDialog::show()
{

	OSStatus status;
	NavDialogCreationOptions options;
    status = NavGetDefaultDialogCreationOptions(&options);
	CheckStatus(status, "MacOpenDialog::NavGetDefaultDialogCreationOptions");

	const char* title = mDialog->getTitle();
	if (title != NULL) {
		CFStringRef cftitle = MakeCFStringRef(title);
		options.windowTitle = cftitle;
	}

	// options.saveFileName can be used to set an initial file
	// options.message displays a message above the browser

	// This will make it appear as a sheet rather than a floating
	// window, though the event loop still looks modal.  I had
	// a crash after closing the dialog though.
	/*
	Window* parent = mDialog->getParent();
	if (parent != NULL) {
		WindowRef macWindow = (WindowRef)parent->getNativeHandle();
		if (macWindow != NULL) {
			// this is supposed to make it appear as a sheet
			options.modality = kWindowModalityWindowModal;
			options.parentWindow = macWindow;
		}
	}
	*/

	// NavCreateChooseFileDialog selects a single file
	// NavCreateGetFileDialog selects multiple files
	// NavCreateChooseFolderDialog is used to select folders
	// NavCreateChooseObjectDialog can select files, folders, or volumes

	// TODO: OpenDialog has a "filter" property that is currently using a Windows
	// syntax like this:
	// sprintf(filter, "%s (.mob)|*.mob;*.MOB", 
	// cat->get(MSG_DLG_OPEN_PROJECT_FILTER));
	// need to generalize this so we can set the inTypeList or use
	// a filterProc.  A filterProc seems better since we have custom types.

	if (mDialog->isSave()) {
		// may need to ask for kNavPreserveSaveFileExtension?
		status = NavCreatePutFileDialog(&options, 
										(OSType)NULL, // inFileType
										(OSType)NULL, // inFileCreator
										OpenDialogNavEventProc,
										this,
										&mHandle);
	}
	else if (mDialog->isAllowDirectories()) {
		status = NavCreateChooseObjectDialog(&options, 
											 OpenDialogNavEventProc,
											 NULL, // previewProc
											 NULL, // filterProc
											 this,
											 &mHandle);
 	}
	else {
		status = NavCreateChooseFileDialog(&options, 
										   NULL, // NavTypeListHandle
										   OpenDialogNavEventProc,
										   NULL, // previewProc
										   NULL, // filterProc
										   this,
										   &mHandle);
	}

	CheckStatus(status, "MacOpenDialog:NavCreate");

	// this appears to be a modal loop even if we don't ask for one
	status = NavDialogRun(mHandle);
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
