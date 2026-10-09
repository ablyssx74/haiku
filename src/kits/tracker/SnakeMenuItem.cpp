#include "SnakeMenuItem.h"

#include <algorithm>
#include <math.h>

#include <Bitmap.h>
#include <ControlLook.h>
#include <InterfaceDefs.h>
#include <GradientLinear.h>
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
	const bool trailMenu = SnakeSelector::MenuPaintsSelector(menu);

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
		_DrawMark(text, active);

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


SnakeSeparatorItem::SnakeSeparatorItem()
	:
	BSeparatorItem()
{
}


void
SnakeSeparatorItem::Draw()
{
	BMenu* menu = Menu();
	if (menu == NULL)
		return;
	MenuPrivate privateAccessor(menu);
	if (privateAccessor.Layout() == B_ITEMS_IN_ROW) {
		BSeparatorItem::Draw();
		return;
	}

	const BRect frame = Frame();
	// (the menu's own low colour is not the colour it is drawn on in every menu: the Desktop's)
	const rgb_color low = ui_color(B_MENU_BACKGROUND_COLOR);
	const bool dark = (low.red * 299 + low.green * 587 + low.blue * 114) / 1000 < 128;
	const rgb_color accent = SnakeSelector::Accent();
	// a mid tone for the line, with the accent in it
	const rgb_color grey = tint_color(low, dark ? 1.9f : 0.6f);
	rgb_color line = make_color((uint8)(grey.red + (accent.red - grey.red) * 0.4f),
		(uint8)(grey.green + (accent.green - grey.green) * 0.4f),
		(uint8)(grey.blue + (accent.blue - grey.blue) * 0.4f));

	const float y = frame.top + floorf(frame.Height() / 2);
	BRect pill(frame.left + 10, y, frame.right - 10, y + 1.5f);
	if (pill.Width() < 12)
		return;

	// fading out at both ends
	BGradientLinear gradient(pill.LeftTop(), pill.RightTop());
	rgb_color clear = line;
	clear.alpha = 0;
	line.alpha = 200;
	gradient.AddColor(clear, 0);
	gradient.AddColor(line, 70);
	gradient.AddColor(line, 185);
	gradient.AddColor(clear, 255);

	menu->PushState();
	menu->SetDrawingMode(B_OP_ALPHA);
	menu->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	menu->FillRoundRect(pill, 0.75f, 0.75f, gradient);
	menu->PopState();
}


void
SnakeMenuItem::Highlight(bool highlight)
{
	BMenu* menu = Menu();
	if (menu != NULL && SnakeSelector::MenuPaintsSelector(menu)) {
		// The trail's shape depends on the selection, so the whole menu is redrawn, not just this
		// row. Only from the window's own locked thread; otherwise the next draw catches up.
		if (menu->Window() != NULL && menu->Window()->IsLocked())
			menu->Invalidate();
	} else
		_inherited::Highlight(highlight);
}


void
SnakeMenuItem::_DrawMark(rgb_color color, bool active)
{
	BMenu* menu = Menu();
	menu->PushState();

	BRect r(Frame());
	float leftMargin;
	MenuPrivate(menu).GetItemMargins(&leftMargin, NULL, NULL, NULL);
	float gap = leftMargin / 4;
	r.right = r.left + leftMargin - gap;
	r.left += gap / 3;

	const BPoint center(floorf((r.left + r.right) / 2.0), floorf((r.top + r.bottom) / 2.0));
	const float size = floorf(std::min(r.Height() - 4, r.Width()) * 0.78f);

	// the check boxes' mark, smaller: the accent on the menu, the text's colour on the selected row (which is the
	// accent itself). The tick's middle is a little right of and below its box's.
	const rgb_color menuColor = ui_color(B_MENU_BACKGROUND_COLOR);
	const bool dark = (menuColor.red * 299 + menuColor.green * 587 + menuColor.blue * 114) / 1000 < 128;
	rgb_color mark = color;
	if (!active) {
		const rgb_color accent = SnakeSelector::Accent();
		mark = dark ? make_color((uint8)(accent.red + (255 - accent.red) * 0.2f),
				(uint8)(accent.green + (255 - accent.green) * 0.2f),
				(uint8)(accent.blue + (255 - accent.blue) * 0.2f))
			: SnakeSelector::Dark(accent);
		if (!IsEnabled()) {
			mark = make_color((uint8)(menuColor.red + (mark.red - menuColor.red) * 0.4f),
				(uint8)(menuColor.green + (mark.green - menuColor.green) * 0.4f),
				(uint8)(menuColor.blue + (mark.blue - menuColor.blue) * 0.4f));
		}
	}
	const BRect box(center.x - 0.69f * size, center.y - 0.32f * size, center.x + 0.31f * size,
		center.y + 0.68f * size);
	SnakeSelector::DrawTick(menu, box, mark, !active);

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
