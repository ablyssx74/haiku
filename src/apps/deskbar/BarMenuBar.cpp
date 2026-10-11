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

Tracker(TM), Be(R), BeOS(R), and BeIA(TM) are trademarks or registered
trademarks of Be Incorporated in the United States and other countries. Other
brand product names are registered trademarks or trademarks of their respective
holders.
All rights reserved.
*/


#include "BarMenuBar.h"
#include "SnakeSelector.h"

#include <algorithm>

#include <Bitmap.h>
#include <ControlLook.h>
#include <Debug.h>
#include <GradientLinear.h>
#include <IconUtils.h>
#include <NodeInfo.h>

#include "icons.h"

#include "BarMenuTitle.h"
#include "BarView.h"
#include "BarWindow.h"
#include "DeskbarMenu.h"
#include "DeskbarUtils.h"
#include "ResourceSet.h"
#include "StatusView.h"
#include "TeamMenu.h"


const float kSepItemWidth = 5.0f;

const float kTeamIconBitmapHeight = 19.f;


//	#pragma mark - TSeparatorItem


TSeparatorItem::TSeparatorItem()
	:
	BSeparatorItem()
{
}


void
TSeparatorItem::Draw()
{
	BMenu* menu = Menu();
	if (menu == NULL)
		return;

	BRect frame(Frame());
	frame.right = frame.left + kSepItemWidth;
	rgb_color base = ui_color(B_MENU_BACKGROUND_COLOR);

	menu->PushState();

	menu->SetHighColor(tint_color(base, 1.22));
	frame.top--;
		// need to expand the frame for some reason

	// stroke a darker line on the left edge
	menu->StrokeLine(frame.LeftTop(), frame.LeftBottom());
	frame.left++;

	// fill in background
	be_control_look->DrawButtonBackground(menu, frame, frame, base);

	menu->PopState();
}


//	#pragma mark - TBarMenuBar


TBarMenuBar::TBarMenuBar(BRect frame, const char* name, TBarView* barView)
	:
	BMenuBar(frame, name, B_FOLLOW_NONE, B_ITEMS_IN_ROW, false),
	fBarView(barView),
	fAppListMenuItem(NULL),
	fSeparatorItem(NULL),
	fTeamIconData(NULL),
	fTeamIconSize(0)
{
	SetItemMargins(0.0f, 0.0f, 0.0f, 0.0f);
	SetFont(be_bold_font);

	TDeskbarMenu* beMenu = new TDeskbarMenu(barView);
	TBarWindow::SetDeskbarMenu(beMenu);

	BBitmap* icon = NULL;
	size_t dataSize;
	const void* data = AppResSet()->FindResource(B_VECTOR_ICON_TYPE,
		R_LeafLogoBitmap, &dataSize);
	if (data != NULL) {
		// seems valid, scale bitmap according to be_bold_font size
		float width = std::max(63.f, ceilf(63 * be_bold_font->Size() / 12.f));
		float height = std::max(22.f, ceilf(22 * be_bold_font->Size() / 12.f));
		icon = new BBitmap(BRect(0, 0, width - 1, height - 1), B_RGBA32);
		if (icon->InitCheck() != B_OK
			|| BIconUtils::GetVectorIcon((const uint8*)data, dataSize, icon)
					!= B_OK) {
			delete icon;
			icon = NULL;
		}
	}

	fDeskbarMenuItem = new TBarMenuTitle(0.0f, 0.0f, icon, beMenu, fBarView);
	AddItem(fDeskbarMenuItem);
}


TBarMenuBar::~TBarMenuBar()
{
}


void
TBarMenuBar::SmartResize(float width, float height)
{
	if (width == -1.0f && height == -1.0f) {
		BRect frame = Frame();
		width = frame.Width();
		height = frame.Height();
	} else
		ResizeTo(width, height);

	if (fSeparatorItem != NULL)
		fDeskbarMenuItem->SetContentSize(width - kSepItemWidth, height);
	else {
		int32 count = CountItems();
		if (fDeskbarMenuItem != NULL)
			fDeskbarMenuItem->SetContentSize(floorf(width / count), height);
		if (fAppListMenuItem != NULL)
			fAppListMenuItem->SetContentSize(floorf(width / count), height);
	}

	InvalidateLayout();
}


bool
TBarMenuBar::AddTeamMenu()
{
	if (CountItems() > 1)
		return false;

	BRect frame(Frame());

	delete fAppListMenuItem;
	fAppListMenuItem = new TBarMenuTitle(0.0f, 0.0f, FetchTeamIcon(),
		new TTeamMenu(fBarView), fBarView);

	bool added = AddItem(fAppListMenuItem, fBarView->Left() ? 0 : 1);

	if (added)
		SmartResize(frame.Width() - 1.0f, frame.Height());
	else
		SmartResize(frame.Width(), frame.Height());

	return added;
}


bool
TBarMenuBar::RemoveTeamMenu()
{
	if (CountItems() < 2)
		return false;

	bool removed = false;

	if (fAppListMenuItem != NULL && RemoveItem(fAppListMenuItem)) {
		delete fAppListMenuItem;
		fAppListMenuItem = NULL;
		SmartResize(-1, -1);
		removed = true;
	}

	return removed;
}


bool
TBarMenuBar::AddSeparatorItem()
{
	if (CountItems() > 1)
		return false;

	BRect frame(Frame());

	delete fSeparatorItem;
	fSeparatorItem = new TSeparatorItem();

	bool added = AddItem(fSeparatorItem);

	if (added)
		SmartResize(frame.Width() - 1.0f, frame.Height());
	else
		SmartResize(frame.Width(), frame.Height());

	return added;
}


bool
TBarMenuBar::RemoveSeperatorItem()
{
	if (CountItems() < 2)
		return false;

	bool removed = false;

	if (fSeparatorItem != NULL && RemoveItem(fSeparatorItem)) {
		delete fSeparatorItem;
		fSeparatorItem = NULL;
		SmartResize(-1, -1);
		removed = true;
	}

	return removed;
}


void
TBarMenuBar::Draw(BRect updateRect)
{
	// skip the fancy BMenuBar drawing code
	BMenu::Draw(updateRect);
}


void
TBarMenuBar::DrawBackground(BRect updateRect)
{
	if (fBarView != NULL && fBarView->PillLook()) {
		// the leaf's block: a lit bar of the trail, in the selector colour, rounded and shaded like it, on the panel
		rgb_color fill;
		fBarView->PillColors(&fill, NULL, NULL, NULL, NULL);
		BRect bounds(Bounds());
		SetHighColor(fill);
		FillRect(bounds);
		// the snake joins the block at the edge nearest the menu: the left one, except for a Deskbar on the left side of
		// the screen (the round corner is the one on the far side; a horizontal Deskbar has none)
		const bool joinOnRight = fBarView->Vertical() && fBarView->Left();
		if (fBarView->Vertical()) {
			SnakeSelector::DrawRaised(this, bounds, 10, joinOnRight ? 4 : 8, 8);
		} else {
			// (a horizontal Deskbar: no round corner, the edge towards the screen's edge is open, the one towards the menu
			// is the block's edge except where the snake of the menu runs into it, at the left)
			SnakeSelector::DrawRaised(this, bounds, 10, 0, fBarView->Top() ? 8 : 4, 0, 12);
		}

		// the block keeps its colour over 30% at that edge and then fades gently towards the panel's colour at the far end
		BGradientLinear fade;
		BRect faded;
		if (joinOnRight) {
			fade.SetStart(BPoint(bounds.right - bounds.Width() * 0.3f, 0));
			fade.SetEnd(BPoint(bounds.left, 0));
			faded = BRect(bounds.left, bounds.top, bounds.right - bounds.Width() * 0.3f, bounds.bottom - 1);
		} else {
			fade.SetStart(BPoint(bounds.left + bounds.Width() * 0.3f, 0));
			fade.SetEnd(BPoint(bounds.right, 0));
			faded = BRect(bounds.left + bounds.Width() * 0.3f, bounds.top, bounds.right, bounds.bottom - 1);
		}
		fill.alpha = 0;
		fade.AddColor(fill, 0);
		fill.alpha = 153;
		fade.AddColor(fill, 255);
		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		FillRect(faded, fade);
		SetDrawingMode(B_OP_COPY);
		return;
	}
	BMenu::DrawBackground(updateRect);
}


void
TBarMenuBar::MouseDown(BPoint where)
{
	// the Deskbar's menu is drawn in the pill look too: it needs a picture of the screen from before it opens
	SnakeSelector::CaptureMenuBackdrop();
	BMenuBar::MouseDown(where);
}


void
TBarMenuBar::MouseMoved(BPoint where, uint32 code, const BMessage* message)
{
	// the following code parallels that in ExpandoMenuBar for DnD tracking

	if (!message) {
		// force a cleanup
		fBarView->DragStop(true);
		BMenuBar::MouseMoved(where, code, message);
		return;
	}

	switch (code) {
		case B_ENTERED_VIEW:
		{
			BPoint loc;
			uint32 buttons;
			GetMouse(&loc, &buttons);
			if (message != NULL && buttons != 0) {
				// attempt to start DnD tracking
				fBarView->CacheDragData(const_cast<BMessage*>(message));
				MouseDown(loc);
			}
			break;
		}
	}

	BMenuBar::MouseMoved(where, code, message);
}


static void
init_tracking_hook(BMenuItem* item, bool (*hookFunction)(BMenu*, void*),
	void* state)
{
	if (!item)
		return;

	BMenu* windowMenu = item->Submenu();
	if (windowMenu) {
		// have a menu, set the tracking hook
		windowMenu->SetTrackingHook(hookFunction, state);
	}
}


void
TBarMenuBar::InitTrackingHook(bool (*hookFunction)(BMenu*, void*),
	void* state, bool both)
{
	BPoint loc;
	uint32 buttons;
	GetMouse(&loc, &buttons);
	// set the hook functions for the two menus
	// will always have the deskbar menu
	// may have the app menu as well (mini mode)
	if (fDeskbarMenuItem->Frame().Contains(loc) || both)
		init_tracking_hook(fDeskbarMenuItem, hookFunction, state);

	if (fAppListMenuItem && (fAppListMenuItem->Frame().Contains(loc) || both))
		init_tracking_hook(fAppListMenuItem, hookFunction, state);
}


const BBitmap*
TBarMenuBar::FetchTeamIcon()
{
	const BBitmap* teamIcon = NULL;

	if (fTeamIconData == NULL || fTeamIconSize == 0) {
		// we haven't fetched vector icon data yet, fetch it
		fTeamIconData = (const uint8*)AppResSet()->FindResource(
			B_VECTOR_ICON_TYPE, R_TeamIcon, &fTeamIconSize);
	}

	if (fTeamIconData != NULL && fTeamIconSize > 0) {
		// seems valid, scale bitmap according to be_bold_font size
		float iconHeight = std::max(kTeamIconBitmapHeight,
			ceilf(kTeamIconBitmapHeight * be_bold_font->Size() / 12.f));
		BRect iconRect = BRect(0, 0, iconHeight, iconHeight);
		iconRect.InsetBy(-1, -1);
			// grow icon by 1px so that it renders nicely at 12pt font size
		BBitmap* icon = new(std::nothrow) BBitmap(iconRect, B_RGBA32);
		if (icon != NULL && icon->InitCheck() == B_OK
			&& BIconUtils::GetVectorIcon(fTeamIconData, fTeamIconSize, icon)
				== B_OK) {
			// rasterize vector icon into a bitmap at the scaled size
			teamIcon = icon;
		} else if (icon != NULL)
			delete icon;
	}

	return teamIcon;
}
