/*
 * The "SnakeTracker Workspaces" window: see SnakeWorkspacesWindow.h.
 */


#include "SnakeWorkspacesWindow.h"

#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <Locale.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <StringView.h>

#include "SnakeWorkspaces.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SnakeWorkspacesWindow"


namespace BPrivate {

static const uint32 kMsgChanged = 'wsCh';

static SnakeWorkspacesWindow* sWindow = NULL;


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
		BMenuItem* item = menu->ItemAt(i);
		int32 itemValue;
		if (item->Message() != NULL && item->Message()->FindInt32("value", &itemValue) == B_OK && itemValue == value) {
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
SnakeWorkspacesWindow::Open()
{
	if (sWindow != NULL && sWindow->Lock()) {
		if (sWindow->IsHidden())
			sWindow->Show();
		sWindow->Activate();
		sWindow->Unlock();
		return;
	}
	sWindow = new SnakeWorkspacesWindow();
	sWindow->Show();
}


SnakeWorkspacesWindow::SnakeWorkspacesWindow()
	:
	BWindow(BRect(200, 200, 520, 340), B_TRANSLATE("SnakeTracker Workspaces"), B_TITLED_WINDOW_LOOK,
		B_NORMAL_WINDOW_FEEL, B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS)
{
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

	int32 ms, effect;
	ReadWorkspaceSettings(&ms, &effect);
	Mark(fFadeMenu, ms);
	Mark(fEffectMenu, effect);

	BMenuField* fade = new BMenuField(B_TRANSLATE("Transition:"), fFadeMenu);
	BMenuField* style = new BMenuField(B_TRANSLATE("Effect:"), fEffectMenu);
	BButton* close = new BButton("close", B_TRANSLATE("Close"), new BMessage(B_QUIT_REQUESTED));
	close->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(new BStringView("note 1", B_TRANSLATE("When you go to another workspace (with the Workspaces")))
		.Add(new BStringView("note 2", B_TRANSLATE("preferences, the Deskbar or hDesktop), the wallpaper changes")))
		.Add(new BStringView("note 3", B_TRANSLATE("with this effect, if the workspaces have different wallpapers.")))
		.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
			.Add(fade->CreateLabelLayoutItem(), 0, 0).Add(fade->CreateMenuBarLayoutItem(), 1, 0)
			.Add(style->CreateLabelLayoutItem(), 0, 1).Add(style->CreateMenuBarLayoutItem(), 1, 1)
			.End()
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(close)
			.End();

	CenterOnScreen();
}


bool
SnakeWorkspacesWindow::QuitRequested()
{
	sWindow = NULL;
	return true;
}


void
SnakeWorkspacesWindow::_Save()
{
	WriteWorkspaceSettings(Marked(fFadeMenu), Marked(fEffectMenu));
}


void
SnakeWorkspacesWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgChanged:
			_Save();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}

}	// namespace BPrivate
