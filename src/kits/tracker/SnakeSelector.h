/*
 * Rounded, beveled menu selector with a "snake trail" for Tracker's navigation menus -- the same look
 * as hDesktop's. The selector of the open menu path runs unbroken from the root menu through every
 * open submenu: a row extends to the edge facing its submenu, the submenu carries an "elbow bar"
 * down that edge to its own row, and concave fillets join the two.
 *
 * Every menu level is its own window, so each level draws its own piece (DrawTrail()) and the pieces
 * meet at the shared edge. The accent colour comes from Accent(); the bevel edges and the text colour
 * are derived from it.
 */
#ifndef _SNAKE_SELECTOR_H
#define _SNAKE_SELECTOR_H


#include <GraphicsDefs.h>
#include <Rect.h>

class BMenu;
class BView;


namespace SnakeSelector {

rgb_color	Accent();
bool		TrailEnabled();
bool		FlatFill();
rgb_color	Light(rgb_color);
rgb_color	Dark(rgb_color);
rgb_color	Outline(rgb_color);
rgb_color	TextOn(rgb_color accent);

// Selector for one item in a menu that doesn't draw the trail itself.
void		DrawLoneSelector(BView* view, BRect frame);

// A submenu records the row of its parent menu it was opened from, when it is attached (the parent's
// window is locked by the same thread then), and forgets it when it is detached. Both invalidate the
// parent so its row is redrawn with or without the edge that joins the submenu.
void		AttachLink(BMenu* submenu);
void		DetachLink(BMenu* submenu);

// Draws the trail pieces of a BNavMenu: its selected row, the elbow bar and fillets from the parent.
// Call after the menu's own background.
void		DrawTrail(BMenu* menu);

bool		MenuDrawsTrail(BMenu* menu);

// The check mark of check boxes (SnakeControlLook's), for a menu: a curved stroke with a soft shadow, drawn in
// \a box (it reaches a little above and to the right of it).
void		DrawTick(BView* view, const BRect& box, rgb_color color, bool shadow);

// The selection of a Tracker label (file name): a pill in a light tint of the accent, drawn under the text.
// \a active is false for a window that is not the active one (a more muted pill). Returns the colour for the text.
// The pill's colour and the colour of the text on it, over a background of colour \a low.
void		SelectionColors(const rgb_color& low, bool active, rgb_color* pill, rgb_color* text);

// On the Desktop there is no background to erase (it is the wallpaper): \a erase is false, and \a backdrop says
// whether the wallpaper is dark or light.
rgb_color	DrawSelectionPill(BView* view, BRect frame, bool active, bool erase = true,
				const rgb_color* backdrop = NULL);
bool		MenuPaintsSelector(BMenu* menu);

}	// namespace SnakeSelector


#endif	// _SNAKE_SELECTOR_H
