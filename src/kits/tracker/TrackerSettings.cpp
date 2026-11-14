/*
Open Tracker License

Terms and Conditions

Copyright (c) 1991-2000, Be Incorporated. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice applies to all licensees
and shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF TITLE, MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
BE INCORPORATED BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Be Incorporated shall not be
used in advertising or otherwise to promote the sale, use or other dealings in
this Software without prior written authorization from Be Incorporated.

Tracker(TM), Be(R), BeOS(R), and BeIA(TM) are trademarks or registered trademarks
of Be Incorporated in the United States and other countries. Other brand product
names are registered trademarks or trademarks of their respective holders.
All rights reserved.
*/


#include "TrackerSettings.h"

#include <Debug.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <Path.h>

#include <stdio.h>
#include <string.h>

#include "Tracker.h"
#include "TrackerDefaults.h"
#include "WidgetAttributeText.h"


#define RGBTOHEX(c) ((c.alpha << 24) | (c.red << 16) | (c.green << 8) | (c.blue))


class TTrackerState : public Settings {
public:
	static TTrackerState* Get();
	void Release();

	void LoadSettingsIfNeeded();
	void SaveSettings(bool onlyIfNonDefault = true);
	void _MigrateSnakeSettings();

	TTrackerState();
	~TTrackerState();

private:
	friend class BPrivate::TrackerSettings;

	static void InitIfNeeded();
	TTrackerState(const TTrackerState&);

	// SnakeTracker's own settings (and "HideDesktopIcons") are in a file of their own,
	// "SnakeSettings": the system's libtracker, which every program using a file panel
	// loads, reads TrackerSettings and prints "unknown command" for every line it does not know.
	Settings fSnake;

	BooleanValueSetting* fShowDisksIcon;
	BooleanValueSetting* fMountVolumesOntoDesktop;
	BooleanValueSetting* fDesktopFilePanelRoot;
	BooleanValueSetting* fMountSharedVolumesOntoDesktop;
	BooleanValueSetting* fEjectWhenUnmounting;

	BooleanValueSetting* fShowFullPathInTitleBar;
	BooleanValueSetting* fSingleWindowBrowse;
	BooleanValueSetting* fShowNavigator;
	BooleanValueSetting* fShowSelectionWhenInactive;
	BooleanValueSetting* fTransparentSelection;
	BooleanValueSetting* fSortFolderNamesFirst;
	BooleanValueSetting* fHideDotFiles;
	BooleanValueSetting* fHideDesktopIcons;
	BooleanValueSetting* fTypeAheadFiltering;
	BooleanValueSetting* fGenerateImageThumbnails;
	BooleanValueSetting* fSnakeTrail;
	BooleanValueSetting* fSnakeFlat;
	BooleanValueSetting* fSnakeLook;
	BooleanValueSetting* fSnakeRoundCorners;
	BooleanValueSetting* fSnakeDeskbarRoundCorners;
	BooleanValueSetting* fSnakeSystemColorsDeskbar;
	BooleanValueSetting* fSnakeSystemColorsTracker;
	BooleanValueSetting* fSnakeAllApps;
	BooleanValueSetting* fSnakeArrows;
	BooleanValueSetting* fSnakeSidePanel;
	BooleanValueSetting* fSnakeSidePanelTrash;
	HexScalarValueSetting* fSnakeAccent;

	ScalarValueSetting* fRecentApplicationsCount;
	ScalarValueSetting* fRecentDocumentsCount;
	ScalarValueSetting* fRecentFoldersCount;

	BooleanValueSetting* fShowVolumeSpaceBar;
	HexScalarValueSetting* fUsedSpaceColor;
	HexScalarValueSetting* fFreeSpaceColor;
	HexScalarValueSetting* fWarningSpaceColor;

	Benaphore fInitLock;
	bool fInited;
	bool fSettingsLoaded;

	int32 fUseCounter;

	typedef Settings _inherited;
};


static TTrackerState gTrackerState;


rgb_color ValueToColor(int32 value)
{
	rgb_color color;
	color.alpha = static_cast<uchar>((value >> 24L) & 0xff);
	color.red = static_cast<uchar>((value >> 16L) & 0xff);
	color.green = static_cast<uchar>((value >> 8L) & 0xff);
	color.blue = static_cast<uchar>(value & 0xff);

	return color;
}


int32 ColorToValue(rgb_color color)
{
	return color.alpha << 24L | color.red << 16L | color.green << 8L
		| color.blue;
}


//	#pragma mark - TTrackerState


TTrackerState::TTrackerState()
	:
	Settings("TrackerSettings", "Tracker"),
	fSnake("SnakeSettings", "Tracker"),
	fShowDisksIcon(NULL),
	fMountVolumesOntoDesktop(NULL),
	fDesktopFilePanelRoot(NULL),
	fMountSharedVolumesOntoDesktop(NULL),
	fEjectWhenUnmounting(NULL),
	fShowFullPathInTitleBar(NULL),
	fSingleWindowBrowse(NULL),
	fShowNavigator(NULL),
	fShowSelectionWhenInactive(NULL),
	fTransparentSelection(NULL),
	fSortFolderNamesFirst(NULL),
	fHideDotFiles(NULL),
	fHideDesktopIcons(NULL),
	fTypeAheadFiltering(NULL),
	fGenerateImageThumbnails(NULL),
	fSnakeTrail(NULL),
	fSnakeFlat(NULL),
	fSnakeLook(NULL),
	fSnakeRoundCorners(NULL),
	fSnakeDeskbarRoundCorners(NULL),
	fSnakeSystemColorsDeskbar(NULL),
	fSnakeSystemColorsTracker(NULL),
	fSnakeAllApps(NULL),
	fSnakeArrows(NULL),
	fSnakeSidePanel(NULL),
	fSnakeSidePanelTrash(NULL),
	fSnakeAccent(NULL),
	fRecentApplicationsCount(NULL),
	fRecentDocumentsCount(NULL),
	fRecentFoldersCount(NULL),
	fShowVolumeSpaceBar(NULL),
	fUsedSpaceColor(NULL),
	fFreeSpaceColor(NULL),
	fWarningSpaceColor(NULL),
	fInited(false),
	fSettingsLoaded(false)
{
}


TTrackerState::TTrackerState(const TTrackerState&)
	:
	Settings("", ""),
	fSnake("", ""),
	fShowDisksIcon(NULL),
	fMountVolumesOntoDesktop(NULL),
	fDesktopFilePanelRoot(NULL),
	fMountSharedVolumesOntoDesktop(NULL),
	fEjectWhenUnmounting(NULL),
	fShowFullPathInTitleBar(NULL),
	fSingleWindowBrowse(NULL),
	fShowNavigator(NULL),
	fShowSelectionWhenInactive(NULL),
	fTransparentSelection(NULL),
	fSortFolderNamesFirst(NULL),
	fHideDotFiles(NULL),
	fHideDesktopIcons(NULL),
	fTypeAheadFiltering(NULL),
	fGenerateImageThumbnails(NULL),
	fSnakeTrail(NULL),
	fSnakeFlat(NULL),
	fSnakeLook(NULL),
	fSnakeRoundCorners(NULL),
	fSnakeDeskbarRoundCorners(NULL),
	fSnakeSystemColorsDeskbar(NULL),
	fSnakeSystemColorsTracker(NULL),
	fSnakeAllApps(NULL),
	fSnakeArrows(NULL),
	fSnakeSidePanel(NULL),
	fSnakeSidePanelTrash(NULL),
	fSnakeAccent(NULL),
	fRecentApplicationsCount(NULL),
	fRecentDocumentsCount(NULL),
	fRecentFoldersCount(NULL),
	fShowVolumeSpaceBar(NULL),
	fUsedSpaceColor(NULL),
	fFreeSpaceColor(NULL),
	fWarningSpaceColor(NULL),
	fInited(false),
	fSettingsLoaded(false)
{
	// Placeholder copy constructor to prevent others from accidentally using
	// the default copy constructor.  Note, the DEBUGGER call is for the off
	// chance that a TTrackerState method (or friend) tries to make a copy.
	DEBUGGER("Don't make a copy of this!");
}


TTrackerState::~TTrackerState()
{
}


void
TTrackerState::SaveSettings(bool onlyIfNonDefault)
{
	if (fSettingsLoaded) {
		_inherited::SaveSettings(onlyIfNonDefault);
		fSnake.SaveSettings(onlyIfNonDefault);
	}
}


void
TTrackerState::LoadSettingsIfNeeded()
{
	if (fSettingsLoaded)
		return;

	// Set default settings before reading from disk

	Add(fShowDisksIcon = new BooleanValueSetting("ShowDisksIcon", kDefaultShowDisksIcon));
	Add(fMountVolumesOntoDesktop
		= new BooleanValueSetting("MountVolumesOntoDesktop", kDefaultMountVolumesOntoDesktop));
	Add(fMountSharedVolumesOntoDesktop = new BooleanValueSetting(
		"MountSharedVolumesOntoDesktop", kDefaultMountSharedVolumesOntoDesktop));
	Add(fEjectWhenUnmounting
		= new BooleanValueSetting("EjectWhenUnmounting", kDefaultEjectWhenUnmounting));

	Add(fDesktopFilePanelRoot // deprecated
		= new BooleanValueSetting("DesktopFilePanelRoot", kDefaultDesktopFilePanelRoot));
	Add(fShowSelectionWhenInactive // deprecated
		= new BooleanValueSetting("ShowSelectionWhenInactive", kDefaultShowSelectionWhenInactive));

	Add(fShowFullPathInTitleBar
		= new BooleanValueSetting("ShowFullPathInTitleBar", kDefaultShowFullPathInTitleBar));
	Add(fSingleWindowBrowse
		= new BooleanValueSetting("SingleWindowBrowse", kDefaultSingleWindowBrowse));
	Add(fShowNavigator = new BooleanValueSetting("ShowNavigator", kDefaultShowNavigator));
	Add(fTransparentSelection
		= new BooleanValueSetting("TransparentSelection", kDefaultTransparentSelection));
	Add(fSortFolderNamesFirst
		= new BooleanValueSetting("SortFolderNamesFirst", kDefaultSortFolderNamesFirst));
	Add(fHideDotFiles = new BooleanValueSetting("HideDotFiles", kDefaultHideDotFiles));
	fSnake.Add(fHideDesktopIcons
		= new BooleanValueSetting("HideDesktopIcons", kDefaultHideDesktopIcons));
	Add(fTypeAheadFiltering
		= new BooleanValueSetting("TypeAheadFiltering", kDefaultTypeAheadFiltering));
	Add(fGenerateImageThumbnails
		= new BooleanValueSetting("GenerateImageThumbnails", kDefaultGenerateImageThumbnails));

	fSnake.Add(fSnakeTrail = new BooleanValueSetting("SnakeTrail", kDefaultSnakeTrail));
	fSnake.Add(fSnakeFlat = new BooleanValueSetting("SnakeFlat", kDefaultSnakeFlat));
	fSnake.Add(fSnakeLook = new BooleanValueSetting("SnakeLook", kDefaultSnakeLook));
	fSnake.Add(fSnakeRoundCorners = new BooleanValueSetting("SnakeRoundCorners", kDefaultSnakeRoundCorners));
	fSnake.Add(fSnakeDeskbarRoundCorners
		= new BooleanValueSetting("SnakeDeskbarRoundCorners", kDefaultSnakeDeskbarRoundCorners));
	fSnake.Add(fSnakeSystemColorsDeskbar = new BooleanValueSetting("SnakeSystemColorsDeskbar", kDefaultSnakeSystemColorsDeskbar));
	fSnake.Add(fSnakeSystemColorsTracker = new BooleanValueSetting("SnakeSystemColorsTracker", kDefaultSnakeSystemColorsTracker));
	fSnake.Add(fSnakeAllApps = new BooleanValueSetting("SnakeAllApps", kDefaultSnakeAllApps));
	fSnake.Add(fSnakeArrows = new BooleanValueSetting("SnakeArrows", kDefaultSnakeArrows));
	fSnake.Add(fSnakeSidePanel = new BooleanValueSetting("SnakeSidePanel", kDefaultSnakeSidePanel));
	fSnake.Add(fSnakeSidePanelTrash = new BooleanValueSetting("SnakeSidePanelTrash", kDefaultSnakeSidePanelTrash));
	fSnake.Add(fSnakeAccent
		= new HexScalarValueSetting("SnakeAccent", RGBTOHEX(kDefaultSnakeAccent), "", ""));

	Add(fRecentApplicationsCount
		= new ScalarValueSetting("RecentApplications", kDefaultRecentApplications, "", ""));
	Add(fRecentDocumentsCount
		= new ScalarValueSetting("RecentDocuments", kDefaultRecentDocuments, "", ""));
	Add(fRecentFoldersCount
		= new ScalarValueSetting("RecentFolders", kDefaultRecentFolders, "", ""));

	Add(fShowVolumeSpaceBar
		= new BooleanValueSetting("ShowVolumeSpaceBar", kDefaultShowVolumeSpaceBar));
	Add(fUsedSpaceColor
		= new HexScalarValueSetting("UsedSpaceColor", RGBTOHEX(kDefaultUsedSpaceColor), "", ""));
	Add(fFreeSpaceColor
		= new HexScalarValueSetting("FreeSpaceColor", RGBTOHEX(kDefaultFreeSpaceColor), "", ""));
	Add(fWarningSpaceColor
		= new HexScalarValueSetting("WarningSpaceColor", RGBTOHEX(kDefaultWarningSpaceColor),
			"", ""));

	TryReadingSettings();
	fSnake.TryReadingSettings();
	_MigrateSnakeSettings();

	NameAttributeText::SetSortFolderNamesFirst(
		fSortFolderNamesFirst->Value());
	RealNameAttributeText::SetSortFolderNamesFirst(
		fSortFolderNamesFirst->Value());

	fSettingsLoaded = true;
}


// The first time with a separate file: the values SnakeTracker 1.0.67 and older kept in TrackerSettings are taken
// over, and TrackerSettings is written again without them.
void
TTrackerState::_MigrateSnakeSettings()
{
	BPath snakePath;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &snakePath) != B_OK
		|| snakePath.Append("Tracker/SnakeSettings") != B_OK || BEntry(snakePath.Path()).Exists()) {
		return;
	}

	BPath oldPath;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &oldPath) == B_OK
		&& oldPath.Append("Tracker/TrackerSettings") == B_OK) {
		SettingsArgvDispatcher* snake[] = {
			fHideDesktopIcons, fSnakeTrail, fSnakeFlat, fSnakeLook, fSnakeRoundCorners,
			fSnakeDeskbarRoundCorners, fSnakeSystemColorsDeskbar, fSnakeSystemColorsTracker,
			fSnakeAllApps, fSnakeArrows, fSnakeSidePanel, fSnakeSidePanelTrash, fSnakeAccent
		};
		FILE* file = fopen(oldPath.Path(), "r");
		if (file != NULL) {
			char line[256];
			while (fgets(line, sizeof(line), file) != NULL) {
				char name[64];
				char value[64];
				if (sscanf(line, "%63s %63s", name, value) != 2)
					continue;
				for (size_t i = 0; i < sizeof(snake) / sizeof(snake[0]); i++) {
					if (strcmp(snake[i]->Name(), name) == 0) {
						const char* argv[] = { name, value, NULL };
						snake[i]->Handle(argv);
					}
				}
			}
			fclose(file);
		}
	}

	_inherited::SaveSettings(true);
	fSnake.SaveSettings(true);
}


//	#pragma mark - TrackerSettings


TrackerSettings::TrackerSettings()
{
	gTrackerState.LoadSettingsIfNeeded();
}


void
TrackerSettings::SaveSettings(bool onlyIfNonDefault)
{
	gTrackerState.SaveSettings(onlyIfNonDefault);
}


bool
TrackerSettings::ShowDisksIcon()
{
	return gTrackerState.fShowDisksIcon->Value();
}


void
TrackerSettings::SetShowDisksIcon(bool enabled)
{
	gTrackerState.fShowDisksIcon->SetValue(enabled);
	gTrackerState.fMountVolumesOntoDesktop->SetValue(!enabled);
}


bool
TrackerSettings::DesktopFilePanelRoot()
{
	return gTrackerState.fDesktopFilePanelRoot->Value();
}


void
TrackerSettings::SetDesktopFilePanelRoot(bool enabled)
{
	gTrackerState.fDesktopFilePanelRoot->SetValue(enabled);
}


bool
TrackerSettings::MountVolumesOntoDesktop()
{
	return gTrackerState.fMountVolumesOntoDesktop->Value();
}


void
TrackerSettings::SetMountVolumesOntoDesktop(bool enabled)
{
	gTrackerState.fShowDisksIcon->SetValue(!enabled);
	gTrackerState.fMountVolumesOntoDesktop->SetValue(enabled);
}


bool
TrackerSettings::MountSharedVolumesOntoDesktop()
{
	return gTrackerState.fMountSharedVolumesOntoDesktop->Value();
}


void
TrackerSettings::SetMountSharedVolumesOntoDesktop(bool enabled)
{
	gTrackerState.fMountSharedVolumesOntoDesktop->SetValue(enabled);
}


bool
TrackerSettings::EjectWhenUnmounting()
{
	return gTrackerState.fEjectWhenUnmounting->Value();
}


void
TrackerSettings::SetEjectWhenUnmounting(bool enabled)
{
	gTrackerState.fEjectWhenUnmounting->SetValue(enabled);
}


bool
TrackerSettings::ShowVolumeSpaceBar()
{
	return gTrackerState.fShowVolumeSpaceBar->Value();
}


void
TrackerSettings::SetShowVolumeSpaceBar(bool enabled)
{
	gTrackerState.fShowVolumeSpaceBar->SetValue(enabled);
}


rgb_color
TrackerSettings::UsedSpaceColor()
{
	return ValueToColor(gTrackerState.fUsedSpaceColor->Value());
}


void
TrackerSettings::SetUsedSpaceColor(rgb_color color)
{
	gTrackerState.fUsedSpaceColor->ValueChanged(ColorToValue(color));
}


rgb_color
TrackerSettings::FreeSpaceColor()
{
	return ValueToColor(gTrackerState.fFreeSpaceColor->Value());
}


void
TrackerSettings::SetFreeSpaceColor(rgb_color color)
{
	gTrackerState.fFreeSpaceColor->ValueChanged(ColorToValue(color));
}


rgb_color
TrackerSettings::WarningSpaceColor()
{
	return ValueToColor(gTrackerState.fWarningSpaceColor->Value());
}


void
TrackerSettings::SetWarningSpaceColor(rgb_color color)
{
	gTrackerState.fWarningSpaceColor->ValueChanged(ColorToValue(color));
}


bool
TrackerSettings::ShowFullPathInTitleBar()
{
	return gTrackerState.fShowFullPathInTitleBar->Value();
}


void
TrackerSettings::SetShowFullPathInTitleBar(bool enabled)
{
	gTrackerState.fShowFullPathInTitleBar->SetValue(enabled);
}


bool
TrackerSettings::SortFolderNamesFirst()
{
	return gTrackerState.fSortFolderNamesFirst->Value();
}


void
TrackerSettings::SetSortFolderNamesFirst(bool enabled)
{
	gTrackerState.fSortFolderNamesFirst->SetValue(enabled);
	NameAttributeText::SetSortFolderNamesFirst(enabled);
	RealNameAttributeText::SetSortFolderNamesFirst(enabled);
}


bool
TrackerSettings::HideDotFiles()
{
	return gTrackerState.fHideDotFiles->Value();
}


void
TrackerSettings::SetHideDotFiles(bool hide)
{
	gTrackerState.fHideDotFiles->SetValue(hide);
}


bool
TrackerSettings::HideDesktopIcons()
{
	return gTrackerState.fHideDesktopIcons->Value();
}


void
TrackerSettings::SetHideDesktopIcons(bool hide)
{
	gTrackerState.fHideDesktopIcons->SetValue(hide);
}


bool
TrackerSettings::TypeAheadFiltering()
{
	return gTrackerState.fTypeAheadFiltering->Value();
}


void
TrackerSettings::SetTypeAheadFiltering(bool enabled)
{
	gTrackerState.fTypeAheadFiltering->SetValue(enabled);
}


bool
TrackerSettings::GenerateImageThumbnails()
{
	return gTrackerState.fGenerateImageThumbnails->Value();
}


void
TrackerSettings::SetGenerateImageThumbnails(bool enabled)
{
	gTrackerState.fGenerateImageThumbnails->SetValue(enabled);
}


bool
TrackerSettings::SnakeTrail()
{
	return gTrackerState.fSnakeTrail->Value();
}


void
TrackerSettings::SetSnakeTrail(bool enabled)
{
	gTrackerState.fSnakeTrail->SetValue(enabled);
}


bool
TrackerSettings::SnakeLook()
{
	return gTrackerState.fSnakeLook->Value();
}


void
TrackerSettings::SetSnakeLook(bool look)
{
	gTrackerState.fSnakeLook->SetValue(look);
}


bool
TrackerSettings::SnakeRoundCorners()
{
	return gTrackerState.fSnakeRoundCorners->Value();
}


void
TrackerSettings::SetSnakeRoundCorners(bool value)
{
	gTrackerState.fSnakeRoundCorners->SetValue(value);
}


bool
TrackerSettings::SnakeDeskbarRoundCorners()
{
	return gTrackerState.fSnakeDeskbarRoundCorners->Value();
}


void
TrackerSettings::SetSnakeDeskbarRoundCorners(bool value)
{
	gTrackerState.fSnakeDeskbarRoundCorners->SetValue(value);
}


bool
TrackerSettings::SnakeSystemColorsDeskbar()
{
	return gTrackerState.fSnakeSystemColorsDeskbar->Value();
}


void
TrackerSettings::SetSnakeSystemColorsDeskbar(bool value)
{
	gTrackerState.fSnakeSystemColorsDeskbar->SetValue(value);
}


bool
TrackerSettings::SnakeSystemColorsTracker()
{
	return gTrackerState.fSnakeSystemColorsTracker->Value();
}


void
TrackerSettings::SetSnakeSystemColorsTracker(bool value)
{
	gTrackerState.fSnakeSystemColorsTracker->SetValue(value);
}


bool
TrackerSettings::SnakeAllApps()
{
	return gTrackerState.fSnakeAllApps->Value();
}


void
TrackerSettings::SetSnakeAllApps(bool value)
{
	gTrackerState.fSnakeAllApps->SetValue(value);
}


bool
TrackerSettings::SnakeFlat()
{
	return gTrackerState.fSnakeFlat->Value();
}


void
TrackerSettings::SetSnakeFlat(bool flat)
{
	gTrackerState.fSnakeFlat->SetValue(flat);
}


bool
TrackerSettings::SnakeArrows()
{
	return gTrackerState.fSnakeArrows->Value();
}


void
TrackerSettings::SetSnakeArrows(bool arrows)
{
	gTrackerState.fSnakeArrows->SetValue(arrows);
}


bool
TrackerSettings::SnakeSidePanel()
{
	return gTrackerState.fSnakeSidePanel->Value();
}


void
TrackerSettings::SetSnakeSidePanel(bool show)
{
	gTrackerState.fSnakeSidePanel->SetValue(show);
}


bool
TrackerSettings::SnakeSidePanelTrash()
{
	return gTrackerState.fSnakeSidePanelTrash->Value();
}


void
TrackerSettings::SetSnakeSidePanelTrash(bool show)
{
	gTrackerState.fSnakeSidePanelTrash->SetValue(show);
}


rgb_color
TrackerSettings::SnakeAccent()
{
	rgb_color color = ValueToColor(gTrackerState.fSnakeAccent->Value());
	color.alpha = 255;
	return color;
}


void
TrackerSettings::SetSnakeAccent(rgb_color color)
{
	color.alpha = 255;
	gTrackerState.fSnakeAccent->ValueChanged(ColorToValue(color));
}


bool
TrackerSettings::ShowSelectionWhenInactive()
{
	return gTrackerState.fShowSelectionWhenInactive->Value();
}


void
TrackerSettings::SetShowSelectionWhenInactive(bool enabled)
{
	gTrackerState.fShowSelectionWhenInactive->SetValue(enabled);
}


bool
TrackerSettings::TransparentSelection()
{
	return gTrackerState.fTransparentSelection->Value();
}


void
TrackerSettings::SetTransparentSelection(bool enabled)
{
	gTrackerState.fTransparentSelection->SetValue(enabled);
}


bool
TrackerSettings::SingleWindowBrowse()
{
	return gTrackerState.fSingleWindowBrowse->Value();
}


void
TrackerSettings::SetSingleWindowBrowse(bool enabled)
{
	gTrackerState.fSingleWindowBrowse->SetValue(enabled);
}


bool
TrackerSettings::ShowNavigator()
{
	return gTrackerState.fShowNavigator->Value();
}


void
TrackerSettings::SetShowNavigator(bool enabled)
{
	gTrackerState.fShowNavigator->SetValue(enabled);
}


void
TrackerSettings::RecentCounts(int32* applications, int32* documents,
	int32* folders)
{
	if (applications != NULL)
		*applications = gTrackerState.fRecentApplicationsCount->Value();

	if (documents != NULL)
		*documents = gTrackerState.fRecentDocumentsCount->Value();

	if (folders != NULL)
		*folders = gTrackerState.fRecentFoldersCount->Value();
}


void
TrackerSettings::SetRecentApplicationsCount(int32 count)
{
	gTrackerState.fRecentApplicationsCount->ValueChanged(count);
}


void
TrackerSettings::SetRecentDocumentsCount(int32 count)
{
	gTrackerState.fRecentDocumentsCount->ValueChanged(count);
}


void
TrackerSettings::SetRecentFoldersCount(int32 count)
{
	gTrackerState.fRecentFoldersCount->ValueChanged(count);
}
