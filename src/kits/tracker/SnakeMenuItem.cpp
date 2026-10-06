#include "SnakeMenuItem.h"

#include <algorithm>
#include <math.h>

#include <Bitmap.h>
#include <ControlLook.h>
#include <InterfaceDefs.h>
#include <Menu.h>
#include <MenuPrivate.h>
#include <Shape.h>
#include <Window.h>

#include "SnakeSelector.h"


namespace BPrivate {

static const float kMarkTint = 0.75f;
static const char* kDeleteShortcutUTF8 = "\xe2\x8c\xa6";	// U+2326


SnakeMenuItem::SnakeMenuItem(const char* label, BMessage* message, char shortcut,
	uint32 modifiers)
	:
	BMenuItem(label, message, shortcut, modifiers)
{
}


SnakeMenuItem::SnakeMenuItem(BMenu* menu, BMessage* message)
	:
	BMenuItem(menu, message)
{
}


SnakeMenuItem::SnakeMenuItem(BMessage* data)
	:
	BMenuItem(data)
{
}


void
SnakeMenuItem::Draw()
{
	BMenu* menu = Menu();
	if (menu == NULL)
		return;

	BRect frame = Frame();
	const bool active = IsSelected() && (IsEnabled() || Submenu() != NULL);

	// A menu that draws the trail itself (SnakeSelector::DrawTrail(), from its DrawBackground()) has
	// already painted the selector; any other menu gets a lone rounded selector from the item.
	const bool trailMenu = SnakeSelector::MenuDrawsTrail(menu);

	menu->PushState();

	const rgb_color background = ui_color(B_MENU_BACKGROUND_COLOR);
	if (!trailMenu) {
		menu->SetHighColor(background);
		menu->FillRect(frame);
		if (active)
			SnakeSelector::DrawLoneSelector(menu, frame);
	}

	rgb_color text;
	if (active) {
		text = SnakeSelector::TextOn(SnakeSelector::Accent());
		menu->SetLowColor(SnakeSelector::Accent());
	} else {
		menu->SetLowColor(background);
		if (IsEnabled())
			text = ui_color(B_MENU_ITEM_TEXT_COLOR);
		else
			text = tint_color(background, B_DISABLED_LABEL_TINT);
	}
	menu->SetHighColor(text);

	menu->MovePenTo(ContentLocation());
	DrawContent();

	MenuPrivate privateAccessor(menu);
	const menu_layout layout = privateAccessor.Layout();
	if (layout != B_ITEMS_IN_ROW && IsMarked())
		_DrawMark(text);

	if (layout == B_ITEMS_IN_COLUMN) {
		uint32 modifiers;
		if (Shortcut(&modifiers) != 0)
			_DrawShortcut(text, privateAccessor.HasSubmenus());

		if (Submenu() != NULL) {
			float symbolSize = roundf(frame.Height() * 2 / 3);
			BRect symbolRect(0, 0, symbolSize, symbolSize);
			symbolRect.OffsetTo(BPoint(frame.right - symbolSize,
				frame.top + (frame.Height() - symbolSize) / 2));
			be_control_look->DrawArrowShape(menu, symbolRect, symbolRect, text,
				BControlLook::B_RIGHT_ARROW, 0, kMarkTint);
		}
	}

	menu->PopState();
}


void
SnakeMenuItem::Highlight(bool highlight)
{
	BMenu* menu = Menu();
	if (menu != NULL && SnakeSelector::MenuDrawsTrail(menu)) {
		// The trail's shape depends on the selection, so the whole menu is redrawn, not just this
		// row. Only from the window's own locked thread; otherwise the next draw catches up.
		if (menu->Window() != NULL && menu->Window()->IsLocked())
			menu->Invalidate();
	} else
		_inherited::Highlight(highlight);
}


void
SnakeMenuItem::_DrawMark(rgb_color color)
{
	BMenu* menu = Menu();
	menu->PushState();

	BRect r(Frame());
	float leftMargin;
	MenuPrivate(menu).GetItemMargins(&leftMargin, NULL, NULL, NULL);
	float gap = leftMargin / 4;
	r.right = r.left + leftMargin - gap;
	r.left += gap / 3;

	BPoint center(floorf((r.left + r.right) / 2.0), floorf((r.top + r.bottom) / 2.0));

	float size = std::min(r.Height() - 2, r.Width());

	BShape arrowShape;
	center.x += 0.5;
	center.y += 0.5;
	size *= 0.3;
	arrowShape.MoveTo(BPoint(center.x - size, center.y - size * 0.25));
	arrowShape.LineTo(BPoint(center.x - size * 0.25, center.y + size));
	arrowShape.LineTo(BPoint(center.x + size, center.y - size));

	menu->SetHighColor(tint_color(color, kMarkTint));
	menu->SetDrawingMode(B_OP_OVER);
	menu->SetPenSize(2.0);
	menu->MovePenTo(B_ORIGIN);
	menu->StrokeShape(&arrowShape);

	menu->PopState();
}


void
SnakeMenuItem::_DrawShortcut(rgb_color color, bool menuHasSubmenus)
{
	BMenu* menu = Menu();
	uint32 modifiers = 0;
	char shortcut = Shortcut(&modifiers);
	BRect bounds = Frame();

	BFont font;
	menu->GetFont(&font);
	BPoint where = ContentLocation();
	// start from the right and walk our way back
	where.x = bounds.right - font.Size();
	if (menuHasSubmenus)
		where.x -= bounds.Height() / 2;

	const float ascent = MenuPrivate(menu).Ascent();
	if (shortcut == B_DELETE)
		menu->DrawString(kDeleteShortcutUTF8, where + BPoint(0, ascent));
	else
		menu->DrawChar(shortcut, where + BPoint(0, ascent));

	where.y += (bounds.Height() - 11) / 2 - 1;
	where.x -= 4;

	const BBitmap* keys[4] = {NULL, NULL, NULL, NULL};
	if ((modifiers & B_COMMAND_KEY) != 0)
		keys[0] = MenuPrivate::MenuItemCommand();
	if ((modifiers & B_CONTROL_KEY) != 0)
		keys[1] = MenuPrivate::MenuItemControl();
	if ((modifiers & B_OPTION_KEY) != 0)
		keys[2] = MenuPrivate::MenuItemOption();
	if ((modifiers & B_SHIFT_KEY) != 0)
		keys[3] = MenuPrivate::MenuItemShift();
	for (int i = 0; i < 4; i++) {
		if (keys[i] == NULL)
			continue;
		where.x -= keys[i]->Bounds().Width() + 1;
		menu->DrawBitmap(keys[i], where);
	}
}

SnakeMenu::SnakeMenu(const char* name, menu_layout layout)
	:
	BMenu(name, layout)
{
}


void
SnakeMenu::AttachedToWindow()
{
	BMenu::AttachedToWindow();
	SnakeSelector::AttachLink(this);
}


void
SnakeMenu::DetachedFromWindow()
{
	SnakeSelector::DetachLink(this);
	BMenu::DetachedFromWindow();
}


void
SnakeMenu::DrawBackground(BRect updateRect)
{
	BMenu::DrawBackground(updateRect);
	SnakeSelector::DrawTrail(this);
}


SnakePopUpMenu::SnakePopUpMenu(const char* name, bool radioMode, bool labelFromMarked,
	menu_layout layout)
	:
	BPopUpMenu(name, radioMode, labelFromMarked, layout)
{
}


void
SnakePopUpMenu::AttachedToWindow()
{
	BPopUpMenu::AttachedToWindow();
	SnakeSelector::AttachLink(this);
}


void
SnakePopUpMenu::DetachedFromWindow()
{
	SnakeSelector::DetachLink(this);
	BPopUpMenu::DetachedFromWindow();
}


void
SnakePopUpMenu::DrawBackground(BRect updateRect)
{
	BPopUpMenu::DrawBackground(updateRect);
	SnakeSelector::DrawTrail(this);
}

}	// namespace BPrivate
