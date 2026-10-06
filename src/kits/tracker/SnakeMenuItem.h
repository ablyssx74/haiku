/*
 * A menu item that draws itself with the snake-trail selector (see SnakeSelector.h) instead of the
 * stock rectangular highlight. It draws what BMenuItem draws -- content, check mark, shortcut and
 * submenu arrow -- so it can stand in for a plain BMenuItem in Tracker's menus.
 */
#ifndef _SNAKE_MENU_ITEM_H
#define _SNAKE_MENU_ITEM_H


#include <MenuItem.h>


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
			void			_DrawMark(rgb_color color);
			void			_DrawShortcut(rgb_color color, bool menuHasSubmenus);

	typedef BMenuItem _inherited;
};

}	// namespace BPrivate

using BPrivate::SnakeMenuItem;


#endif	// _SNAKE_MENU_ITEM_H
