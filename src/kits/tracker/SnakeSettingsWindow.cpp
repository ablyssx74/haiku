/*
 * The SnakeTracker Settings window: see SnakeSettingsWindow.h.
 */


#include "SnakeSettingsWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <Locale.h>
#include <TabView.h>

#include "SettingsViews.h"
#include "SnakeHeaderIconView.h"
#include "SnakeWallpaperView.h"
#include "SnakeWorkspacesView.h"
#include "TrackerSettings.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SnakeSettingsWindow"


namespace BPrivate {

static const uint32 kMsgDefaults = 'ssDf';
static const uint32 kMsgRevert = 'ssRv';

static SnakeSettingsWindow* sWindow = NULL;


void
SnakeSettingsWindow::Open()
{
	if (sWindow != NULL) {
		if (sWindow->Lock()) {
			// (the window follows to the workspace it is asked for on)
			sWindow->SetWorkspaces(B_CURRENT_WORKSPACE);
			if (sWindow->IsHidden())
				sWindow->Show();
			sWindow->Activate();
			sWindow->Unlock();
		}
		return;
	}
	sWindow = new SnakeSettingsWindow();
	sWindow->Show();
}


SnakeSettingsWindow::SnakeSettingsWindow()
	:
	BWindow(BRect(120, 120, 600, 560), B_TRANSLATE("SnakeTracker Settings"), B_TITLED_WINDOW,
		B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS),
	fTabs(NULL),
	fCore(NULL),
	fWallpaper(NULL)
{
	// the core tab: the settings that were the SnakeTracker pane of Tracker's preferences (it is no longer there)
	fCore = new SnakeSettingsView();
	BButton* defaults = new BButton("defaults", B_TRANSLATE("Defaults"), new BMessage(kMsgDefaults));
	BButton* revert = new BButton("revert", B_TRANSLATE("Revert"), new BMessage(kMsgRevert));
	BGroupView* core = new BGroupView("core", B_VERTICAL);
	BLayoutBuilder::Group<>(core, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.Add(fCore)
		.AddGroup(B_HORIZONTAL)
			.Add(defaults)
			.Add(revert)
			.AddGlue()
			.End()
		.SetInsets(B_USE_WINDOW_INSETS);

	fWallpaper = new SnakeWallpaperView();

	fTabs = new BTabView("tabs", B_WIDTH_FROM_LABEL);
	fTabs->AddTab(core, new BTab());
	fTabs->TabAt(0)->SetLabel(B_TRANSLATE("Core settings"));
	fTabs->AddTab(fWallpaper, new BTab());
	fTabs->TabAt(1)->SetLabel(B_TRANSLATE("Wallpaper"));
	fTabs->AddTab(new SnakeWorkspacesView(), new BTab());
	fTabs->TabAt(2)->SetLabel(B_TRANSLATE("Workspaces"));
	fTabs->AddTab(new SnakeHeaderIconView(), new BTab());
	fTabs->TabAt(3)->SetLabel(B_TRANSLATE("Custom HVIF Icons"));

	BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(fTabs);

	fCore->RecordRevertSettings();
	fCore->ShowCurrentSettings();
	CenterOnScreen();
}


bool
SnakeSettingsWindow::QuitRequested()
{
	fWallpaper->Save();
	TrackerSettings().SaveSettings(false);
	sWindow = NULL;
	return true;
}


void
SnakeSettingsWindow::WindowActivated(bool active)
{
	if (active && fWallpaper != NULL)
		fWallpaper->Refresh();
}


void
SnakeSettingsWindow::WorkspaceActivated(int32 workspace, bool active)
{
	if (active && fWallpaper != NULL)
		fWallpaper->Refresh();
}


void
SnakeSettingsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kSettingsContentsModified:
			// (the core settings apply as they are changed; kept at once)
			TrackerSettings().SaveSettings(false);
			break;

		case kMsgDefaults:
			fCore->SetDefaults();
			fCore->ShowCurrentSettings();
			TrackerSettings().SaveSettings(false);
			break;

		case kMsgRevert:
			fCore->Revert();
			fCore->ShowCurrentSettings();
			TrackerSettings().SaveSettings(false);
			break;

		default:
		{
			// What the controls of a tab send has no target of its own, so it comes here, and goes on to the view of the tab
			// that is shown. (A message that view does not know goes up its chain of handlers, and ends up here again: it is
			// passed on only once, and only messages that are the tabs' own are.)
			static bool passing = false;
			const uint32 family = message->what >> 24;
			const bool tabs = family == 'w' || family == 'h' || message->what == B_REFS_RECEIVED
				|| message->what == B_SIMPLE_DATA;
			BView* tab = fTabs != NULL && fTabs->TabAt(fTabs->Selection()) != NULL
				? fTabs->TabAt(fTabs->Selection())->View() : NULL;
			if (tab != NULL && tabs && !passing) {
				passing = true;
				tab->MessageReceived(message);
				passing = false;
			} else
				BWindow::MessageReceived(message);
			break;
		}
	}
}

}	// namespace BPrivate
