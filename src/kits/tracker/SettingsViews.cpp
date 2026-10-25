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


#include "SettingsViews.h"

#include <Box.h>
#include <OS.h>
#include <Path.h>
#include <Entry.h>
#include <Roster.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <ColorControl.h>
#include <ControlLook.h>
#include <LayoutBuilder.h>
#include <math.h>
#include <Locale.h>
#include <MenuField.h>
#include <NodeMonitor.h>
#include <Point.h>
#include <PopUpMenu.h>
#include <RadioButton.h>
#include <StringView.h>

#include "Commands.h"
#include "DeskWindow.h"
#include "Model.h"
#include "Tracker.h"
#include "TrackerDefaults.h"
#include "WidgetAttributeText.h"


static const uint32 kSpaceBarSwitchColor = 'SBsc';


static void
send_bool_notices(uint32 what, const char* name, bool value)
{
	TTracker* tracker = dynamic_cast<TTracker*>(be_app);
	if (tracker == NULL)
		return;

	BMessage message;
	message.AddBool(name, value);
	tracker->SendNotices(what, &message);
}


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SettingsView"


//	#pragma mark - SettingsView


SettingsView::SettingsView(const char* name)
	:
	BGroupView(name)
{
}


SettingsView::~SettingsView()
{
}


/*!
	The inherited functions should set the default values
	and update the UI gadgets. The latter can by done by
	calling ShowCurrentSettings().
*/
void
SettingsView::SetDefaults()
{
}


/*!
	This function is used by the window to tell whether
	it can ghost the defaults button or not. It doesn't
	shows the default settings, this function should
	return true.
*/
bool
SettingsView::IsDefaultable() const
{
	return true;
}


/*!
	The inherited functions should set the values that was
	active when the settings window opened. It should also
	update the UI widgets accordingly, preferrable by calling
	ShowCurrentSettings().
*/
void
SettingsView::Revert()
{
}


/*!
	This function is called when the window is shown to let
	the settings views record the state to revert to.
*/
void
SettingsView::RecordRevertSettings()
{
}


/*!
	This function is used by the window to tell the view
	to display the current settings in the tracker.
*/
void
SettingsView::ShowCurrentSettings()
{
}


/*!
	This function is used by the window to tell whether
	it can ghost the revert button or not. It it shows the
	reverted settings, this function should return true.
*/
bool
SettingsView::IsRevertable() const
{
	return true;
}


// #pragma mark - DesktopSettingsView


DesktopSettingsView::DesktopSettingsView()
	:
	SettingsView("DesktopSettingsView"),
	fShowDisksIconRadioButton(NULL),
	fMountVolumesOntoDesktopRadioButton(NULL),
	fMountSharedVolumesOntoDesktopCheckBox(NULL),
	fShowDisksIcon(kDefaultShowDisksIcon),
	fMountVolumesOntoDesktop(kDefaultMountVolumesOntoDesktop),
	fMountSharedVolumesOntoDesktop(kDefaultMountSharedVolumesOntoDesktop),
	fIntegrateNonBootBeOSDesktops(false),
	fEjectWhenUnmounting(kDefaultEjectWhenUnmounting)
{
	fShowDisksIconRadioButton = new BRadioButton("",
		B_TRANSLATE("Show Disks icon"),
		new BMessage(kShowDisksIconChanged));

	fMountVolumesOntoDesktopRadioButton = new BRadioButton("",
		B_TRANSLATE("Show volumes on Desktop"),
		new BMessage(kVolumesOnDesktopChanged));

	fMountSharedVolumesOntoDesktopCheckBox = new BCheckBox("",
		B_TRANSLATE("Show shared volumes on Desktop"),
		new BMessage(kVolumesOnDesktopChanged));

	const float spacing = be_control_look->DefaultItemSpacing();

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.Add(fShowDisksIconRadioButton)
		.Add(fMountVolumesOntoDesktopRadioButton)
		.AddGroup(B_VERTICAL, 0)
			.Add(fMountSharedVolumesOntoDesktopCheckBox)
			.SetInsets(spacing * 2, 0, 0, 0)
			.End()
		.AddGlue()
		.SetInsets(spacing);
}


void
DesktopSettingsView::AttachedToWindow()
{
	fShowDisksIconRadioButton->SetTarget(this);
	fMountVolumesOntoDesktopRadioButton->SetTarget(this);
	fMountSharedVolumesOntoDesktopCheckBox->SetTarget(this);
}


void
DesktopSettingsView::MessageReceived(BMessage* message)
{
	TTracker* tracker = dynamic_cast<TTracker*>(be_app);
	if (tracker == NULL)
		return;

	TrackerSettings settings;

	switch (message->what) {
		case kShowDisksIconChanged:
		{
			// Turn on and off related settings:
			fMountVolumesOntoDesktopRadioButton->SetValue(
				!(fShowDisksIconRadioButton->Value() == 1));
			fMountSharedVolumesOntoDesktopCheckBox->SetEnabled(
				fMountVolumesOntoDesktopRadioButton->Value() == 1);

			// Set the new settings in the tracker:
			settings.SetShowDisksIcon(fShowDisksIconRadioButton->Value() == 1);
			settings.SetMountVolumesOntoDesktop(
				fMountVolumesOntoDesktopRadioButton->Value() == 1);
			settings.SetMountSharedVolumesOntoDesktop(
				fMountSharedVolumesOntoDesktopCheckBox->Value() == 1);

			// Construct the notification message:
			BMessage notificationMessage;
			notificationMessage.AddBool("ShowDisksIcon",
				fShowDisksIconRadioButton->Value() == 1);
			notificationMessage.AddBool("MountVolumesOntoDesktop",
				fMountVolumesOntoDesktopRadioButton->Value() == 1);
			notificationMessage.AddBool("MountSharedVolumesOntoDesktop",
				fMountSharedVolumesOntoDesktopCheckBox->Value() == 1);

			// Send the notification message:
			tracker->SendNotices(kVolumesOnDesktopChanged,
				&notificationMessage);

			// Tell the settings window the contents have changed:
			Window()->PostMessage(kSettingsContentsModified);
			break;
		}

		case kVolumesOnDesktopChanged:
		{
			// Turn on and off related settings:
			fShowDisksIconRadioButton->SetValue(
				!(fMountVolumesOntoDesktopRadioButton->Value() == 1));
			fMountSharedVolumesOntoDesktopCheckBox->SetEnabled(
				fMountVolumesOntoDesktopRadioButton->Value() == 1);

			// Set the new settings in the tracker:
			settings.SetShowDisksIcon(fShowDisksIconRadioButton->Value() == 1);
			settings.SetMountVolumesOntoDesktop(
				fMountVolumesOntoDesktopRadioButton->Value() == 1);
			settings.SetMountSharedVolumesOntoDesktop(
				fMountSharedVolumesOntoDesktopCheckBox->Value() == 1);

			// Construct the notification message:
			BMessage notificationMessage;
			notificationMessage.AddBool("ShowDisksIcon",
				fShowDisksIconRadioButton->Value() == 1);
			notificationMessage.AddBool("MountVolumesOntoDesktop",
				fMountVolumesOntoDesktopRadioButton->Value() == 1);
			notificationMessage.AddBool("MountSharedVolumesOntoDesktop",
				fMountSharedVolumesOntoDesktopCheckBox->Value() == 1);

			// Send the notification message:
			tracker->SendNotices(kVolumesOnDesktopChanged, &notificationMessage);

			// Tell the settings window the contents have changed:
			Window()->PostMessage(kSettingsContentsModified);
			break;
		}

		default:
			_inherited::MessageReceived(message);
			break;
	}
}


void
DesktopSettingsView::SetDefaults()
{
	// ToDo: Avoid the duplication of the default values.
	TrackerSettings settings;

	settings.SetShowDisksIcon(kDefaultShowDisksIcon);
	settings.SetMountVolumesOntoDesktop(kDefaultMountVolumesOntoDesktop);
	settings.SetMountSharedVolumesOntoDesktop(kDefaultMountSharedVolumesOntoDesktop);
	settings.SetEjectWhenUnmounting(kDefaultEjectWhenUnmounting);

	ShowCurrentSettings();
	_SendNotices();
}


bool
DesktopSettingsView::IsDefaultable() const
{
	TrackerSettings settings;

	return settings.ShowDisksIcon() != kDefaultShowDisksIcon
		|| settings.MountVolumesOntoDesktop() != kDefaultMountVolumesOntoDesktop
		|| settings.MountSharedVolumesOntoDesktop() != kDefaultMountSharedVolumesOntoDesktop
		|| settings.EjectWhenUnmounting() != kDefaultEjectWhenUnmounting;
}


void
DesktopSettingsView::Revert()
{
	TrackerSettings settings;

	settings.SetShowDisksIcon(fShowDisksIcon);
	settings.SetMountVolumesOntoDesktop(fMountVolumesOntoDesktop);
	settings.SetMountSharedVolumesOntoDesktop(fMountSharedVolumesOntoDesktop);
	settings.SetEjectWhenUnmounting(fEjectWhenUnmounting);

	ShowCurrentSettings();
	_SendNotices();
}


void
DesktopSettingsView::_SendNotices()
{
	TTracker* tracker = dynamic_cast<TTracker*>(be_app);
	if (tracker == NULL)
		return;

	// Construct the notification message:
	BMessage notificationMessage;
	notificationMessage.AddBool("ShowDisksIcon",
		fShowDisksIconRadioButton->Value() == 1);
	notificationMessage.AddBool("MountVolumesOntoDesktop",
		fMountVolumesOntoDesktopRadioButton->Value() == 1);
	notificationMessage.AddBool("MountSharedVolumesOntoDesktop",
		fMountSharedVolumesOntoDesktopCheckBox->Value() == 1);

	// Send notices to the tracker about the change:
	tracker->SendNotices(kVolumesOnDesktopChanged, &notificationMessage);
	tracker->SendNotices(kDesktopIntegrationChanged, &notificationMessage);
}


void
DesktopSettingsView::ShowCurrentSettings()
{
	TrackerSettings settings;

	fShowDisksIconRadioButton->SetValue(settings.ShowDisksIcon());
	fMountVolumesOntoDesktopRadioButton->SetValue(
		settings.MountVolumesOntoDesktop());

	fMountSharedVolumesOntoDesktopCheckBox->SetValue(
		settings.MountSharedVolumesOntoDesktop());
	fMountSharedVolumesOntoDesktopCheckBox->SetEnabled(
		settings.MountVolumesOntoDesktop());
}


void
DesktopSettingsView::RecordRevertSettings()
{
	TrackerSettings settings;

	fShowDisksIcon = settings.ShowDisksIcon();
	fMountVolumesOntoDesktop = settings.MountVolumesOntoDesktop();
	fMountSharedVolumesOntoDesktop = settings.MountSharedVolumesOntoDesktop();
	fEjectWhenUnmounting = settings.EjectWhenUnmounting();
}


bool
DesktopSettingsView::IsRevertable() const
{
	return fShowDisksIcon != (fShowDisksIconRadioButton->Value() > 0)
		|| fMountVolumesOntoDesktop !=
			(fMountVolumesOntoDesktopRadioButton->Value() > 0)
		|| fMountSharedVolumesOntoDesktop !=
			(fMountSharedVolumesOntoDesktopCheckBox->Value() > 0);
}


// #pragma mark - WindowsSettingsView


static uint32
_ColorValue(rgb_color color)
{
	return (color.red << 16) | (color.green << 8) | color.blue;
}


//	#pragma mark - SnakeColorSwatch


SnakeColorSwatch::SnakeColorSwatch(const char* name)
	:
	BView(name, B_WILL_DRAW),
	fColor(kDefaultSnakeAccent)
{
	SetViewColor(B_TRANSPARENT_COLOR);
	const float size = ceilf(be_plain_font->Size() * 2.4f);
	SetExplicitMinSize(BSize(size, size));
	SetExplicitMaxSize(BSize(size, size));
	SetExplicitPreferredSize(BSize(size, size));
}


void
SnakeColorSwatch::SetColor(rgb_color color)
{
	fColor = color;
	Invalidate();
}


void
SnakeColorSwatch::Draw(BRect)
{
	BRect bounds(Bounds());
	rgb_color panel = ui_color(B_PANEL_BACKGROUND_COLOR);
	SetHighColor(panel);
	FillRect(bounds);
	// a thin frame in a darker tone of the panel, and the colour inside it
	SetHighColor(tint_color(panel, B_DARKEN_3_TINT));
	StrokeRect(bounds);
	SetHighColor(fColor);
	FillRect(bounds.InsetByCopy(1, 1));
}


WindowsSettingsView::WindowsSettingsView()
	:
	SettingsView("WindowsSettingsView"),
	fShowFullPathInTitleBarCheckBox(NULL),
	fSingleWindowBrowseCheckBox(NULL),
	fShowNavigatorCheckBox(NULL),
	fOutlineSelectionCheckBox(NULL),
	fSortFolderNamesFirstCheckBox(NULL),
	fHideDotFilesCheckBox(NULL),
	fTypeAheadFilteringCheckBox(NULL),
	fGenerateImageThumbnailsCheckBox(NULL),
	fShowFullPathInTitleBar(kDefaultShowFullPathInTitleBar),
	fSingleWindowBrowse(kDefaultSingleWindowBrowse),
	fShowNavigator(kDefaultShowNavigator),
	fTransparentSelection(kDefaultTransparentSelection),
	fSortFolderNamesFirst(kDefaultSortFolderNamesFirst),
	fHideDotFiles(kDefaultHideDotFiles),
	fTypeAheadFiltering(kDefaultTypeAheadFiltering),
	fGenerateImageThumbnails(kDefaultGenerateImageThumbnails)
{
	fShowFullPathInTitleBarCheckBox = new BCheckBox("",
		B_TRANSLATE("Show folder location in title tab"),
		new BMessage(kWindowsShowFullPathChanged));

	fSingleWindowBrowseCheckBox = new BCheckBox("",
		B_TRANSLATE("Single window navigation"),
		new BMessage(kSingleWindowBrowseChanged));

	fShowNavigatorCheckBox = new BCheckBox("",
		B_TRANSLATE("Show navigator"),
		new BMessage(kShowNavigatorChanged));

	fOutlineSelectionCheckBox = new BCheckBox("",
		B_TRANSLATE("Outline selection rectangle only"),
		new BMessage(kTransparentSelectionChanged));

	fSortFolderNamesFirstCheckBox = new BCheckBox("",
		B_TRANSLATE("List folders first"),
		new BMessage(kSortFolderNamesFirstChanged));

	fHideDotFilesCheckBox = new BCheckBox("",
		B_TRANSLATE("Hide dotfiles"),
		new BMessage(kHideDotFilesChanged));

	fTypeAheadFilteringCheckBox = new BCheckBox("",
		B_TRANSLATE("Enable type-ahead filtering"),
		new BMessage(kTypeAheadFilteringChanged));

	fGenerateImageThumbnailsCheckBox = new BCheckBox("",
		B_TRANSLATE("Generate image thumbnails"),
		new BMessage(kGenerateImageThumbnailsChanged));

	const float spacing = be_control_look->DefaultItemSpacing();

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.AddGroup(B_VERTICAL, 0)
			.Add(fShowFullPathInTitleBarCheckBox)
			.Add(fSingleWindowBrowseCheckBox)
			.End()
		.AddGroup(B_VERTICAL)
			.Add(fShowNavigatorCheckBox)
			.SetInsets(spacing * 2, 0, 0, 0)
			.End()
		.AddGroup(B_VERTICAL, 0)
			.Add(fOutlineSelectionCheckBox)
			.Add(fSortFolderNamesFirstCheckBox)
			.Add(fHideDotFilesCheckBox)
			.Add(fTypeAheadFilteringCheckBox)
			.Add(fGenerateImageThumbnailsCheckBox)
			.End()
		.AddGlue()
		.SetInsets(spacing);
}


void
WindowsSettingsView::AttachedToWindow()
{
	fSingleWindowBrowseCheckBox->SetTarget(this);
	fShowNavigatorCheckBox->SetTarget(this);
	fShowFullPathInTitleBarCheckBox->SetTarget(this);
	fOutlineSelectionCheckBox->SetTarget(this);
	fSortFolderNamesFirstCheckBox->SetTarget(this);
	fHideDotFilesCheckBox->SetTarget(this);
	fTypeAheadFilteringCheckBox->SetTarget(this);
	fGenerateImageThumbnailsCheckBox->SetTarget(this);
}


void
WindowsSettingsView::MessageReceived(BMessage* message)
{
	TTracker* tracker = dynamic_cast<TTracker*>(be_app);
	if (tracker == NULL)
		return;

	TrackerSettings settings;

	switch (message->what) {
		case kWindowsShowFullPathChanged:
			settings.SetShowFullPathInTitleBar(
				fShowFullPathInTitleBarCheckBox->Value() == 1);
			tracker->SendNotices(kWindowsShowFullPathChanged);
			Window()->PostMessage(kSettingsContentsModified);
			break;

		case kSingleWindowBrowseChanged:
			settings.SetSingleWindowBrowse(
				fSingleWindowBrowseCheckBox->Value() == 1);
			if (fSingleWindowBrowseCheckBox->Value() == 0) {
				fShowNavigatorCheckBox->SetEnabled(false);
				settings.SetShowNavigator(0);
			} else {
				fShowNavigatorCheckBox->SetEnabled(true);
				settings.SetShowNavigator(
					fShowNavigatorCheckBox->Value() != 0);
			}
			tracker->SendNotices(kShowNavigatorChanged);
			tracker->SendNotices(kSingleWindowBrowseChanged);
			Window()->PostMessage(kSettingsContentsModified);
			break;

		case kShowNavigatorChanged:
			settings.SetShowNavigator(fShowNavigatorCheckBox->Value() == 1);
			tracker->SendNotices(kShowNavigatorChanged);
			Window()->PostMessage(kSettingsContentsModified);
			break;

		case kTransparentSelectionChanged:
		{
			settings.SetTransparentSelection(
				fOutlineSelectionCheckBox->Value() == B_CONTROL_OFF);

			// Make the notification message and send it to the tracker:
			send_bool_notices(kTransparentSelectionChanged,
				"TransparentSelection", settings.TransparentSelection());

			Window()->PostMessage(kSettingsContentsModified);
			break;
		}

		case kSortFolderNamesFirstChanged:
		{
			settings.SetSortFolderNamesFirst(
				fSortFolderNamesFirstCheckBox->Value() == 1);

			// Make the notification message and send it to the tracker:
			send_bool_notices(kSortFolderNamesFirstChanged,
				"SortFolderNamesFirst",
				fSortFolderNamesFirstCheckBox->Value() == 1);

			Window()->PostMessage(kSettingsContentsModified);
			break;
		}

		case kHideDotFilesChanged:
		{
			settings.SetHideDotFiles(
				fHideDotFilesCheckBox->Value() == 1);
			send_bool_notices(kHideDotFilesChanged,
				"HideDotFiles",
				fHideDotFilesCheckBox->Value() == 1);
			Window()->PostMessage(kSettingsContentsModified);
			break;
		}

		case kTypeAheadFilteringChanged:
		{
			settings.SetTypeAheadFiltering(
				fTypeAheadFilteringCheckBox->Value() == 1);
			send_bool_notices(kTypeAheadFilteringChanged,
				"TypeAheadFiltering",
				fTypeAheadFilteringCheckBox->Value() == 1);
			Window()->PostMessage(kSettingsContentsModified);
			break;
		}

		case kGenerateImageThumbnailsChanged:
		{
			settings.SetGenerateImageThumbnails(
				fGenerateImageThumbnailsCheckBox->Value() == 1);
			send_bool_notices(kGenerateImageThumbnailsChanged,
				"GenerateImageThumbnails",
				fGenerateImageThumbnailsCheckBox->Value() == 1);
			Window()->PostMessage(kSettingsContentsModified);
			break;
		}

		default:
			_inherited::MessageReceived(message);
			break;
	}
}


void
WindowsSettingsView::SetDefaults()
{
	TTracker* tracker = dynamic_cast<TTracker*>(be_app);
	if (tracker == NULL)
		return;

	TrackerSettings settings;

	if (settings.ShowFullPathInTitleBar() != kDefaultShowFullPathInTitleBar) {
		settings.SetShowFullPathInTitleBar(kDefaultShowFullPathInTitleBar);
		tracker->SendNotices(kWindowsShowFullPathChanged);
	}

	if (settings.SingleWindowBrowse() != kDefaultSingleWindowBrowse) {
		settings.SetSingleWindowBrowse(kDefaultSingleWindowBrowse);
		tracker->SendNotices(kSingleWindowBrowseChanged);
	}

	if (settings.ShowNavigator() != kDefaultShowNavigator) {
		settings.SetShowNavigator(kDefaultShowNavigator);
		tracker->SendNotices(kShowNavigatorChanged);
	}

	if (settings.TransparentSelection() != kDefaultTransparentSelection) {
		settings.SetTransparentSelection(kDefaultTransparentSelection);
		send_bool_notices(kTransparentSelectionChanged,
			"TransparentSelection", kDefaultTransparentSelection);
	}

	if (settings.SortFolderNamesFirst() != kDefaultSortFolderNamesFirst) {
		settings.SetSortFolderNamesFirst(kDefaultSortFolderNamesFirst);
		send_bool_notices(kSortFolderNamesFirstChanged,
			"SortFolderNamesFirst", kDefaultSortFolderNamesFirst);
	}

	if (settings.HideDotFiles() != kDefaultHideDotFiles) {
		settings.SetHideDotFiles(kDefaultHideDotFiles);
		send_bool_notices(kHideDotFilesChanged,
			"HideDotFiles", kDefaultHideDotFiles);
	}

	if (settings.TypeAheadFiltering() != kDefaultTypeAheadFiltering) {
		settings.SetTypeAheadFiltering(kDefaultTypeAheadFiltering);
		send_bool_notices(kTypeAheadFilteringChanged,
			"TypeAheadFiltering", kDefaultTypeAheadFiltering);
	}

	if (settings.GenerateImageThumbnails() != kDefaultGenerateImageThumbnails) {
		settings.SetGenerateImageThumbnails(kDefaultGenerateImageThumbnails);
		send_bool_notices(kGenerateImageThumbnailsChanged,
			"GenerateImageThumbnails", kDefaultGenerateImageThumbnails);
	}


	ShowCurrentSettings();
}


bool
WindowsSettingsView::IsDefaultable() const
{
	TrackerSettings settings;

	return settings.ShowFullPathInTitleBar() != kDefaultShowFullPathInTitleBar
		|| settings.SingleWindowBrowse() != kDefaultSingleWindowBrowse
		|| settings.ShowNavigator() != kDefaultShowNavigator
		|| settings.TransparentSelection() != kDefaultTransparentSelection
		|| settings.SortFolderNamesFirst() != kDefaultSortFolderNamesFirst
		|| settings.HideDotFiles() != kDefaultHideDotFiles
		|| settings.TypeAheadFiltering() != kDefaultTypeAheadFiltering
		|| settings.GenerateImageThumbnails() != kDefaultGenerateImageThumbnails;
}


void
WindowsSettingsView::Revert()
{
	TTracker* tracker = dynamic_cast<TTracker*>(be_app);
	if (tracker == NULL)
		return;

	TrackerSettings settings;

	if (settings.ShowFullPathInTitleBar() != fShowFullPathInTitleBar) {
		settings.SetShowFullPathInTitleBar(fShowFullPathInTitleBar);
		tracker->SendNotices(kWindowsShowFullPathChanged);
	}

	if (settings.SingleWindowBrowse() != fSingleWindowBrowse) {
		settings.SetSingleWindowBrowse(fSingleWindowBrowse);
		tracker->SendNotices(kSingleWindowBrowseChanged);
	}

	if (settings.ShowNavigator() != fShowNavigator) {
		settings.SetShowNavigator(fShowNavigator);
		tracker->SendNotices(kShowNavigatorChanged);
	}

	if (settings.TransparentSelection() != fTransparentSelection) {
		settings.SetTransparentSelection(fTransparentSelection);
		send_bool_notices(kTransparentSelectionChanged,
			"TransparentSelection", fTransparentSelection);
	}

	if (settings.SortFolderNamesFirst() != fSortFolderNamesFirst) {
		settings.SetSortFolderNamesFirst(fSortFolderNamesFirst);
		send_bool_notices(kSortFolderNamesFirstChanged,
			"SortFolderNamesFirst", fSortFolderNamesFirst);
	}

	if (settings.HideDotFiles() != fHideDotFiles) {
		settings.SetSortFolderNamesFirst(fHideDotFiles);
		send_bool_notices(kHideDotFilesChanged,
			"HideDotFiles", fHideDotFiles);
	}

	if (settings.TypeAheadFiltering() != fTypeAheadFiltering) {
		settings.SetTypeAheadFiltering(fTypeAheadFiltering);
		send_bool_notices(kTypeAheadFilteringChanged,
			"TypeAheadFiltering", fTypeAheadFiltering);
	}

	if (settings.GenerateImageThumbnails() != fGenerateImageThumbnails) {
		settings.SetGenerateImageThumbnails(fGenerateImageThumbnails);
		send_bool_notices(kGenerateImageThumbnailsChanged,
			"GenerateImageThumbnails", fGenerateImageThumbnails);
	}


	ShowCurrentSettings();
}


void
WindowsSettingsView::ShowCurrentSettings()
{
	TrackerSettings settings;

	fShowFullPathInTitleBarCheckBox->SetValue(
		settings.ShowFullPathInTitleBar());
	fSingleWindowBrowseCheckBox->SetValue(settings.SingleWindowBrowse());
	fShowNavigatorCheckBox->SetEnabled(settings.SingleWindowBrowse());
	fShowNavigatorCheckBox->SetValue(settings.ShowNavigator());
	fOutlineSelectionCheckBox->SetValue(settings.TransparentSelection()
		? B_CONTROL_OFF : B_CONTROL_ON);
	fSortFolderNamesFirstCheckBox->SetValue(settings.SortFolderNamesFirst());
	fHideDotFilesCheckBox->SetValue(settings.HideDotFiles());
	fTypeAheadFilteringCheckBox->SetValue(settings.TypeAheadFiltering());
	fGenerateImageThumbnailsCheckBox->SetValue(
		settings.GenerateImageThumbnails());
}


void
WindowsSettingsView::RecordRevertSettings()
{
	TrackerSettings settings;

	fShowFullPathInTitleBar = settings.ShowFullPathInTitleBar();
	fSingleWindowBrowse = settings.SingleWindowBrowse();
	fShowNavigator = settings.ShowNavigator();
	fTransparentSelection = settings.TransparentSelection();
	fSortFolderNamesFirst = settings.SortFolderNamesFirst();
	fHideDotFiles = settings.HideDotFiles();
	fTypeAheadFiltering = settings.TypeAheadFiltering();
	fGenerateImageThumbnails = settings.GenerateImageThumbnails();
}


bool
WindowsSettingsView::IsRevertable() const
{
	TrackerSettings settings;

	return fShowFullPathInTitleBar != settings.ShowFullPathInTitleBar()
		|| fSingleWindowBrowse != settings.SingleWindowBrowse()
		|| fShowNavigator != settings.ShowNavigator()
		|| fTransparentSelection != settings.TransparentSelection()
		|| fSortFolderNamesFirst != settings.SortFolderNamesFirst()
		|| fHideDotFiles != settings.HideDotFiles()
		|| fTypeAheadFiltering != settings.TypeAheadFiltering()
		|| fGenerateImageThumbnails != settings.GenerateImageThumbnails();
}


// #pragma mark - SnakeSettingsView


// A "[?]" that says what an option does when the pointer rests on it.
static BStringView*
HelpMark(const char* text)
{
	BStringView* mark = new BStringView("", "[?]");
	mark->SetToolTip(text);
	return mark;
}


// Restarts Tracker and the Deskbar (these builds): a helper that outlives them starts both again, then the Deskbar is told to
// quit and Tracker ends itself.
static void
RestartSnakeTracker()
{
	app_info info;
	if (be_app == NULL || be_app->GetAppInfo(&info) != B_OK)
		return;
	BPath tracker(&info.ref);
	BPath folder;
	if (tracker.GetParent(&folder) != B_OK)
		return;
	BPath deskbar(folder.Path(), "Deskbar");

	BString command;
	command << "(sleep 2; '" << deskbar.Path() << "' >/dev/null 2>&1 & '" << tracker.Path()
		<< "' >/dev/null 2>&1 &) >/dev/null 2>&1 &";
	system(command.String());

	BMessenger deskbarTeam("application/x-vnd.Be-TSKB");
	if (deskbarTeam.IsValid()) {
		team_id team = deskbarTeam.Team();
		deskbarTeam.SendMessage(B_QUIT_REQUESTED);
		snooze(300000);
		kill_team(team);
	}
	kill_team(be_app->Team());
}


SnakeSettingsView::SnakeSettingsView()
	:
	SettingsView("SnakeSettingsView"),
	fSnakeLookCheckBox(NULL),
	fSnakeTrailCheckBox(NULL),
	fSnakeFlatCheckBox(NULL),
	fSnakeArrowsCheckBox(NULL),
	fSnakeRoundCheckBox(NULL),
	fSnakeSysDeskbarCheckBox(NULL),
	fSnakeSysTrackerCheckBox(NULL),
	fSnakeAllAppsCheckBox(NULL),
	fSnakeAccentControl(NULL),
	fSnakeAccentSwatch(NULL),
	fRestartButton(NULL),
	fSnakeLook(kDefaultSnakeLook),
	fSnakeTrail(kDefaultSnakeTrail),
	fSnakeFlat(kDefaultSnakeFlat),
	fSnakeArrows(kDefaultSnakeArrows),
	fSnakeRound(kDefaultSnakeRoundCorners),
	fSnakeSysDeskbar(kDefaultSnakeSystemColorsDeskbar),
	fSnakeSysTracker(kDefaultSnakeSystemColorsTracker),
	fSnakeAllApps(kDefaultSnakeAllApps),
	fSnakeAccent(kDefaultSnakeAccent)
{
	fSnakeLookCheckBox = new BCheckBox("",
		B_TRANSLATE("Raised Snake look for Tracker and Deskbar"), new BMessage(kSnakeLookChanged));
	fSnakeTrailCheckBox = new BCheckBox("",
		B_TRANSLATE("Snake trail in folder menus"), new BMessage(kSnakeTrailChanged));
	fSnakeFlatCheckBox = new BCheckBox("",
		B_TRANSLATE("Flat menu selector"), new BMessage(kSnakeFlatChanged));
	fSnakeArrowsCheckBox = new BCheckBox("",
		B_TRANSLATE("Scroll bar arrows"), new BMessage(kSnakeArrowsChanged));
	fSnakeRoundCheckBox = new BCheckBox("",
		B_TRANSLATE("Enable rounded corners on Tracker & Deskbar"), new BMessage(kSnakeRoundChanged));
	fSnakeSysDeskbarCheckBox = new BCheckBox("",
		B_TRANSLATE("Use system colors for Deskbar"), new BMessage(kSnakeSysDeskbarChanged));
	fSnakeSysTrackerCheckBox = new BCheckBox("",
		B_TRANSLATE("Use system colors for Tracker"), new BMessage(kSnakeSysTrackerChanged));
	fSnakeAllAppsCheckBox = new BCheckBox("",
		B_TRANSLATE("Snake look in all programs & menus"), new BMessage(kSnakeAllAppsChanged));

	fSnakeAccentControl = new BColorControl(BPoint(0, 0), B_CELLS_32x8, 4,
		"snake_accent", new BMessage(kSnakeAccentChanged));
	fSnakeAccentSwatch = new SnakeColorSwatch("snake_accent_swatch");

	fRestartButton = new BButton("", B_TRANSLATE("Restart\nSnakeTracker"),
		new BMessage(kSnakeRestart));

	const float spacing = be_control_look->DefaultItemSpacing();

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.AddGroup(B_VERTICAL, 0)
			.AddGroup(B_HORIZONTAL, spacing / 2).Add(fSnakeLookCheckBox)
				.Add(HelpMark(B_TRANSLATE("Draws the menus of Tracker and the Deskbar, the Deskbar's panel\nand program tabs, and the menu bar titles in the raised\nSnakeTracker pill look. Off, they are drawn the way Haiku\ndraws them, and rounded corners are not available; the\nsnake trail and the buttons, scroll bars and the like stay.")))
				.AddGlue().End()
			.AddGroup(B_HORIZONTAL, spacing / 2).Add(fSnakeTrailCheckBox)
				.Add(HelpMark(B_TRANSLATE("The highlight stays joined through every open submenu\nof a folder menu, like a snake.")))
				.AddGlue().End()
			.AddGroup(B_HORIZONTAL, spacing / 2).Add(fSnakeFlatCheckBox)
				.Add(HelpMark(B_TRANSLATE("Draws the menu selector in one flat color. Off, it has\na light top edge and a dark bottom edge.")))
				.AddGlue().End()
			.AddGroup(B_HORIZONTAL, spacing / 2).Add(fSnakeArrowsCheckBox)
				.Add(HelpMark(B_TRANSLATE("Shows the arrow buttons at the ends of scroll bars.")))
				.AddGlue().End()
			.AddGroup(B_HORIZONTAL, spacing / 2).Add(fSnakeRoundCheckBox)
				.Add(HelpMark(B_TRANSLATE("Rounds the corners of the menus of Tracker and the Deskbar,\nand the outer corners of the Deskbar's own window (not those\nat the edge of the screen). Haiku's square window outline is\ncovered with the screen behind it, so the wallpaper or the\nwindow behind shows through the corners. Off, the corners\nare square and have an outline. Needs the raised Snake look.")))
				.AddGlue().End()
			.AddGroup(B_HORIZONTAL, spacing / 2).Add(fSnakeSysDeskbarCheckBox)
				.Add(HelpMark(B_TRANSLATE("On: the Deskbar uses the colors of Haiku's Appearance\nsettings (the menu background and selection colors).\nOff: it uses the SnakeTracker color.")))
				.AddGlue().End()
			.AddGroup(B_HORIZONTAL, spacing / 2).Add(fSnakeSysTrackerCheckBox)
				.Add(HelpMark(B_TRANSLATE("On: Tracker uses the colors of Haiku's Appearance\nsettings (the menu background and selection colors).\nOff: it uses the SnakeTracker color.")))
				.AddGlue().End()
			.AddGroup(B_HORIZONTAL, spacing / 2).Add(fSnakeAllAppsCheckBox)
				.Add(HelpMark(B_TRANSLATE("The pill look for the menus of every program that uses\nHaiku's own menus, in Haiku's Appearance colors, with\nsquare corners. Programs that draw their own menus do\nnot change. Menus pick it up as they open.")))
				.AddGlue().End()
			.AddGroup(B_HORIZONTAL, spacing / 2)
				.Add(new BStringView("", B_TRANSLATE("SnakeTracker color:")))
				.Add(fSnakeAccentSwatch)
				.AddGlue()
				.End()
			.Add(fSnakeAccentControl)
			.End()
		.AddStrut(spacing)
		.AddGroup(B_HORIZONTAL, spacing / 2)
			.Add(fRestartButton)
			.Add(HelpMark(B_TRANSLATE("Restarts Tracker and the Deskbar so that every setting\ntakes effect. Open folder windows close and open again.")))
			.AddGlue()
			.End()
		.AddGlue()
		.SetInsets(spacing);
}


void
SnakeSettingsView::AttachedToWindow()
{
	fSnakeLookCheckBox->SetTarget(this);
	fSnakeTrailCheckBox->SetTarget(this);
	fSnakeFlatCheckBox->SetTarget(this);
	fSnakeArrowsCheckBox->SetTarget(this);
	fSnakeRoundCheckBox->SetTarget(this);
	fSnakeSysDeskbarCheckBox->SetTarget(this);
	fSnakeSysTrackerCheckBox->SetTarget(this);
	fSnakeAllAppsCheckBox->SetTarget(this);
	fSnakeAccentControl->SetTarget(this);
	fRestartButton->SetTarget(this);
}


void
SnakeSettingsView::MessageReceived(BMessage* message)
{
	TrackerSettings settings;

	switch (message->what) {
		case kSnakeTrailChanged:
			settings.SetSnakeTrail(fSnakeTrailCheckBox->Value() == 1);
			Window()->PostMessage(kSettingsContentsModified);
			break;

		case kSnakeFlatChanged:
			settings.SetSnakeFlat(fSnakeFlatCheckBox->Value() == 1);
			Window()->PostMessage(kSettingsContentsModified);
			break;

		case kSnakeLookChanged:
			settings.SetSnakeLook(fSnakeLookCheckBox->Value() == 1);
			// (rounded corners belong to the raised look: without it there is nothing to round)
			fSnakeRoundCheckBox->SetEnabled(fSnakeLookCheckBox->Value() == 1);
			// (the flat selector is the plain look's: the raised look shades the selector itself)
			fSnakeFlatCheckBox->SetEnabled(fSnakeLookCheckBox->Value() != 1);
			Window()->PostMessage(kSettingsContentsModified);
			break;

		case kSnakeArrowsChanged:
			settings.SetSnakeArrows(fSnakeArrowsCheckBox->Value() == 1);
			Window()->PostMessage(kSettingsContentsModified);
			break;

		case kSnakeRoundChanged:
			settings.SetSnakeRoundCorners(fSnakeRoundCheckBox->Value() == 1);
			Window()->PostMessage(kSettingsContentsModified);
			break;

		case kSnakeSysDeskbarChanged:
			settings.SetSnakeSystemColorsDeskbar(fSnakeSysDeskbarCheckBox->Value() == 1);
			Window()->PostMessage(kSettingsContentsModified);
			break;

		case kSnakeSysTrackerChanged:
			settings.SetSnakeSystemColorsTracker(fSnakeSysTrackerCheckBox->Value() == 1);
			Window()->PostMessage(kSettingsContentsModified);
			break;

		case kSnakeAllAppsChanged:
			settings.SetSnakeAllApps(fSnakeAllAppsCheckBox->Value() == 1);
			Window()->PostMessage(kSettingsContentsModified);
			break;

		case kSnakeAccentChanged:
			settings.SetSnakeAccent(fSnakeAccentControl->ValueAsColor());
			fSnakeAccentSwatch->SetColor(fSnakeAccentControl->ValueAsColor());
			Window()->PostMessage(kSettingsContentsModified);
			break;

		case kSnakeRestart:
			RestartSnakeTracker();
			break;

		default:
			_inherited::MessageReceived(message);
			break;
	}
}


void
SnakeSettingsView::SetDefaults()
{
	TrackerSettings settings;
	settings.SetSnakeTrail(kDefaultSnakeTrail);
	settings.SetSnakeFlat(kDefaultSnakeFlat);
	settings.SetSnakeLook(kDefaultSnakeLook);
	settings.SetSnakeArrows(kDefaultSnakeArrows);
	settings.SetSnakeRoundCorners(kDefaultSnakeRoundCorners);
	settings.SetSnakeSystemColorsDeskbar(kDefaultSnakeSystemColorsDeskbar);
	settings.SetSnakeSystemColorsTracker(kDefaultSnakeSystemColorsTracker);
	settings.SetSnakeAllApps(kDefaultSnakeAllApps);
	settings.SetSnakeAccent(kDefaultSnakeAccent);
	ShowCurrentSettings();
}


bool
SnakeSettingsView::IsDefaultable() const
{
	TrackerSettings settings;
	return settings.SnakeTrail() != kDefaultSnakeTrail
		|| settings.SnakeFlat() != kDefaultSnakeFlat
		|| settings.SnakeLook() != kDefaultSnakeLook
		|| settings.SnakeArrows() != kDefaultSnakeArrows
		|| settings.SnakeRoundCorners() != kDefaultSnakeRoundCorners
		|| settings.SnakeSystemColorsDeskbar() != kDefaultSnakeSystemColorsDeskbar
		|| settings.SnakeSystemColorsTracker() != kDefaultSnakeSystemColorsTracker
		|| settings.SnakeAllApps() != kDefaultSnakeAllApps
		|| _ColorValue(settings.SnakeAccent()) != _ColorValue(kDefaultSnakeAccent);
}


void
SnakeSettingsView::Revert()
{
	TrackerSettings settings;
	settings.SetSnakeTrail(fSnakeTrail);
	settings.SetSnakeFlat(fSnakeFlat);
	settings.SetSnakeLook(fSnakeLook);
	settings.SetSnakeArrows(fSnakeArrows);
	settings.SetSnakeRoundCorners(fSnakeRound);
	settings.SetSnakeSystemColorsDeskbar(fSnakeSysDeskbar);
	settings.SetSnakeSystemColorsTracker(fSnakeSysTracker);
	settings.SetSnakeAllApps(fSnakeAllApps);
	settings.SetSnakeAccent(fSnakeAccent);
	ShowCurrentSettings();
}


void
SnakeSettingsView::ShowCurrentSettings()
{
	TrackerSettings settings;
	fSnakeTrailCheckBox->SetValue(settings.SnakeTrail());
	fSnakeFlatCheckBox->SetValue(settings.SnakeFlat());
	fSnakeLookCheckBox->SetValue(settings.SnakeLook());
	fSnakeArrowsCheckBox->SetValue(settings.SnakeArrows());
	fSnakeRoundCheckBox->SetValue(settings.SnakeRoundCorners());
	fSnakeRoundCheckBox->SetEnabled(settings.SnakeLook());
	fSnakeFlatCheckBox->SetEnabled(!settings.SnakeLook());
	fSnakeSysDeskbarCheckBox->SetValue(settings.SnakeSystemColorsDeskbar());
	fSnakeSysTrackerCheckBox->SetValue(settings.SnakeSystemColorsTracker());
	fSnakeAllAppsCheckBox->SetValue(settings.SnakeAllApps());
	fSnakeAccentControl->SetValue(settings.SnakeAccent());
	fSnakeAccentSwatch->SetColor(settings.SnakeAccent());
}


void
SnakeSettingsView::RecordRevertSettings()
{
	TrackerSettings settings;
	fSnakeTrail = settings.SnakeTrail();
	fSnakeFlat = settings.SnakeFlat();
	fSnakeLook = settings.SnakeLook();
	fSnakeArrows = settings.SnakeArrows();
	fSnakeRound = settings.SnakeRoundCorners();
	fSnakeSysDeskbar = settings.SnakeSystemColorsDeskbar();
	fSnakeSysTracker = settings.SnakeSystemColorsTracker();
	fSnakeAllApps = settings.SnakeAllApps();
	fSnakeAccent = settings.SnakeAccent();
}


bool
SnakeSettingsView::IsRevertable() const
{
	TrackerSettings settings;
	return fSnakeTrail != settings.SnakeTrail()
		|| fSnakeFlat != settings.SnakeFlat()
		|| fSnakeLook != settings.SnakeLook()
		|| fSnakeArrows != settings.SnakeArrows()
		|| fSnakeRound != settings.SnakeRoundCorners()
		|| fSnakeSysDeskbar != settings.SnakeSystemColorsDeskbar()
		|| fSnakeSysTracker != settings.SnakeSystemColorsTracker()
		|| fSnakeAllApps != settings.SnakeAllApps()
		|| _ColorValue(fSnakeAccent) != _ColorValue(settings.SnakeAccent());
}


// #pragma mark - SpaceBarSettingsView


SpaceBarSettingsView::SpaceBarSettingsView()
	:
	SettingsView("SpaceBarSettingsView")
{
	fSpaceBarShowCheckBox = new BCheckBox("",
		B_TRANSLATE("Show space bars on volumes"),
		new BMessage(kUpdateVolumeSpaceBar));

	BPopUpMenu* menu = new BPopUpMenu(B_EMPTY_STRING);
	menu->SetFont(be_plain_font);

	BMenuItem* item;
	menu->AddItem(item = new BMenuItem(
		B_TRANSLATE("Used space color"),
		new BMessage(kSpaceBarSwitchColor)));
	item->SetMarked(true);
	fCurrentColor = 0;
	menu->AddItem(new BMenuItem(
		B_TRANSLATE("Free space color"),
		new BMessage(kSpaceBarSwitchColor)));
	menu->AddItem(new BMenuItem(
		B_TRANSLATE("Warning space color"),
		new BMessage(kSpaceBarSwitchColor)));

	fColorPicker = new BMenuField("menu", NULL, menu);

	fColorControl = new BColorControl(BPoint(0, 0),
		B_CELLS_16x16, 1, "SpaceColorControl",
		new BMessage(kSpaceBarColorChanged));
	fColorControl->SetValue(TrackerSettings().UsedSpaceColor());

	BBox* box = new BBox("box");
	box->SetLabel(fColorPicker);
	box->AddChild(BLayoutBuilder::Group<>(B_HORIZONTAL)
		.Add(fColorControl)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.View());

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.Add(fSpaceBarShowCheckBox)
		.Add(box)
		.AddGlue()
		.SetInsets(B_USE_DEFAULT_SPACING);
}


SpaceBarSettingsView::~SpaceBarSettingsView()
{
}


void
SpaceBarSettingsView::AttachedToWindow()
{
	fSpaceBarShowCheckBox->SetTarget(this);
	fColorControl->SetTarget(this);
	fColorPicker->Menu()->SetTargetForItems(this);
}


void
SpaceBarSettingsView::MessageReceived(BMessage* message)
{
	TTracker* tracker = dynamic_cast<TTracker*>(be_app);
	if (tracker == NULL)
		return;

	TrackerSettings settings;

	switch (message->what) {
		case kUpdateVolumeSpaceBar:
		{
			settings.SetShowVolumeSpaceBar(
				fSpaceBarShowCheckBox->Value() == 1);
			Window()->PostMessage(kSettingsContentsModified);
			tracker->PostMessage(kShowVolumeSpaceBar);
			break;
		}

		case kSpaceBarSwitchColor:
		{
			fCurrentColor = message->FindInt32("index");
			switch (fCurrentColor) {
				case 0:
					fColorControl->SetValue(settings.UsedSpaceColor());
					break;

				case 1:
					fColorControl->SetValue(settings.FreeSpaceColor());
					break;

				case 2:
					fColorControl->SetValue(settings.WarningSpaceColor());
					break;
			}
			break;
		}

		case kSpaceBarColorChanged:
		{
			rgb_color color = fColorControl->ValueAsColor();
			color.alpha = kDefaultSpaceBarAlpha;
				// alpha is ignored by BColorControl but is checked
				// in equalities

			switch (fCurrentColor) {
				case 0:
					settings.SetUsedSpaceColor(color);
					break;

				case 1:
					settings.SetFreeSpaceColor(color);
					break;

				case 2:
					settings.SetWarningSpaceColor(color);
					break;
			}

			BWindow* window = Window();
			if (window != NULL)
				window->PostMessage(kSettingsContentsModified);

			tracker->PostMessage(kSpaceBarColorChanged);
			break;
		}

		default:
			_inherited::MessageReceived(message);
			break;
	}
}


void
SpaceBarSettingsView::SetDefaults()
{
	TTracker* tracker = dynamic_cast<TTracker*>(be_app);
	if (tracker == NULL)
		return;

	TrackerSettings settings;

	if (!settings.ShowVolumeSpaceBar()) {
		settings.SetShowVolumeSpaceBar(true);
		send_bool_notices(kShowVolumeSpaceBar, "ShowVolumeSpaceBar", kDefaultShowVolumeSpaceBar);
	}

	if (settings.UsedSpaceColor() != kDefaultUsedSpaceColor
		|| settings.FreeSpaceColor() != kDefaultFreeSpaceColor
		|| settings.WarningSpaceColor() != kDefaultWarningSpaceColor) {
		settings.SetUsedSpaceColor(kDefaultUsedSpaceColor);
		settings.SetFreeSpaceColor(kDefaultFreeSpaceColor);
		settings.SetWarningSpaceColor(kDefaultWarningSpaceColor);
		tracker->SendNotices(kSpaceBarColorChanged);
	}

	ShowCurrentSettings();
}


bool
SpaceBarSettingsView::IsDefaultable() const
{
	TrackerSettings settings;

	return settings.ShowVolumeSpaceBar() != kDefaultShowVolumeSpaceBar
		|| settings.UsedSpaceColor() != kDefaultUsedSpaceColor
		|| settings.FreeSpaceColor() != kDefaultFreeSpaceColor
		|| settings.WarningSpaceColor() != kDefaultWarningSpaceColor;
}


void
SpaceBarSettingsView::Revert()
{
	TTracker* tracker = dynamic_cast<TTracker*>(be_app);
	if (tracker == NULL)
		return;

	TrackerSettings settings;

	if (settings.ShowVolumeSpaceBar() != fSpaceBarShow) {
		settings.SetShowVolumeSpaceBar(fSpaceBarShow);
		send_bool_notices(kShowVolumeSpaceBar, "ShowVolumeSpaceBar",
			fSpaceBarShow);
	}

	if (settings.UsedSpaceColor() != fUsedSpaceColor
		|| settings.FreeSpaceColor() != fFreeSpaceColor
		|| settings.WarningSpaceColor() != fWarningSpaceColor) {
		settings.SetUsedSpaceColor(fUsedSpaceColor);
		settings.SetFreeSpaceColor(fFreeSpaceColor);
		settings.SetWarningSpaceColor(fWarningSpaceColor);
		tracker->SendNotices(kSpaceBarColorChanged);
	}

	ShowCurrentSettings();
}


void
SpaceBarSettingsView::ShowCurrentSettings()
{
	TrackerSettings settings;

	fSpaceBarShowCheckBox->SetValue(settings.ShowVolumeSpaceBar());

	switch (fCurrentColor) {
		case 0:
			fColorControl->SetValue(settings.UsedSpaceColor());
			break;
		case 1:
			fColorControl->SetValue(settings.FreeSpaceColor());
			break;
		case 2:
			fColorControl->SetValue(settings.WarningSpaceColor());
			break;
	}
}


void
SpaceBarSettingsView::RecordRevertSettings()
{
	TrackerSettings settings;

	fSpaceBarShow = settings.ShowVolumeSpaceBar();
	fUsedSpaceColor = settings.UsedSpaceColor();
	fFreeSpaceColor = settings.FreeSpaceColor();
	fWarningSpaceColor = settings.WarningSpaceColor();
}


bool
SpaceBarSettingsView::IsRevertable() const
{
	TrackerSettings settings;

	return fSpaceBarShow != settings.ShowVolumeSpaceBar()
		|| fUsedSpaceColor != settings.UsedSpaceColor()
		|| fFreeSpaceColor != settings.FreeSpaceColor()
		|| fWarningSpaceColor != settings.WarningSpaceColor();
}
