/*
 * A menu item that draws itself with the snake-trail selector (see SnakeSelector.h) instead of the
 * stock rectangular highlight. It draws what BMenuItem draws -- content, check mark, shortcut and
 * submenu arrow -- so it can stand in for a plain BMenuItem in Tracker's menus.
 */
#ifndef _SNAKE_MENU_ITEM_H
#define _SNAKE_MENU_ITEM_H


#include <Menu.h>
#include <MenuItem.h>
#include <SeparatorItem.h>
#include <PopUpMenu.h>


namespace BPrivate {

class SnakeMenuItem : public BMenuItem {
public:
							SnakeMenuItem(const char* label, BMessage* message, char shortcut = 0,
								uint32 modifiers = 0);
							SnakeMenuItem(BMenu* menu, BMessage* message = NULL);
							SnakeMenuItem(BMessage* data);

	// BMenuItem::IsSelected() is protected; the trail code needs it from outside.
			bool			IsItemSelected() const { return IsSelected(); }

protected:
	virtual	void			Draw();
	virtual	void			Highlight(bool highlight);

private:
			void			_DrawMark(rgb_color color, bool active);
			void			_DrawShortcut(rgb_color color, bool menuHasSubmenus);

	typedef BMenuItem _inherited;
};

// The line between groups of items: a thin pill with a hint of the accent, fading out at its ends.
class SnakeSeparatorItem : public BSeparatorItem {
public:
							SnakeSeparatorItem();

protected:
	virtual	void			Draw();
};

// Plain menus that draw the snake trail (see SnakeSelector.h). Only for menus whose items are all
// SnakeMenuItems (or subclasses): the trail menu leaves the selector to them.
class SnakeMenu : public BMenu {
public:
							SnakeMenu(const char* name, menu_layout layout = B_ITEMS_IN_COLUMN);

	virtual	void			AttachedToWindow();
	virtual	void			DetachedFromWindow();
	virtual	void			DrawBackground(BRect updateRect);
};


class SnakePopUpMenu : public BPopUpMenu {
public:
							SnakePopUpMenu(const char* name, bool radioMode = true,
								bool labelFromMarked = true, menu_layout layout = B_ITEMS_IN_COLUMN);

	virtual	void			AttachedToWindow();
	virtual	void			DetachedFromWindow();
	virtual	void			DrawBackground(BRect updateRect);
};

}	// namespace BPrivate

using BPrivate::SnakeMenu;
using BPrivate::SnakeMenuItem;
using BPrivate::SnakePopUpMenu;


#endif	// _SNAKE_MENU_ITEM_H
