/*
 * The "SnakeTracker Wallpaper" window: see SnakeWallpaperWindow.h.
 */


#include "SnakeWallpaperWindow.h"

#include <Alert.h>
#include <Button.h>
#include <Catalog.h>
#include <CheckBox.h>
#include <Directory.h>
#include <Entry.h>
#include <FilePanel.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Locale.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <app/LaunchRoster.h>
#include <Messenger.h>
#include <Node.h>
#include <fs_attr.h>
#include <PopUpMenu.h>
#include <Roster.h>
#include <Path.h>
#include <FindDirectory.h>
#include <ScrollView.h>
#include <StringItem.h>
#include <StringView.h>
#include <TextControl.h>

#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "SnakeWallpaper.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SnakeWallpaperWindow"


namespace BPrivate {

static const uint32 kMsgChanged = 'wpCh';
static const uint32 kMsgAddFolder = 'wpAd';
static const uint32 kMsgRemoveFolder = 'wpRm';
static const uint32 kMsgNextNow = 'wpNx';
static const uint32 kMsgFolderPicked = 'wpFp';
static const uint32 kMsgSelection = 'wpSl';
static const uint32 kMsgKeyEdited = 'wpKy';
static const uint32 kMsgPickSvg = 'wpSv';
static const uint32 kMsgSvgPicked = 'wpSp';

static const char* const kKeyNames[4] = { "flickrkey", "unsplashkey", "pexelskey", "pixabaykey" };

static SnakeWallpaperWindow* sWindow = NULL;

static const int32 kIntervals[] = { 1, 5, 10, 15, 30, 60, 120, 180, 360, 720, 1440 };
static const int32 kIntervalCount = sizeof(kIntervals) / sizeof(kIntervals[0]);
static const int32 kModes[] = { -1, 2, 1, 3 };
static const int32 kModeCount = sizeof(kModes) / sizeof(kModes[0]);


static BString
IntervalLabel(int32 minutes)
{
	BString label;
	if (minutes < 60)
		label << minutes << (minutes == 1 ? " minute" : " minutes");
	else if (minutes < 1440)
		label << minutes / 60 << (minutes == 60 ? " hour" : " hours");
	else
		label << "day";
	return label;
}


static BPopUpMenu*
BuildMenu(const char* name)
{
	BPopUpMenu* menu = new BPopUpMenu(name);
	menu->SetLabelFromMarked(true);
	return menu;
}


static void
AddChoice(BPopUpMenu* menu, const char* label, int32 value)
{
	BMessage* message = new BMessage(kMsgChanged);
	message->AddInt32("value", value);
	menu->AddItem(new BMenuItem(label, message));
}


static void
Mark(BPopUpMenu* menu, int32 value)
{
	for (int32 i = 0; i < menu->CountItems(); i++) {
		int32 v;
		BMenuItem* item = menu->ItemAt(i);
		if (item->Message() != NULL && item->Message()->FindInt32("value", &v) == B_OK && v == value) {
			item->SetMarked(true);
			return;
		}
	}
	if (menu->CountItems() > 0)
		menu->ItemAt(0)->SetMarked(true);
}


static int32
Marked(BPopUpMenu* menu)
{
	BMenuItem* item = menu->FindMarked();
	int32 value = 0;
	if (item != NULL && item->Message() != NULL)
		item->Message()->FindInt32("value", &value);
	return value;
}


void
SnakeWallpaperWindow::Open()
{
	if (sWindow != NULL) {
		if (sWindow->Lock()) {
			// (the window follows to the workspace it is asked for on, and shows that workspace's picture)
			sWindow->SetWorkspaces(B_CURRENT_WORKSPACE);
			sWindow->_ShowCurrent();
			sWindow->Activate();
			sWindow->Unlock();
		}
		return;
	}
	sWindow = new SnakeWallpaperWindow();
	sWindow->Show();
}


SnakeWallpaperWindow::SnakeWallpaperWindow()
	:
	BWindow(BRect(120, 120, 560, 480), B_TRANSLATE("SnakeTracker Wallpaper"), B_TITLED_WINDOW,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS),
	fSvgPanel(NULL),
	fPanel(NULL)
{
	fEnable = new BCheckBox("enable", B_TRANSLATE("Change the wallpaper automatically"), new BMessage(kMsgChanged));

	fFolders = new BListView("folders");
	fFolders->SetSelectionMessage(new BMessage(kMsgSelection));
	BScrollView* scroll = new BScrollView("scroll", fFolders, 0, false, true);
	scroll->SetExplicitMinSize(BSize(360, 80));

	fAdd = new BButton("add", B_TRANSLATE("Add folder" B_UTF8_ELLIPSIS), new BMessage(kMsgAddFolder));
	fRemove = new BButton("remove", B_TRANSLATE("Remove"), new BMessage(kMsgRemoveFolder));

	fIntervalMenu = BuildMenu("interval");
	for (int32 i = 0; i < kIntervalCount; i++)
		AddChoice(fIntervalMenu, IntervalLabel(kIntervals[i]).String(), kIntervals[i]);
	fOrderMenu = BuildMenu("order");
	AddChoice(fOrderMenu, B_TRANSLATE("Random"), 1);
	AddChoice(fOrderMenu, B_TRANSLATE("One after another"), 0);
	fWorkspaceMenu = BuildMenu("workspaces");
	AddChoice(fWorkspaceMenu, B_TRANSLATE("The same on all workspaces"), 0);
	AddChoice(fWorkspaceMenu, B_TRANSLATE("A different one on each workspace"), 1);
	fModeMenu = BuildMenu("placement");
	AddChoice(fModeMenu, B_TRANSLATE("As it is now"), -1);
	AddChoice(fModeMenu, B_TRANSLATE("Scale to fit"), 2);
	AddChoice(fModeMenu, B_TRANSLATE("Center"), 1);
	AddChoice(fModeMenu, B_TRANSLATE("Tile"), 3);

	static const char* const kOnlineLabels[9] = { "Bing: picture of the day", "Wikimedia Commons: picture of the day",
		"Wallhaven: most popular of the day", "Picsum: random photos", "Art Institute of Chicago: paintings",
		"Flickr: interesting pictures", "Unsplash: random landscapes", "Pexels: curated photos",
		"Pixabay: editor's choice" };
	for (int i = 0; i < 9; i++)
		fOnline[i] = new BCheckBox(kOnlineSources[i], kOnlineLabels[i], new BMessage(kMsgChanged));
	static const char* const kKeyHints[4] = { "your Flickr API key", "your Unsplash access key", "your Pexels API key",
		"your Pixabay API key" };
	for (int i = 0; i < 4; i++) {
		fKey[i] = new BTextControl(kKeyNames[i], "", "", new BMessage(kMsgKeyEdited));
		fKey[i]->SetModificationMessage(new BMessage(kMsgKeyEdited));
		fKey[i]->TextView()->SetExplicitMinSize(BSize(190, B_SIZE_UNSET));
		fKey[i]->SetToolTip(kKeyHints[i]);
	}
	fCurrent = new BStringView("current", "");
	// (a long title and credit are cut off at the end, not made the window wider; the tooltip has all of it)
	fCurrent->SetTruncation(B_TRUNCATE_END);
	fCurrent->SetExplicitMinSize(BSize(100, B_SIZE_UNSET));

	fFadeMenu = BuildMenu("fade");
	AddChoice(fFadeMenu, B_TRANSLATE("None"), 0);
	AddChoice(fFadeMenu, B_TRANSLATE("Half a second"), 500);
	AddChoice(fFadeMenu, B_TRANSLATE("One second"), 1000);
	AddChoice(fFadeMenu, B_TRANSLATE("Two seconds"), 2000);
	fEffectMenu = BuildMenu("effect");
	AddChoice(fEffectMenu, B_TRANSLATE("Fade"), 0);
	AddChoice(fEffectMenu, B_TRANSLATE("Wipe"), 1);
	AddChoice(fEffectMenu, B_TRANSLATE("Slide"), 2);
	AddChoice(fEffectMenu, B_TRANSLATE("Circle reveal"), 3);

	fSvg = new BCheckBox("svg", B_TRANSLATE("SVG wallpaper in the accent color (replaces the pictures)"),
		new BMessage(kMsgChanged));
	fSvg->SetToolTip(B_TRANSLATE("A drawing that is made again in the accent color whenever that changes (the SnakeTracker\n"
		"color of the Tracker preferences, or Haiku's selection color). It needs the librsvg package."));
	fSvgMenu = BuildMenu("svg template");
	{
		static const char* const kTemplates[3][2] = { { "builtin:waves", "Waves" }, { "builtin:bubbles", "Bubbles" },
			{ "builtin:bands", "Bands" } };
		for (int i = 0; i < 3; i++) {
			BMessage* message = new BMessage(kMsgChanged);
			message->AddString("svgfile", kTemplates[i][0]);
			message->AddInt32("value", i);
			fSvgMenu->AddItem(new BMenuItem(B_TRANSLATE_NOCOLLECT(kTemplates[i][1]), message));
		}
		fSvgMenu->AddSeparatorItem();
		fSvgMenu->AddItem(new BMenuItem(B_TRANSLATE("Other file" B_UTF8_ELLIPSIS), new BMessage(kMsgPickSvg)));
	}

	fNext = new BButton("next", B_TRANSLATE("Next wallpaper"), new BMessage(kMsgNextNow));
	BButton* close = new BButton("close", B_TRANSLATE("Close"), new BMessage(B_QUIT_REQUESTED));

	BMenuField* interval = new BMenuField(B_TRANSLATE("Change every:"), fIntervalMenu);
	BMenuField* order = new BMenuField(B_TRANSLATE("Order:"), fOrderMenu);
	BMenuField* workspaces = new BMenuField(B_TRANSLATE("Workspaces:"), fWorkspaceMenu);
	BMenuField* mode = new BMenuField(B_TRANSLATE("Placement:"), fModeMenu);
	BMenuField* fade = new BMenuField(B_TRANSLATE("Transition:"), fFadeMenu);
	BMenuField* effect = new BMenuField(B_TRANSLATE("Effect:"), fEffectMenu);
	BMenuField* svgTemplate = new BMenuField(B_TRANSLATE("Template:"), fSvgMenu);

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(fEnable)
		.AddGroup(B_VERTICAL, B_USE_SMALL_SPACING)
			.Add(new BStringView("folders label", B_TRANSLATE("Pictures come from these folders (and the folders in them):")))
			.Add(scroll)
			.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
				.Add(fAdd)
				.Add(fRemove)
				.AddGlue()
				.End()
			.End()
		.AddGroup(B_VERTICAL, B_USE_SMALL_SPACING)
			.Add(new BStringView("online label", B_TRANSLATE("Pictures from the internet (fetched once a day):")))
			.AddGrid(B_USE_DEFAULT_SPACING, 0)
				.Add(fOnline[0], 0, 0)
				.Add(fOnline[1], 0, 1)
				.Add(fOnline[2], 0, 2)
				.Add(fOnline[3], 0, 3)
				.Add(fOnline[4], 0, 4)
				.Add(fOnline[5], 0, 5).Add(fKey[0], 1, 5)
				.Add(fOnline[6], 0, 6).Add(fKey[1], 1, 6)
				.Add(fOnline[7], 0, 7).Add(fKey[2], 1, 7)
				.Add(fOnline[8], 0, 8).Add(fKey[3], 1, 8)
				.End()
			.Add(new BStringView("key note", B_TRANSLATE("Flickr, Unsplash, Pexels and Pixabay need a (free) key of your own.")))
			.End()
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.Add(fSvg)
			.AddGlue()
			.Add(svgTemplate)
			.End()
		.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
			.Add(interval->CreateLabelLayoutItem(), 0, 0)
			.Add(interval->CreateMenuBarLayoutItem(), 1, 0)
			.Add(order->CreateLabelLayoutItem(), 0, 1)
			.Add(order->CreateMenuBarLayoutItem(), 1, 1)
			.Add(workspaces->CreateLabelLayoutItem(), 0, 2)
			.Add(workspaces->CreateMenuBarLayoutItem(), 1, 2)
			.Add(mode->CreateLabelLayoutItem(), 0, 3)
			.Add(mode->CreateMenuBarLayoutItem(), 1, 3)
			.Add(fade->CreateLabelLayoutItem(), 0, 4)
			.Add(fade->CreateMenuBarLayoutItem(), 1, 4)
			.Add(effect->CreateLabelLayoutItem(), 0, 5)
			.Add(effect->CreateMenuBarLayoutItem(), 1, 5)
			.End()
		.Add(fCurrent)
		.AddGroup(B_HORIZONTAL)
			.Add(fNext)
			.AddGlue()
			.Add(close)
			.End();

	_Load();
	_ShowCurrent();
	CenterOnScreen();
}


SnakeWallpaperWindow::~SnakeWallpaperWindow()
{
	delete fPanel;
	delete fSvgPanel;
}


bool
SnakeWallpaperWindow::QuitRequested()
{
	_Save(false);
	sWindow = NULL;
	return true;
}


void
SnakeWallpaperWindow::_Load()
{
	BMessage settings;
	ReadWallpaperSettings(&settings);
	fEnable->SetValue(settings.GetBool("enabled", false) ? B_CONTROL_ON : B_CONTROL_OFF);
	while (fFolders->CountItems() > 0)
		delete fFolders->RemoveItem((int32)0);
	const char* folder;
	for (int32 i = 0; settings.FindString("folder", i, &folder) == B_OK; i++)
		fFolders->AddItem(new BStringItem(folder));
	for (int i = 0; i < 9; i++) {
		bool on = false;
		const char* source;
		for (int32 j = 0; settings.FindString("online", j, &source) == B_OK; j++)
			on = on || strcmp(source, kOnlineSources[i]) == 0;
		fOnline[i]->SetValue(on ? B_CONTROL_ON : B_CONTROL_OFF);
	}
	for (int i = 0; i < 4; i++)
		fKey[i]->SetText(settings.GetString(kKeyNames[i], ""));
	Mark(fIntervalMenu, settings.GetInt32("interval", 30));
	Mark(fOrderMenu, settings.GetBool("random", true) ? 1 : 0);
	Mark(fWorkspaceMenu, settings.GetBool("perworkspace", false) ? 1 : 0);
	Mark(fModeMenu, settings.GetInt32("mode", -1));
	Mark(fFadeMenu, settings.GetInt32("fadems", 0));
	Mark(fEffectMenu, settings.GetInt32("fadestyle", 0));
	fSvg->SetValue(settings.GetBool("svg", false) ? B_CONTROL_ON : B_CONTROL_OFF);
	_MarkSvg(settings.GetString("svgfile", "builtin:waves"));
	_UpdateControls();
}


// Writes the settings and tells the server; changeNow: and has it change the wallpaper at once (new folders, just switched on).
void
SnakeWallpaperWindow::_Save(bool changeNow, bool notify)
{
	BMessage settings;
	settings.AddBool("enabled", fEnable->Value() == B_CONTROL_ON);
	for (int32 i = 0; i < fFolders->CountItems(); i++)
		settings.AddString("folder", ((BStringItem*)fFolders->ItemAt(i))->Text());
	for (int i = 0; i < 9; i++) {
		if (fOnline[i]->Value() == B_CONTROL_ON)
			settings.AddString("online", kOnlineSources[i]);
	}
	for (int i = 0; i < 4; i++) {
		// (a key is a run of letters and digits: what was pasted around it is dropped)
		BString key(fKey[i]->Text());
		key.Trim();
		if (key.Length() > 0)
			settings.AddString(kKeyNames[i], key.String());
	}
	settings.AddInt32("interval", Marked(fIntervalMenu));
	settings.AddBool("random", Marked(fOrderMenu) == 1);
	settings.AddBool("perworkspace", Marked(fWorkspaceMenu) == 1);
	settings.AddInt32("mode", Marked(fModeMenu));
	settings.AddInt32("fadems", Marked(fFadeMenu));
	settings.AddInt32("fadestyle", Marked(fEffectMenu));
	settings.AddBool("svg", fSvg->Value() == B_CONTROL_ON);
	{
		BMenuItem* marked = fSvgMenu->FindMarked();
		BString file;
		if (marked != NULL && marked->Message() != NULL && marked->Message()->FindString("svgfile", &file) == B_OK)
			settings.AddString("svgfile", file.String());
		else if (fSvgFile.Length() > 0)
			settings.AddString("svgfile", fSvgFile.String());
	}
	WriteWallpaperSettings(settings);
	if (!notify)
		return;

	if (!_ServerReady()) {
		BAlert* alert = new BAlert("", B_TRANSLATE("The SnakeTracker server could not be started, so the wallpaper "
			"will not change by itself. Restarting the computer will start it."), B_TRANSLATE("OK"));
		alert->Go(NULL);
		return;
	}
	BMessenger server(kSnakeServerSignature);
	server.SendMessage(kMsgWallpaperReload);
	if (changeNow && fEnable->Value() == B_CONTROL_ON && (fFolders->CountItems() > 0 || settings.HasString("online")))
		server.SendMessage(kMsgWallpaperNext);
}


// What the window does about the server, when ~/config/settings/snaketracker_wallpaper_debug exists, in snaketracker_wallpaper.log.
static void
Note(const char* text, long value = 0)
{
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK || path.Append("snaketracker_wallpaper_debug") != B_OK
		|| access(path.Path(), F_OK) != 0 || find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK
		|| path.Append("snaketracker_wallpaper.log") != B_OK)
		return;
	FILE* file = fopen(path.Path(), "a");
	if (file != NULL) {
		fprintf(file, "Window: %s %ld\n", text, value);
		fclose(file);
	}
}


// Does the server answer, and is it the server of this build? One that is not (it was running before the package was updated,
// and may not know the settings the window has) is asked to quit and started again.
static bool
ServerIsCurrent()
{
	BMessenger server(kSnakeServerSignature);
	BMessage ping(kMsgWallpaperPing), reply;
	if (!server.IsValid() || server.SendMessage(&ping, &reply, 1000000, 1000000) != B_OK || reply.what != kMsgWallpaperPong)
		return false;
	const char* build = NULL;
	return reply.FindString("build", &build) == B_OK && build != NULL && strcmp(build, SNAKE_BUILD_ID) == 0;
}


bool
SnakeWallpaperWindow::_ServerReady()
{
	if (ServerIsCurrent())
		return true;
	Note("the server is missing or of another build; restarting it");

	BMessenger server(kSnakeServerSignature);
	if (server.IsValid()) {
		server.SendMessage(B_QUIT_REQUESTED);
		for (int i = 0; i < 50 && BMessenger(kSnakeServerSignature).IsValid(); i++)
			snooze(100000);
		Note("the old server is gone:", BMessenger(kSnakeServerSignature).IsValid() ? 0 : 1);
	}
	for (int attempt = 0; attempt < 3; attempt++) {
		// (the launch daemon first, which knows the server as a service; the roster by its signature if that fails)
		status_t status = BLaunchRoster().Start("x-vnd.snaketracker-server");
		Note("launch daemon:", (long)status);
		if (status != B_OK && !BMessenger(kSnakeServerSignature).IsValid()) {
			status = be_roster->Launch(kSnakeServerSignature);
			Note("roster:", (long)status);
		}
		for (int i = 0; i < 40; i++) {
			snooze(100000);
			if (ServerIsCurrent())
				return true;
		}
		Note("the new server did not answer, attempt", attempt + 1);
	}
	return false;
}


void
SnakeWallpaperWindow::_UpdateControls()
{
	fRemove->SetEnabled(fFolders->CurrentSelection() >= 0);
	bool online = false;
	for (int i = 0; i < 9; i++)
		online = online || fOnline[i]->Value() == B_CONTROL_ON;
	fNext->SetEnabled(fFolders->CountItems() > 0 || online);
}


// Marks the template of the settings in the menu ("Other file" shows the name of the file that was chosen).
void
SnakeWallpaperWindow::_MarkSvg(const char* file)
{
	for (int32 i = 0; i < fSvgMenu->CountItems(); i++) {
		BMenuItem* item = fSvgMenu->ItemAt(i);
		BString name;
		if (item->Message() != NULL && item->Message()->FindString("svgfile", &name) == B_OK && name == file) {
			item->SetMarked(true);
			return;
		}
	}
	// a file of the user's: the last item shows its name
	BMenuItem* other = fSvgMenu->ItemAt(fSvgMenu->CountItems() - 1);
	fSvgFile = file;
	const char* slash = strrchr(file, '/');
	BMessage* message = new BMessage(kMsgChanged);
	message->AddString("svgfile", file);
	message->AddInt32("value", 99);
	BMenuItem* chosen = new BMenuItem(slash != NULL ? slash + 1 : file, message);
	fSvgMenu->AddItem(chosen, fSvgMenu->CountItems() - 1);
	chosen->SetMarked(true);
	(void)other;
}


// What the picture on the Desktop is, when it came from the internet (its title and copyright stay with the file).
void
SnakeWallpaperWindow::_ShowCurrent()
{
	BString text;
	BPath desktop;
	if (find_directory(B_DESKTOP_DIRECTORY, &desktop) == B_OK) {
		BNode node(desktop.Path());
		attr_info info;
		if (node.GetAttrInfo("be:bgndimginfo", &info) == B_OK && info.size > 0) {
			char* buffer = new char[info.size];
			BMessage current;
			const char* path;
			if (node.ReadAttr("be:bgndimginfo", info.type, 0, buffer, (size_t)info.size) == info.size
				&& current.Unflatten(buffer) == B_OK && current.FindString("be:bgndimginfopath", &path) == B_OK) {
				// the picture of the workspace this is on: the entry made for it alone, else one that includes it
				const uint32 bit = 1u << current_workspace();
				int32 best = -1;
				for (int32 i = 0; ; i++) {
					int32 mask;
					const char* candidate;
					if (current.FindInt32("be:bgndimginfoworkspaces", i, &mask) != B_OK
						|| current.FindString("be:bgndimginfopath", i, &candidate) != B_OK)
						break;
					if ((uint32)mask == bit) {
						best = i;
						break;
					}
					if (((uint32)mask & bit) != 0 && best < 0)
						best = i;
				}
				if (best >= 0)
					current.FindString("be:bgndimginfopath", best, &path);
				BNode picture(path);
				char title[256] = "", copyright[256] = "";
				if (picture.ReadAttr("SnakeTracker:Title", B_STRING_TYPE, 0, title, sizeof(title) - 1) > 0) {
					picture.ReadAttr("SnakeTracker:Copyright", B_STRING_TYPE, 0, copyright, sizeof(copyright) - 1);
					text << title;
					if (copyright[0] != '\0')
						text << " - " << copyright;
				} else {
					const char* slash = strrchr(path, '/');
					text << (slash != NULL ? slash + 1 : path);
				}
			}
			delete[] buffer;
		}
	}
	fCurrent->SetText(text.String());
	fCurrent->SetToolTip(text.String());
}


void
SnakeWallpaperWindow::WorkspaceActivated(int32 workspace, bool active)
{
	if (active)
		_ShowCurrent();
}


void
SnakeWallpaperWindow::WindowActivated(bool active)
{
	if (active)
		_ShowCurrent();
}


void
SnakeWallpaperWindow::_AddFolder(const char* path)
{
	for (int32 i = 0; i < fFolders->CountItems(); i++) {
		if (strcmp(((BStringItem*)fFolders->ItemAt(i))->Text(), path) == 0)
			return;
	}
	fFolders->AddItem(new BStringItem(path));
}


void
SnakeWallpaperWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgChanged:
		{
			_UpdateControls();
			// (a new picture at once when it is switched on; a change of the timer or the order can wait for the next one)
			_Save(!message->HasInt32("value"));
			break;
		}

		case kMsgSelection:
			_UpdateControls();
			break;

		case kMsgPickSvg:
			if (fSvgPanel == NULL) {
				BMessage picked(kMsgSvgPicked);
				fSvgPanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(this), NULL, B_FILE_NODE, false, &picked);
			}
			fSvgPanel->Show();
			// (the menu showed "Other file" as marked: back to what was chosen until a file is)
			break;

		case kMsgSvgPicked:
		{
			entry_ref ref;
			BPath path;
			if (message->FindRef("refs", &ref) == B_OK && BEntry(&ref, true).GetPath(&path) == B_OK) {
				_MarkSvg(path.Path());
				_Save(true);
			}
			break;
		}

		case kMsgKeyEdited:
			// (saved as it is typed; the server is told when the window closes, or when a source is switched on)
			_Save(false, false);
			break;

		case kMsgAddFolder:
			if (fPanel == NULL) {
				BMessage picked(kMsgFolderPicked);
				fPanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(this), NULL, B_DIRECTORY_NODE, false, &picked);
			}
			fPanel->Show();
			break;

		case kMsgFolderPicked:
		case B_REFS_RECEIVED:
		case B_SIMPLE_DATA:
		{
			entry_ref ref;
			bool added = false;
			for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++) {
				BEntry entry(&ref, true);
				BPath path;
				if (entry.IsDirectory() && entry.GetPath(&path) == B_OK) {
					_AddFolder(path.Path());
					added = true;
				}
			}
			if (added) {
				_UpdateControls();
				_Save(true);
			}
			break;
		}

		case kMsgRemoveFolder:
		{
			int32 index = fFolders->CurrentSelection();
			if (index >= 0) {
				delete fFolders->RemoveItem(index);
				_UpdateControls();
				_Save(false);
			}
			break;
		}

		case kMsgNextNow:
		{
			// (the server answers only when it is the one of this version: after an update the old one is still running
			// until the computer restarts, and it does not know these messages, so it is restarted here)
			if (_ServerReady()) {
				BMessenger server(kSnakeServerSignature);
				server.SendMessage(kMsgWallpaperNext);
			}
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}

}	// namespace BPrivate
