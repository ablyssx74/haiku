/*
 * Rounded, beveled menu selector for Tracker's navigation menus (the same look as hDesktop's).
 * The accent colour comes from SnakeAccent(); the bevel edges and the text colour are derived from it.
 */
#ifndef _SNAKE_SELECTOR_H
#define _SNAKE_SELECTOR_H


#include <GraphicsDefs.h>
#include <Rect.h>
#include <View.h>


namespace SnakeSelector {

const float kRadius = 4.0f;


inline rgb_color
Accent()
{
	// TODO: follow hDesktop's Selector Color when it is running.
	return make_color(70, 110, 200);
}


inline rgb_color
Mix(rgb_color c, float toward, float amount)
{
	// amount 0..1 moves each channel toward the given level (255 = lighter, 0 = darker)
	return make_color((uint8)(c.red + (toward - c.red) * amount),
		(uint8)(c.green + (toward - c.green) * amount),
		(uint8)(c.blue + (toward - c.blue) * amount));
}


inline rgb_color
Light(rgb_color c)
{
	return Mix(c, 255, 0.35f);
}


inline rgb_color
Dark(rgb_color c)
{
	return Mix(c, 0, 0.38f);
}


// White on dark accents, near-black on light ones.
inline rgb_color
TextOn(rgb_color c)
{
	float lum = (0.2126f * c.red + 0.7152f * c.green + 0.0722f * c.blue) / 255.0f;
	return lum > 0.62f ? make_color(20, 22, 28) : make_color(255, 255, 255);
}


// Draws the selector for an item whose frame is `frame`: a light top edge, a dark bottom edge and
// the accent between them. The caller has already painted the menu background.
inline void
Draw(BView* view, BRect frame)
{
	rgb_color accent = Accent();
	BRect r = frame.InsetByCopy(2, 1);

	view->PushState();
	view->SetDrawingMode(B_OP_ALPHA);
	view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);

	view->SetHighColor(Light(accent));
	view->FillRoundRect(r.OffsetByCopy(0, -1), kRadius, kRadius);
	view->SetHighColor(Dark(accent));
	view->FillRoundRect(r.OffsetByCopy(0, 1), kRadius, kRadius);
	view->SetHighColor(accent);
	view->FillRoundRect(r, kRadius, kRadius);

	view->PopState();
}

}	// namespace SnakeSelector


#endif	// _SNAKE_SELECTOR_H
