/*
 * The Deskbar's header icon: the snake in the header's block. The user can replace it with a vector icon (an HVIF file), make the
 * header taller and have the icon sit at the left, in the middle or at the right: all of it is set in the Custom HVIF Icons tab of
 * the SnakeTracker Settings window (in Tracker; see SnakeHeader.h), and this reads it.
 */
#ifndef HEADER_ICON_H
#define HEADER_ICON_H


#include <SupportDefs.h>


class BBitmap;


// the icon at \a size pixels, from the user's file if there is one, else the built-in snake (the caller owns it)
BBitmap*	HeaderIconBitmap(int32 size);
// changes whenever the icon does (0 for the built-in one)
int64		HeaderIconStamp();
// how many pixels taller than usual the header is, and where the icon sits (0 left, 1 center, 2 right)
int32		HeaderExtraHeight();
int32		HeaderAlignment();			// across the header
int32		HeaderVerticalAlignment();	// up and down in it (0 top, 1 center, 2 bottom)
int32		HeaderIconPercent();		// how much of the header the icon fills (100: all of it)
// Tracker's icon, when the user chose one: drawn into \a icon (a bitmap of the size it is wanted at); false when there is none
bool		CustomTrackerIcon(BBitmap* icon);


#endif	// HEADER_ICON_H
