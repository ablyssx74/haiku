/*
 * Copyright 2001-2014 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Stephan Aßmus, superstippi@gmx.de
 *		DarkWyrm, bpmagic@columbus.rr.com
 *		John Scipione, jscipione@gmail.com
 *		Clemens Zeidler, haiku@clemens-zeidler.de
 */


/*!	Decorator inspired by the "b6" xfwm4 theme: a yellow tab with black
	text, lavender/purple buttons and a ridged bottom-right grab bar.
	Structurally based on BeDecorator so it keeps stack & tile support
	and the resizable-corner handle that MacDecorator lacks.
*/


#include "B6Decorator.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <stdio.h>

#include <WindowPrivate.h>

#include <Autolock.h>
#include <Debug.h>
#include <GradientLinear.h>
#include <Rect.h>
#include <Region.h>
#include <View.h>

#include "BitmapDrawingEngine.h"
#include "Desktop.h"
#include "DesktopSettings.h"
#include "DrawingEngine.h"
#include "DrawState.h"
#include "FontManager.h"
#include "PatternHandler.h"
#include "RGBColor.h"
#include "ServerBitmap.h"


//#define DEBUG_DECORATOR
#ifdef DEBUG_DECORATOR
#	define STRACE(x) printf x
#else
#	define STRACE(x) ;
#endif


// b6 theme palette, sampled from the xfwm4 "b6" theme bitmaps
// (ablyss/xfwm4-themes-4.10.0/themes/b6). The frame and text colors are
// the same for both focus states in that theme; only the tab and the
// grab bar dim when a window loses focus.
static const rgb_color kFrameColor        = { 238, 238, 230, 255 }; // #EEEEE6

static const rgb_color kActiveTabLight    = { 255, 255,  80, 255 }; // #FFFF50
static const rgb_color kActiveTabColor    = { 255, 221,  25, 255 }; // #FFDD19
static const rgb_color kActiveTabShadow   = { 176, 124,   0, 255 }; // #B07C00

static const rgb_color kInactiveTabLight  = { 255, 255, 171, 255 }; // #FFFFAB
static const rgb_color kInactiveTabColor  = { 255, 241, 139, 255 }; // #FFF18B
static const rgb_color kInactiveTabShadow = { 221, 195, 127, 255 }; // #DDC37F

static const rgb_color kTextColor         = {   0,   0,   0, 255 }; // #000000

static const rgb_color kActiveButtonColor   = { 140, 145, 209, 255 }; // #8C91D1
static const rgb_color kActiveButtonLight   = { 184, 188, 243, 255 }; // #B8BCF3
static const rgb_color kActiveButtonShadow  = {  98, 103, 167, 255 }; // #6267A7

static const rgb_color kInactiveButtonColor = { 200, 203, 234, 255 }; // #C8CBEA
static const rgb_color kInactiveButtonLight = { 225, 227, 251, 255 }; // #E1E3FB
static const rgb_color kInactiveButtonShadow = { 170, 172, 197, 255 }; // #AAACC5

// Geometry shared between the tab's flag caps (see _OverhangRect()) and
// the close/zoom button placement (see _GetButtonSizeAndOffset()), so a
// button and the cap it sits in always stay concentric regardless of
// the tab's own (font-dependent) height.
static const float kFlagCapRadiusRatio = 0.5f;
static const float kFlagOverhangRatio = 0.8f;
static const float kFlagButtonDiameterRatio = 0.75f;


static const unsigned char f = 0xff; // way to write 0xff shorter

static const unsigned char kInnerShadowBits[] = {
	f, f, f, f, f, f, f, f, f, 0,
	f, f, f, f, f, f, 0, f, 0, f,
	f, f, f, f, f, 0, f, 0, f, 0,
	f, f, f, f, 0, f, 0, 0, 0, 0,
	f, f, f, 0, f, 0, 0, 0, 0, 0,
	f, f, 0, f, 0, 0, 0, 0, 0, 0,
	f, 0, f, 0, 0, 0, 0, 0, 0, 0,
	f, f, 0, 0, 0, 0, 0, 0, 0, 0,
	f, 0, f, 0, 0, 0, 0, 0, 0, 0,
	0, f, 0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char kOuterShadowBits[] = {
	f, f, f, f, f, f, f, f, f, f,
	f, f, f, f, f, f, f, f, f, f,
	f, f, f, f, f, f, f, f, f, f,
	f, f, f, f, f, f, f, f, f, f,
	f, f, f, f, f, f, f, f, f, 0,
	f, f, f, f, f, f, f, 0, 0, 0,
	f, f, f, f, f, f, 0, f, 0, 0,
	f, f, f, f, f, 0, f, 0, 0, 0,
	f, f, f, f, f, 0, 0, 0, 0, 0,
	f, f, f, f, 0, 0, 0, 0, 0, 0
};

static const unsigned char kBigInnerShadowBits[] = {
	f, f, f, f, f, f, f,
	f, f, f, f, f, f, 0,
	f, f, f, f, f, 0, 0,
	f, f, f, f, 0, f, 0,
	f, f, f, 0, f, 0, 0,
	f, f, 0, f, 0, 0, 0,
	f, 0, 0, 0, 0, 0, 0
};

static const unsigned char kBigOuterShadowBits[] = {
	f, f, f, f, f, f, f,
	f, f, f, f, f, f, 0,
	f, f, f, f, f, f, 0,
	f, f, f, f, f, f, 0,
	f, f, f, f, f, f, 0,
	f, f, f, f, f, f, 0,
	f, 0, 0, 0, 0, 0, 0
};

static const unsigned char kSmallInnerShadowBits[] = {
	f, f, f, 0, 0,
	f, f, 0, f, 0,
	f, 0, f, 0, 0,
	0, f, 0, 0, 0,
	0, 0, 0, 0, 0
};

static const unsigned char kSmallOuterShadowBits[] = {
	f, f, f, f, f,
	f, f, f, f, f,
	f, f, f, f, f,
	f, f, f, f, 0,
	f, f, 0, 0, 0
};

static const unsigned char kGlintBits[] = {
	0, f, 0,
	f, 0, f,
	0, f, f
};


//     #pragma mark - B6DecorAddOn


B6DecorAddOn::B6DecorAddOn(image_id id, const char* name)
	:
	DecorAddOn(id, name)
{
}


Decorator*
B6DecorAddOn::_AllocateDecorator(DesktopSettings& settings, BRect rect,
	Desktop* desktop)
{
	return new (std::nothrow)B6Decorator(settings, rect, desktop);
}


//	#pragma mark - B6Decorator


// TODO: get rid of DesktopSettings here, and introduce private accessor
//	methods to the Decorator base class
B6Decorator::B6Decorator(DesktopSettings& settings, BRect rect,
	Desktop* desktop)
	:
	SATDecorator(settings, rect, desktop),
	fCStatus(B_NO_INIT)
{
	STRACE(("B6Decorator:\n"));
	STRACE(("\tFrame (%.1f,%.1f,%.1f,%.1f)\n",
		rect.left, rect.top, rect.right, rect.bottom));

	fCloseBitmap = _CreateTemporaryBitmap(BRect(0, 0, 9, 9));
	fBigZoomBitmap = _CreateTemporaryBitmap(BRect(0, 0, 6, 6));
	fSmallZoomBitmap = _CreateTemporaryBitmap(BRect(0, 0, 4, 4));
	fGlintBitmap = _CreateTemporaryBitmap(BRect(0, 0, 2, 2));
		// glint bitmap is used by close and zoom buttons
		// the tab's flag overhang and the grab bar are both drawn
		// procedurally (see _DrawTab() and _DrawGrabBar()), so there is
		// no b6 theme artwork to load here

	if (fCloseBitmap == NULL || fBigZoomBitmap == NULL
		|| fSmallZoomBitmap == NULL || fGlintBitmap == NULL) {
		fCStatus = B_NO_MEMORY;
	} else
		fCStatus = B_OK;
}


B6Decorator::~B6Decorator()
{
	STRACE(("B6Decorator: ~B6Decorator()\n"));
	//delete[] fFrameColors;

	if (fCloseBitmap != NULL)
		fCloseBitmap->ReleaseReference();

	if (fBigZoomBitmap != NULL)
		fBigZoomBitmap->ReleaseReference();

	if (fSmallZoomBitmap != NULL)
		fSmallZoomBitmap->ReleaseReference();

	if (fGlintBitmap != NULL)
		fGlintBitmap->ReleaseReference();
}


// #pragma mark - Public methods


/*!	Returns the frame colors for the specified decorator component.

	The meaning of the color array elements depends on the specified component.
	For some components some array elements are unused.

	Unlike BeDecorator, the tab and button colors here are the fixed b6
	theme palette rather than colors derived from the user's system tab
	color, since the yellow tab and lavender buttons are the signature
	look of this skin. The frame (border) colors and the stack & tile /
	resize-border highlight feedback still follow the same scheme as
	BeDecorator.

	\param component The component for which to return the frame colors.
	\param highlight The highlight set for the component.
	\param colors An array of colors to be initialized by the function.
*/
void
B6Decorator::GetComponentColors(Component component, uint8 highlight,
	ComponentColors _colors, Decorator::Tab* _tab)
{
	Decorator::Tab* tab = static_cast<Decorator::Tab*>(_tab);
	switch (component) {
		case COMPONENT_TAB:
		{
			bool active = highlight == HIGHLIGHT_STACK_AND_TILE
				|| (tab != NULL && tab->buttonFocus);

			rgb_color tabColor = active ? kActiveTabColor : kInactiveTabColor;
			rgb_color tabLight = active ? kActiveTabLight : kInactiveTabLight;
			rgb_color tabShadow = active
				? kActiveTabShadow : kInactiveTabShadow;

			if (highlight == HIGHLIGHT_STACK_AND_TILE) {
				tabColor = tint_color(tabColor, B_DARKEN_1_TINT);
				tabLight = tint_color(tabLight, B_DARKEN_1_TINT);
			}

			_colors[COLOR_TAB_FRAME_LIGHT]
				= tint_color(kFrameColor, B_DARKEN_2_TINT);
			_colors[COLOR_TAB_FRAME_DARK]
				= tint_color(kFrameColor, B_DARKEN_3_TINT);
			_colors[COLOR_TAB] = tabColor;
			_colors[COLOR_TAB_LIGHT] = tabLight;
			_colors[COLOR_TAB_BEVEL] = tabLight;
			_colors[COLOR_TAB_SHADOW] = tabShadow;
			_colors[COLOR_TAB_TEXT] = kTextColor;
			break;
		}

		case COMPONENT_CLOSE_BUTTON:
		case COMPONENT_ZOOM_BUTTON:
			if (highlight == HIGHLIGHT_STACK_AND_TILE
					|| (tab != NULL && tab->buttonFocus)) {
				_colors[COLOR_BUTTON] = kActiveButtonColor;
				_colors[COLOR_BUTTON_LIGHT] = kActiveButtonLight;
			} else {
				_colors[COLOR_BUTTON] = kInactiveButtonColor;
				_colors[COLOR_BUTTON_LIGHT] = kInactiveButtonLight;
			}
			break;

		case COMPONENT_LEFT_BORDER:
		case COMPONENT_RIGHT_BORDER:
		case COMPONENT_TOP_BORDER:
		case COMPONENT_BOTTOM_BORDER:
		case COMPONENT_RESIZE_CORNER:
		default:
		{
			rgb_color base = kFrameColor;

			_colors[0].red = std::max(0, base.red - 72);
			_colors[0].green = std::max(0, base.green - 72);
			_colors[0].blue = std::max(0, base.blue - 72);
			_colors[0].alpha = 255;

			_colors[1].red = std::min(255, base.red + 64);
			_colors[1].green = std::min(255, base.green  + 64);
			_colors[1].blue = std::min(255, base.blue  + 64);
			_colors[1].alpha = 255;

			_colors[2].red = std::max(0, base.red - 8);
			_colors[2].green = std::max(0, base.green - 8);
			_colors[2].blue = std::max(0, base.blue - 8);
			_colors[2].alpha = 255;

			_colors[3].red = std::max(0, base.red - 88);
			_colors[3].green = std::max(0, base.green - 88);
			_colors[3].blue = std::max(0, base.blue - 88);
			_colors[3].alpha = 255;

			_colors[4].red = std::max(0, base.red - 72);
			_colors[4].green = std::max(0, base.green - 72);
			_colors[4].blue = std::max(0, base.blue - 72);
			_colors[4].alpha = 255;

			_colors[5].red = std::max(0, base.red - 128);
			_colors[5].green = std::max(0, base.green - 128);
			_colors[5].blue = std::max(0, base.blue - 128);
			_colors[5].alpha = 255;

			// for the resize-border highlight dye everything bluish.
			if (highlight == HIGHLIGHT_RESIZE_BORDER) {
				for (int32 i = 0; i < 6; i++) {
					_colors[i].red = std::max((int)_colors[i].red - 80, 0);
					_colors[i].green = std::max((int)_colors[i].green - 80, 0);
					_colors[i].blue = 255;
				}
			} else {
				// kFrameColor is a light warm grey, so even the darkest
				// shade the offsets above produce (_colors[5]) only
				// reaches a middling grey -- nowhere near the crisp
				// black edge Haiku's default decorator outlines its
				// windows with. _DrawFrame() always strokes the true
				// outer edge of the top/left border with _colors[0] and
				// of the bottom/right border with _colors[5] (see its
				// "(4 - i) == 4 ? 5 : (4 - i)" index there), so forcing
				// just those two to pure black gives the window a solid
				// black outline while _colors[1..4] still carry the
				// bevel gradient toward the content.
				_colors[0] = kTextColor;
				_colors[5] = kTextColor;
			}
			break;
		}
	}
}


/*!	\brief Extends hit-testing to the tab's two flag overhangs.

	The overhangs (see _OverhangRect()) extend past the tab's normal
	tabRect but aren't part of it, so the base implementation's
	tab->tabRect.Contains(where) check would miss clicks and drags
	there. Treat a hit anywhere in either overhang as a hit on the tab.
*/
Decorator::Region
B6Decorator::RegionAt(BPoint where, int32& tabIndex) const
{
	Region region = TabDecorator::RegionAt(where, tabIndex);
	if (region != REGION_NONE)
		return region;

	if (fTabList.CountItems() == 1) {
		Decorator::Tab* tab = fTabList.ItemAt(0);
		BRect left = _OverhangRect(tab, true);
		BRect right = _OverhangRect(tab, false);
		if ((left.IsValid() && left.Contains(where))
			|| (right.IsValid() && right.Contains(where))) {
			tabIndex = 0;
			return REGION_TAB;
		}
	}

	return REGION_NONE;
}


// #pragma mark - Protected methods


/*!	\brief Grows the tracked tab region to include both flag overhangs.

	Haiku's redraw and move tracking (footprint, dirty regions on move,
	etc.) all key off fTabsRegion/fTitleBarRect rather than the tab's
	drawn pixels. Without this, the flags the b6 theme's corner artwork
	pokes past the tab would leave stale pixels behind when a window is
	dragged, since the desktop wouldn't know it needs to repaint them.
*/
void
B6Decorator::_DoTabLayout()
{
	TabDecorator::_DoTabLayout();

	if (fTabList.CountItems() != 1)
		return;

	Decorator::Tab* tab = fTabList.ItemAt(0);
	BRect left = _OverhangRect(tab, true);
	BRect right = _OverhangRect(tab, false);
	if (!left.IsValid() && !right.IsValid())
		return;

	_RepositionButtons(tab);
	_IncludeFlagRegion(fTabsRegion, tab, true);
	_IncludeFlagRegion(fTabsRegion, tab, false);
	if (left.IsValid())
		fTitleBarRect = fTitleBarRect | left;
	if (right.IsValid())
		fTitleBarRect = fTitleBarRect | right;
}


/*!	\brief Keeps both flag overhangs tracked (and repainted) across a
		resize.

	TabDecorator::_ResizeBy() has a fast path for the common single-tab
	case that recomputes tab->tabRect directly and calls the private
	_LayoutTabItems() itself, rather than going through _DoTabLayout().
	That means our _DoTabLayout() override above never runs during a
	resize, so without this, the overhang regions would silently stop
	being tracked (and stale pixels would linger) after the first
	resize. Re-derive and re-include them here instead, and add both
	their old and new position to \a dirty so they actually get
	redrawn.
*/
void
B6Decorator::_ResizeBy(BPoint offset, BRegion* dirty)
{
	BRect oldLeft, oldRight;
	if (fTabList.CountItems() == 1) {
		oldLeft = _OverhangRect(fTabList.ItemAt(0), true);
		oldRight = _OverhangRect(fTabList.ItemAt(0), false);
	}

	TabDecorator::_ResizeBy(offset, dirty);

	if (fTabList.CountItems() != 1)
		return;

	Decorator::Tab* tab = fTabList.ItemAt(0);
	BRect left = _OverhangRect(tab, true);
	BRect right = _OverhangRect(tab, false);
	if (!left.IsValid() && !right.IsValid())
		return;

	_RepositionButtons(tab);
	_IncludeFlagRegion(fTabsRegion, tab, true);
	_IncludeFlagRegion(fTabsRegion, tab, false);
	if (left.IsValid())
		fTitleBarRect = fTitleBarRect | left;
	if (right.IsValid())
		fTitleBarRect = fTitleBarRect | right;

	if (dirty != NULL) {
		if (oldLeft.IsValid())
			dirty->Include(oldLeft);
		if (oldRight.IsValid())
			dirty->Include(oldRight);
		if (left.IsValid())
			dirty->Include(left);
		if (right.IsValid())
			dirty->Include(right);
	}
}


/*!	\brief The area to the left (\a leftSide true) or right (false) of
		a single tab's tabRect where a "flag" (see _DrawTab()) is
		allowed to extend past the window's edge, scaled off the tab's
		own (font-dependent) height so it stays proportional. Invalid
		(and the corresponding flag skipped) for stacked tabs and for
		kLeftTitledWindowLook, where a horizontal flag doesn't apply.
*/
BRect
B6Decorator::_OverhangRect(Decorator::Tab* tab, bool leftSide) const
{
	if (tab == NULL || !tab->tabRect.IsValid()
		|| tab->look == kLeftTitledWindowLook
		|| fTabList.CountItems() != 1) {
		return BRect(0, 0, -1, -1);
	}

	const BRect& tabRect = tab->tabRect;
	float overhangWidth = tabRect.Height() * kFlagOverhangRatio;

	if (leftSide) {
		return BRect(tabRect.left - overhangWidth, tabRect.top,
			tabRect.left - 1, tabRect.bottom);
	}

	return BRect(tabRect.right + 1, tabRect.top,
		tabRect.right + overhangWidth, tabRect.bottom);
}


/*!	\brief Adds a flag's actual (rounded-cap) silhouette to \a region,
		one thin horizontal strip per pixel row, rather than its
		rectangular bounding box.

		fTabsRegion feeds GetFootprint(), which the desktop uses to
		decide what's "this window's" opaque, owned area -- so anything
		included there is excluded from the desktop's own drawing
		underneath it. _DrawTab()'s flags only ever paint the rounded
		caps themselves, not their full bounding squares, so including
		the squares here would claim territory this decorator never
		actually paints: exactly the unpainted (black) corners next to
		a cap that a rectangular fTabsRegion.Include(overhang) produced.
		Matching the tracked region to the painted shape keeps the
		untouched corners outside the window's claimed area entirely, so
		the desktop keeps drawing its own background there instead.
*/
void
B6Decorator::_IncludeFlagRegion(BRegion& region, Decorator::Tab* tab,
	bool leftSide) const
{
	BRect overhang = _OverhangRect(tab, leftSide);
	if (!overhang.IsValid())
		return;

	const BRect& tabRect = tab->tabRect;
	float capRadius = tabRect.Height() / 2.0f;
	float capCenterX = leftSide ? overhang.left + capRadius
		: overhang.right - capRadius;
	float capCenterY = (tabRect.top + tabRect.bottom) / 2.0f;
	float bodyEdge = leftSide ? tabRect.left : tabRect.right;

	// the straight body is already a plain rect
	if (leftSide)
		region.Include(BRect(capCenterX, tabRect.top, bodyEdge - 1,
			tabRect.bottom));
	else
		region.Include(BRect(bodyEdge + 1, tabRect.top, capCenterX,
			tabRect.bottom));

	// the rounded cap, approximated one row at a time from how far the
	// circle actually extends past capCenterX at that row
	// (x = sqrt(r^2 - dy^2))
	int32 top = (int32)floorf(tabRect.top);
	int32 bottom = (int32)ceilf(tabRect.bottom);
	for (int32 y = top; y <= bottom; y++) {
		float dy = (y + 0.5f) - capCenterY;
		if (fabsf(dy) >= capRadius)
			continue;
		float dx = sqrtf(capRadius * capRadius - dy * dy);
		if (leftSide)
			region.Include(BRect(capCenterX - dx, y, capCenterX, y));
		else
			region.Include(BRect(capCenterX, y, capCenterX + dx, y));
	}
}


void
B6Decorator::_DrawFrame(BRect invalid)
{
	STRACE(("_DrawFrame(%f,%f,%f,%f)\n", invalid.left, invalid.top,
		invalid.right, invalid.bottom));

	// NOTE: the DrawingEngine needs to be locked for the entire
	// time for the clipping to stay valid for this decorator

	if (fTopTab->look == B_NO_BORDER_WINDOW_LOOK)
		return;

	if (fBorderWidth <= 0)
		return;

	// Draw the border frame
	BRect r = BRect(fTopBorder.LeftTop(), fBottomBorder.RightBottom());
	switch ((int)fTopTab->look) {
		case B_TITLED_WINDOW_LOOK:
		case B_DOCUMENT_WINDOW_LOOK:
		case B_MODAL_WINDOW_LOOK:
		{
			// top
			if (invalid.Intersects(fTopBorder)) {
				ComponentColors colors;
				_GetComponentColors(COMPONENT_TOP_BORDER, colors, fTopTab);

				for (int8 i = 0; i < 5; i++) {
					fDrawingEngine->StrokeLine(BPoint(r.left + i, r.top + i),
						BPoint(r.right - i, r.top + i), colors[i]);
				}
				if (fTitleBarRect.IsValid()) {
					// grey along the bottom of the tab
					// (overwrites "white" from frame)
					fDrawingEngine->StrokeLine(
						BPoint(fTitleBarRect.left + 2,
							fTitleBarRect.bottom + 1),
						BPoint(fTitleBarRect.right - 2,
							fTitleBarRect.bottom + 1),
						colors[2]);
				}
			}
			// left
			if (invalid.Intersects(fLeftBorder.InsetByCopy(0, -fBorderWidth))) {
				ComponentColors colors;
				_GetComponentColors(COMPONENT_LEFT_BORDER, colors, fTopTab);

				for (int8 i = 0; i < 5; i++) {
					fDrawingEngine->StrokeLine(BPoint(r.left + i, r.top + i),
						BPoint(r.left + i, r.bottom - i), colors[i]);
				}
			}
			// bottom
			if (invalid.Intersects(fBottomBorder)) {
				ComponentColors colors;
				_GetComponentColors(COMPONENT_BOTTOM_BORDER, colors, fTopTab);

				for (int8 i = 0; i < 5; i++) {
					fDrawingEngine->StrokeLine(BPoint(r.left + i, r.bottom - i),
						BPoint(r.right - i, r.bottom - i),
						colors[(4 - i) == 4 ? 5 : (4 - i)]);
				}
			}
			// right
			if (invalid.Intersects(
					fRightBorder.InsetByCopy(0, -fBorderWidth))) {
				ComponentColors colors;
				_GetComponentColors(COMPONENT_RIGHT_BORDER, colors, fTopTab);

				for (int8 i = 0; i < 5; i++) {
					fDrawingEngine->StrokeLine(BPoint(r.right - i, r.top + i),
						BPoint(r.right - i, r.bottom - i),
						colors[(4 - i) == 4 ? 5 : (4 - i)]);
				}
			}
			break;
		}

		case B_FLOATING_WINDOW_LOOK:
		case kLeftTitledWindowLook:
		{
			// top
			if (invalid.Intersects(fTopBorder)) {
				ComponentColors colors;
				_GetComponentColors(COMPONENT_TOP_BORDER, colors, fTopTab);

				for (int8 i = 0; i < 3; i++) {
					fDrawingEngine->StrokeLine(BPoint(r.left + i, r.top + i),
						BPoint(r.right - i, r.top + i), colors[i * 2]);
				}
				if (fTitleBarRect.IsValid()
					&& fTopTab->look != kLeftTitledWindowLook) {
					// grey along the bottom of the tab
					// (overwrites "white" from frame)
					fDrawingEngine->StrokeLine(
						BPoint(fTitleBarRect.left + 2,
							fTitleBarRect.bottom + 1),
						BPoint(fTitleBarRect.right - 2,
							fTitleBarRect.bottom + 1), colors[2]);
				}
			}
			// left
			if (invalid.Intersects(fLeftBorder.InsetByCopy(0, -fBorderWidth))) {
				ComponentColors colors;
				_GetComponentColors(COMPONENT_LEFT_BORDER, colors, fTopTab);

				for (int8 i = 0; i < 3; i++) {
					fDrawingEngine->StrokeLine(BPoint(r.left + i, r.top + i),
						BPoint(r.left + i, r.bottom - i), colors[i * 2]);
				}
				if (fTopTab->look == kLeftTitledWindowLook
					&& fTitleBarRect.IsValid()) {
					// grey along the right side of the tab
					// (overwrites "white" from frame)
					fDrawingEngine->StrokeLine(
						BPoint(fTitleBarRect.right + 1,
							fTitleBarRect.top + 2),
						BPoint(fTitleBarRect.right + 1,
							fTitleBarRect.bottom - 2), colors[2]);
				}
			}
			// bottom
			if (invalid.Intersects(fBottomBorder)) {
				ComponentColors colors;
				_GetComponentColors(COMPONENT_BOTTOM_BORDER, colors, fTopTab);

				for (int8 i = 0; i < 3; i++) {
					fDrawingEngine->StrokeLine(BPoint(r.left + i, r.bottom - i),
						BPoint(r.right - i, r.bottom - i),
						colors[(2 - i) == 2 ? 5 : (2 - i) * 2]);
				}
			}
			// right
			if (invalid.Intersects(fRightBorder.InsetByCopy(0, -fBorderWidth))) {
				ComponentColors colors;
				_GetComponentColors(COMPONENT_RIGHT_BORDER, colors, fTopTab);

				for (int8 i = 0; i < 3; i++) {
					fDrawingEngine->StrokeLine(BPoint(r.right - i, r.top + i),
						BPoint(r.right - i, r.bottom - i),
						colors[(2 - i) == 2 ? 5 : (2 - i) * 2]);
				}
			}
			break;
		}

		case B_BORDERED_WINDOW_LOOK:
		{
			// TODO: Draw the borders individually!
			ComponentColors colors;
			_GetComponentColors(COMPONENT_LEFT_BORDER, colors, fTopTab);

			fDrawingEngine->StrokeRect(r, colors[5]);
			break;
		}

		default:
			// don't draw a border frame
			break;
	}

	// Draw the resize/grab bar in the bottom right corner if we're
	// supposed to. Unlike BeDecorator, this bar is drawn for every
	// resizable window look, not just document windows, and uses the
	// a rounded purple quarter-circle grab handle, enlarged ("extended")
	// well past the b6 theme's own 19x19 bottom-right-active/inactive.xpm
	// bracket, which is sharp-cornered rather than rounded.
	if (!(fTopTab->flags & B_NOT_RESIZABLE)) {
		switch ((int)fTopTab->look) {
			case B_DOCUMENT_WINDOW_LOOK:
			case B_TITLED_WINDOW_LOOK:
			case B_FLOATING_WINDOW_LOOK:
			case B_MODAL_WINDOW_LOOK:
			case kLeftTitledWindowLook:
			{
				static const float kGrabBarSize = 30.0f;
				BPoint corner(fRightBorder.right - 1,
					fBottomBorder.bottom - 1);
				BRect grabRect(corner.x - kGrabBarSize + 1,
					corner.y - kGrabBarSize + 1, corner.x, corner.y);

				if (!invalid.Intersects(grabRect))
					break;

				bool focus = fTopTab != NULL && IsFocus(fTopTab);

				rgb_color base = focus
					? kActiveButtonColor : kInactiveButtonColor;
				rgb_color light = focus
					? kActiveButtonLight : kInactiveButtonLight;
				rgb_color shadow = focus
					? kActiveButtonShadow : kInactiveButtonShadow;

				if (RegionHighlight(REGION_RIGHT_BOTTOM_CORNER)
						== HIGHLIGHT_RESIZE_BORDER) {
					base = (rgb_color){ 40, 40, 220, 255 };
					light = (rgb_color){ 130, 130, 255, 255 };
					shadow = (rgb_color){ 0, 0, 140, 255 };
						// dye the corner blue during an active resize drag
				}

				_DrawGrabBar(grabRect, base, light, shadow);
				break;
			}

			default:
				// don't draw resize corner
				break;
		}
	}
}


/*!	\brief Actually draws the tab

	This function is called when the tab itself needs drawn. Other items,
	like the window title or buttons, should not be drawn here.

	\param tab The \a tab to update.
	\param invalid The area of the \a tab to update.
*/
void
B6Decorator::_DrawTab(Decorator::Tab* tab, BRect invalid)
{
	STRACE(("_DrawTab(%.1f, %.1f, %.1f, %.1f)\n",
			invalid.left, invalid.top, invalid.right, invalid.bottom));
	const BRect& tabRect = tab->tabRect;
	BRect leftOverhang = _OverhangRect(tab, true);
	BRect rightOverhang = _OverhangRect(tab, false);
		// the areas past tabRect's left/right edges the flag caps are
		// allowed to poke into; see _OverhangRect() and _DoTabLayout()
	bool hasLeft = leftOverhang.IsValid();
	bool hasRight = rightOverhang.IsValid();

	// If a window has a tab, this will draw it and any buttons which are
	// in it. The overhangs are checked too since either can be dirty on
	// its own (e.g. a targeted repaint) without tabRect itself being
	// dirty.
	if (!tabRect.IsValid()
		|| (!invalid.Intersects(tabRect) && !invalid.Intersects(leftOverhang)
			&& !invalid.Intersects(rightOverhang))) {
		return;
	}

	ComponentColors colors;
	_GetComponentColors(COMPONENT_TAB, colors, tab);

	// Where there's a valid overhang, it supplies its own left/right
	// edge (a rounded flag cap, see _DrawFlag()) instead of the plain
	// vertical line a tab normally starts/ends with, and the top/bevel
	// edges extend to meet it instead of stopping at tabRect.left/right.
	float frameLeft = hasLeft ? leftOverhang.left : tabRect.left;
	float frameRight = hasRight ? rightOverhang.right : tabRect.right;

	// outer frame
	if (!hasLeft) {
		fDrawingEngine->StrokeLine(tabRect.LeftTop(), tabRect.LeftBottom(),
			colors[COLOR_TAB_FRAME_LIGHT]);
	}
	fDrawingEngine->StrokeLine(BPoint(frameLeft, tabRect.top),
		BPoint(frameRight, tabRect.top), colors[COLOR_TAB_FRAME_LIGHT]);
	if (tab->look != kLeftTitledWindowLook) {
		if (!hasRight) {
			fDrawingEngine->StrokeLine(tabRect.RightTop(),
				tabRect.RightBottom(), colors[COLOR_TAB_FRAME_DARK]);
		}
	} else {
		fDrawingEngine->StrokeLine(tabRect.LeftBottom(),
			tabRect.RightBottom(), colors[COLOR_TAB_FRAME_DARK]);
	}

	float tabBotton = tabRect.bottom;
	if (fTopTab != tab)
		tabBotton -= 1;

	// bevel
	if (!hasLeft) {
		fDrawingEngine->StrokeLine(BPoint(tabRect.left + 1, tabRect.top + 1),
			BPoint(tabRect.left + 1,
				tabBotton - (tab->look == kLeftTitledWindowLook ? 1 : 0)),
			colors[COLOR_TAB_BEVEL]);
	}
	fDrawingEngine->StrokeLine(BPoint(frameLeft + 1, tabRect.top + 1),
		BPoint(frameRight - (tab->look == kLeftTitledWindowLook ? 0 : 1),
			tabRect.top + 1),
		colors[COLOR_TAB_BEVEL]);

	if (tab->look != kLeftTitledWindowLook) {
		if (!hasRight) {
			fDrawingEngine->StrokeLine(
				BPoint(tabRect.right - 1, tabRect.top + 2),
				BPoint(tabRect.right - 1, tabBotton),
				colors[COLOR_TAB_SHADOW]);
		}
	} else {
		fDrawingEngine->StrokeLine(
			BPoint(tabRect.left + 2, tabRect.bottom - 1),
			BPoint(tabRect.right, tabRect.bottom - 1),
			colors[COLOR_TAB_SHADOW]);
	}

	// fill
	if (fTopTab->look != kLeftTitledWindowLook) {
		fDrawingEngine->FillRect(BRect(tabRect.left + 2, tabRect.top + 2,
			tabRect.right - 2, tabRect.bottom), colors[COLOR_TAB]);
	} else {
		fDrawingEngine->FillRect(BRect(tabRect.left + 2, tabRect.top + 2,
			tabRect.right, tabRect.bottom - 2), colors[COLOR_TAB]);
	}

	_DrawFlag(leftOverhang, tabRect, true, colors);
	_DrawFlag(rightOverhang, tabRect, false, colors);

	_DrawTitle(tab, tabRect);

	_DrawButtons(tab, invalid);
}


/*!	\brief Draws one rounded "flag" extending the tab out past the
		window's left (\a leftSide true) or right (false) edge (see
		_OverhangRect()/_DoTabLayout()): a semicircular cap at the
		overhang's outer edge plus a straight body connecting it to
		\a tabRect. This is original artwork rather than a reproduction
		of the b6 theme's own top-left-active/inactive.xpm, which is a
		vertical (taller-than-tab) ribbon shape with no horizontal
		counterpart to draw from. Does nothing if \a overhang is invalid
		(see _OverhangRect()).
*/
void
B6Decorator::_DrawFlag(BRect overhang, const BRect& tabRect, bool leftSide,
	ComponentColors colors)
{
	if (!overhang.IsValid())
		return;

	float capRadius = tabRect.Height() / 2.0f;
	float capCenterX = leftSide ? overhang.left + capRadius
		: overhang.right - capRadius;
	BPoint capCenter(capCenterX, (tabRect.top + tabRect.bottom) / 2.0f);
	BRect capRect(capCenter.x - capRadius, capCenter.y - capRadius,
		capCenter.x + capRadius, capCenter.y + capRadius);
	float bodyEdge = leftSide ? tabRect.left : tabRect.right;

	BGradientLinear gradient;
	if (leftSide) {
		gradient.SetStart(BPoint(overhang.left, tabRect.top));
		gradient.SetEnd(BPoint(tabRect.left, tabRect.bottom));
	} else {
		gradient.SetStart(BPoint(overhang.right, tabRect.top));
		gradient.SetEnd(BPoint(tabRect.right, tabRect.bottom));
	}
	gradient.AddColor(colors[COLOR_TAB_BEVEL], 0);
	gradient.AddColor(colors[COLOR_TAB], 140);
	gradient.AddColor(colors[COLOR_TAB_SHADOW], 255);

	fDrawingEngine->DrawEllipse(capRect, true, gradient);
	if (leftSide) {
		fDrawingEngine->FillRect(
			BRect(capCenter.x, tabRect.top, bodyEdge + 1, tabRect.bottom),
			gradient);
	} else {
		fDrawingEngine->FillRect(
			BRect(bodyEdge - 1, tabRect.top, capCenter.x, tabRect.bottom),
			gradient);
	}

	// The outline is stitched together from an arc and two straight
	// lines, which (being separate draw calls) don't always meet at the
	// exact same pixel: a slightly larger stroke radius makes sure the
	// arc fully covers the fill's edge instead of leaving a sliver of
	// unstroked fill poking past it, and overlapping the arc's span and
	// the lines' start points by a few pixels/degrees closes the gap
	// that otherwise shows as a stray dot where the cap meets the tab's
	// top/bottom edge.
	//
	// Uses the tab's own bevel/shadow tones (the gradient's own start
	// and end colors above) rather than COLOR_TAB_FRAME_LIGHT/DARK: the
	// frame colors are a grey derived independently of the tab color,
	// which reads as a much harsher, higher-contrast line against the
	// yellow fill than an outline drawn from colors already in the
	// gradient it's outlining.
	const float kStrokeOverscan = 2.0f;
	BRect strokeRect = capRect.InsetByCopy(-kStrokeOverscan,
		-kStrokeOverscan);
	float capNear = leftSide ? capCenter.x - 3 : capCenter.x + 3;

	fDrawingEngine->SetHighColor(colors[COLOR_TAB_BEVEL]);
	fDrawingEngine->DrawArc(strokeRect, leftSide ? 80.0f : 350.0f, 110.0f,
		false);
	fDrawingEngine->StrokeLine(BPoint(capNear, tabRect.top),
		BPoint(bodyEdge, tabRect.top), colors[COLOR_TAB_BEVEL]);

	fDrawingEngine->SetHighColor(colors[COLOR_TAB_SHADOW]);
	fDrawingEngine->DrawArc(strokeRect, leftSide ? 170.0f : 260.0f, 110.0f,
		false);
	fDrawingEngine->StrokeLine(BPoint(capNear, tabRect.bottom),
		BPoint(bodyEdge, tabRect.bottom), colors[COLOR_TAB_SHADOW]);
}


/*!	\brief Actually draws the title

	The main tasks for this function are to ensure that the decorator draws
	the title only in its own area and drawing the title itself.
	Using B_OP_COPY for drawing the title is recommended because of the marked
	performance hit of the other drawing modes, but it is not a requirement.

	\param _tab The \a tab to update.
	\param r area of the title to update.
*/
void
B6Decorator::_DrawTitle(Decorator::Tab* _tab, BRect r)
{
	STRACE(("_DrawTitle(%f, %f, %f, %f)\n", r.left, r.top, r.right, r.bottom));

	Decorator::Tab* tab = static_cast<Decorator::Tab*>(_tab);

	const BRect& tabRect = tab->tabRect;
	const BRect& closeRect = tab->closeRect;
	const BRect& zoomRect = tab->zoomRect;

	ComponentColors colors;
	_GetComponentColors(COMPONENT_TAB, colors, tab);

	fDrawingEngine->SetDrawingMode(B_OP_OVER);
	fDrawingEngine->SetHighColor(colors[COLOR_TAB_TEXT]);
	fDrawingEngine->SetLowColor(colors[COLOR_TAB]);
	fDrawingEngine->SetFont(fDrawState.Font());

	// figure out position of text
	font_height fontHeight;
	fDrawState.Font().GetHeight(fontHeight);

	BPoint titlePos;
	if (fTopTab->look != kLeftTitledWindowLook) {
		titlePos.x = closeRect.IsValid() ? closeRect.right + tab->textOffset
			: tabRect.left + tab->textOffset;
		titlePos.y = floorf(((tabRect.top + 2.0) + tabRect.bottom
			+ fontHeight.ascent + fontHeight.descent) / 2.0
			- fontHeight.descent + 0.5);
	} else {
		titlePos.x = floorf(((tabRect.left + 2.0) + tabRect.right
			+ fontHeight.ascent + fontHeight.descent) / 2.0
			- fontHeight.descent + 0.5);
		titlePos.y = zoomRect.IsValid() ? zoomRect.top - tab->textOffset
			: tabRect.bottom - tab->textOffset;
	}

	fDrawingEngine->SetFont(fDrawState.Font());

	fDrawingEngine->DrawString(tab->truncatedTitle.String(),
		tab->truncatedTitleLength, titlePos);

	fDrawingEngine->SetDrawingMode(B_OP_COPY);
}


/*!	\brief Actually draws the close button

	Unless a subclass has a particularly large button, it is probably
	unnecessary to check the update rectangle.

	\param _tab The \a tab to update.
	\param direct Draw without double buffering.
	\param rect The area of the button to update.
*/
void
B6Decorator::_DrawClose(Decorator::Tab* _tab, bool direct, BRect rect)
{
	STRACE(("_DrawClose(%f,%f,%f,%f)\n", rect.left, rect.top, rect.right,
		rect.bottom));

	Decorator::Tab* tab = static_cast<Decorator::Tab*>(_tab);

	int32 index = (tab->buttonFocus ? 0 : 1) + (tab->closePressed ? 0 : 2);
	ServerBitmap* bitmap = tab->closeBitmaps[index];
	if (bitmap == NULL) {
		bitmap = _GetBitmapForButton(tab, COMPONENT_CLOSE_BUTTON,
			tab->closePressed, rect.IntegerWidth(), rect.IntegerHeight());
		tab->closeBitmaps[index] = bitmap;
	}

	_DrawButtonHalo(rect, tab);
	_DrawButtonBitmap(bitmap, direct, rect);
}


/*!	\brief Actually draws the zoom button

	Unless a subclass has a particularly large button, it is probably
	unnecessary to check the update rectangle.

	\param _tab The \a tab to update.
	\param direct Draw without double buffering.
	\param rect The area of the button to update.
*/
void
B6Decorator::_DrawZoom(Decorator::Tab* _tab, bool direct, BRect rect)
{
	STRACE(("_DrawZoom(%f,%f,%f,%f)\n", rect.left, rect.top, rect.right,
		rect.bottom));

	if (rect.IntegerWidth() < 1)
		return;

	Decorator::Tab* tab = static_cast<Decorator::Tab*>(_tab);
	int32 index = (tab->buttonFocus ? 0 : 1) + (tab->zoomPressed ? 0 : 2);
	ServerBitmap* bitmap = tab->zoomBitmaps[index];
	if (bitmap == NULL) {
		bitmap = _GetBitmapForButton(tab, COMPONENT_ZOOM_BUTTON,
			tab->zoomPressed, rect.IntegerWidth(), rect.IntegerHeight());
		tab->zoomBitmaps[index] = bitmap;
	}

	_DrawButtonHalo(rect, tab);
	_DrawButtonBitmap(bitmap, direct, rect);
}


void
B6Decorator::_DrawMinimize(Decorator::Tab* tab, bool direct, BRect rect)
{
	// This decorator doesn't have this button
}


void
B6Decorator::_GetButtonSizeAndOffset(const BRect& tabRect, float* _offset,
	float* _size, float* _inset) const
{
	float tabSize = fTopTab->look == kLeftTitledWindowLook ?
		tabRect.Width() : tabRect.Height();

	*_offset = 5.0f;
	*_inset = 0.0f;

	if (fTopTab->look == kLeftTitledWindowLook) {
		*_size = std::max(0.0f, tabSize - 7.0f);
		return;
	}

	// Sized to match the round buttons _RepositionButtons() centers on
	// the flag caps; this only affects the framework's own min/max tab
	// size bookkeeping; the buttons' actual final position is set by
	// _RepositionButtons() after layout, decoupled from this offset.
	float capRadius = tabSize * kFlagCapRadiusRatio;
	*_size = std::max(0.0f, capRadius * 2.0f * kFlagButtonDiameterRatio);
}


/*!	\brief Centers the close/zoom buttons on their respective flag caps
		(see _OverhangRect()/_DrawFlag()) instead of leaving them at the
		position TabDecorator::_LayoutTabItems() (which only knows how
		to inset a button the same offset from both the tab's edge and
		its top, and can't be pointed further out than tabRect at all)
		puts them at. Left alone (falling back to the standard in-tab
		position) for kLeftTitledWindowLook or when there's no valid cap
		on that side -- stacked tabs, or B_NOT_CLOSABLE/B_NOT_ZOOMABLE.
*/
void
B6Decorator::_RepositionButtons(Decorator::Tab* tab) const
{
	if (tab == NULL || tab->look == kLeftTitledWindowLook
		|| !tab->tabRect.IsValid()) {
		return;
	}

	const BRect& tabRect = tab->tabRect;
	float capRadius = tabRect.Height() * kFlagCapRadiusRatio;
	float buttonRadius = capRadius * kFlagButtonDiameterRatio;
	float centerY = (tabRect.top + tabRect.bottom) / 2.0f;

	if ((tab->flags & B_NOT_CLOSABLE) == 0) {
		BRect left = _OverhangRect(tab, true);
		if (left.IsValid()) {
			float centerX = left.left + capRadius;
			tab->closeRect.Set(centerX - buttonRadius, centerY - buttonRadius,
				centerX + buttonRadius, centerY + buttonRadius);
		}
	}

	if ((tab->flags & B_NOT_ZOOMABLE) == 0) {
		BRect right = _OverhangRect(tab, false);
		if (right.IsValid()) {
			float centerX = right.right - capRadius;
			tab->zoomRect.Set(centerX - buttonRadius, centerY - buttonRadius,
				centerX + buttonRadius, centerY + buttonRadius);
		}
	}
}


// #pragma mark - Private methods


/*!
	\brief Draws a bevel around a rectangle.
	\param rect The rectangular area to draw in.
	\param down Whether or not the button is pressed down.
	\param light The light color to use.
	\param shadow The shadow color to use.
*/
void
B6Decorator::_DrawBevelRect(DrawingEngine* engine, const BRect rect, bool down,
	rgb_color light, rgb_color shadow)
{
	if (down) {
		BRect inner(rect.InsetByCopy(1.0f, 1.0f));

		engine->StrokeLine(rect.LeftBottom(), rect.LeftTop(), shadow);
		engine->StrokeLine(rect.LeftTop(), rect.RightTop(), shadow);
		engine->StrokeLine(inner.LeftBottom(), inner.LeftTop(), shadow);
		engine->StrokeLine(inner.LeftTop(), inner.RightTop(), shadow);

		engine->StrokeLine(rect.RightTop(), rect.RightBottom(), light);
		engine->StrokeLine(rect.RightBottom(), rect.LeftBottom(), light);
		engine->StrokeLine(inner.RightTop(), inner.RightBottom(), light);
		engine->StrokeLine(inner.RightBottom(), inner.LeftBottom(), light);
	} else {
		BRect r1(rect);
		r1.left += 1.0f;
		r1.top  += 1.0f;

		BRect r2(rect);
		r2.bottom -= 1.0f;
		r2.right  -= 1.0f;

		engine->StrokeRect(r2, shadow);
			// inner dark box
		engine->StrokeRect(rect, shadow);
			// outer dark box
		engine->StrokeRect(r1, light);
			// light box
	}
}


/*!
	\brief Draws a framed rectangle with a gradient.
	\param rect The rectangular area to draw in.
	\param startColor The start color of the gradient.
	\param endColor The end color of the gradient.
*/
void
B6Decorator::_DrawBlendedRect(DrawingEngine* engine, const BRect rect,
	bool down, rgb_color colorA, rgb_color colorB, rgb_color colorC,
	rgb_color colorD)
{
	BRect fillRect(rect.InsetByCopy(1.0f, 1.0f));

	BGradientLinear gradient;
	if (down) {
		gradient.SetStart(fillRect.RightBottom());
		gradient.SetEnd(fillRect.LeftTop());
	} else {
		gradient.SetStart(fillRect.LeftTop());
		gradient.SetEnd(fillRect.RightBottom());
	}

	gradient.AddColor(colorA, 0);
	gradient.AddColor(colorB, 95);
	gradient.AddColor(colorC, 159);
	gradient.AddColor(colorD, 255);

	engine->FillRect(fillRect, gradient);
}


void
B6Decorator::_DrawButtonBitmap(ServerBitmap* bitmap, bool direct, BRect rect)
{
	if (bitmap == NULL)
		return;

	bool copyToFrontEnabled = fDrawingEngine->CopyToFrontEnabled();
	fDrawingEngine->SetCopyToFrontEnabled(direct);
	drawing_mode oldMode;
	fDrawingEngine->SetDrawingMode(B_OP_OVER, oldMode);
	fDrawingEngine->DrawBitmap(bitmap, rect.OffsetToCopy(0, 0), rect);
	fDrawingEngine->SetDrawingMode(oldMode);
	fDrawingEngine->SetCopyToFrontEnabled(copyToFrontEnabled);
}


/*!	\brief Draws a soft shadow-toned disc slightly larger than a
		close/zoom button, underneath it, before _DrawButtonBitmap()
		draws the (antialiased, but still not pixel-identical to the
		cap's own circle) button bitmap on top.

		The button's circular mask (_MaskToCircle()) and the flag cap's
		circle (_DrawFlag()) are two independently rasterized circles
		that are only meant to be concentric, not identical pixel for
		pixel; any stray gap between them would otherwise show the
		cap's flat yellow through it. This halo means such a gap shows
		a soft shadow tone instead.
*/
void
B6Decorator::_DrawButtonHalo(BRect rect, Decorator::Tab* tab)
{
	ComponentColors colors;
	_GetComponentColors(COMPONENT_TAB, colors, tab);

	fDrawingEngine->SetHighColor(colors[COLOR_TAB_SHADOW]);
	fDrawingEngine->DrawEllipse(rect.InsetByCopy(-1.0f, -1.0f), true);
}


/*!
	\brief Draws a truly rounded grab bar in the bottom-right corner: a
		quarter-circle disc, tapering smoothly toward the window content,
		rather than the b6 theme's own sharp-mitred bottom-right-active/
		inactive.xpm bracket.

		The trick is drawing a full circle of radius rect.Width(),
		centered exactly on \a rect's outer (bottom-right) corner point:
		only the quarter of it that falls inside the window (inside
		\a rect, and clipped to the window bounds beyond that) is ever
		visible, which is exactly the rounded quarter we want, with no
		need for arbitrary path/clipping support. DrawEllipse() centers
		the circle in the *middle* of the BRect passed to it, not at one
		of its corners, so the bounding box below is built out from the
		corner point in both directions rather than up and to the left
		of it, to get the center where we actually want it.

	\param rect The bounding box for the grab bar; its bottom-right
		corner is the disc's center.
	\param base The base (mid) color of the diagonal gradient fill.
	\param light The light color, at \a rect's top-left.
	\param shadow The shadow color, at \a rect's bottom-right.
*/
void
B6Decorator::_DrawGrabBar(BRect rect, rgb_color base, rgb_color light,
	rgb_color shadow)
{
	BPoint center = rect.RightBottom();
	float radius = rect.Width();
	BRect circle(center.x - radius, center.y - radius,
		center.x + radius, center.y + radius);

	BGradientLinear gradient;
	gradient.SetStart(rect.LeftTop());
	gradient.SetEnd(rect.RightBottom());
	gradient.AddColor(light, 0);
	gradient.AddColor(base, 140);
	gradient.AddColor(shadow, 255);

	fDrawingEngine->DrawEllipse(circle, true, gradient);

	static const rgb_color kOutline = (rgb_color){ 41, 41, 41, 255 };
	fDrawingEngine->SetHighColor(kOutline);
	fDrawingEngine->DrawArc(circle, 0.0f, 360.0f, false);
}


ServerBitmap*
B6Decorator::_GetBitmapForButton(Decorator::Tab* tab, Component item,
	bool down, int32 width, int32 height)
{
	uint8* data;
	size_t size;
	size_t offset;

	// TODO: the list of shared bitmaps is never freed
	struct decorator_bitmap {
		Component			item;
		bool				down;
		int32				width;
		int32				height;
		rgb_color			baseColor;
		rgb_color			lightColor;
		UtilityBitmap*		bitmap;
		decorator_bitmap*	next;
	};

	static BLocker sBitmapListLock("decorator lock", true);
	static decorator_bitmap* sBitmapList = NULL;

	// b6 theme button colors are fixed (see GetComponentColors() above),
	// the light/shadow bevel tones below are derived from them the same
	// way BeDecorator derives its bevel tones from the BeOS R5 palette.

	ComponentColors colors;
	_GetComponentColors(item, colors, tab);

	const rgb_color buttonColor(colors[COLOR_BUTTON]);

	bool isGrayscale = buttonColor.red == buttonColor.green
		&& buttonColor.green == buttonColor.blue;

	rgb_color buttonColorLight1(buttonColor);
	buttonColorLight1.red = std::min(255, buttonColor.red + 35),
	buttonColorLight1.green = std::min(255, buttonColor.green + 35),
	buttonColorLight1.blue = std::min(255, buttonColor.blue
		+ (isGrayscale ? 35 : 0));
		// greyscale color stays grayscale

	rgb_color buttonColorLight2(buttonColor);
	buttonColorLight2.red = std::min(255, buttonColor.red + 52),
	buttonColorLight2.green = std::min(255, buttonColor.green + 52),
	buttonColorLight2.blue = std::min(255, buttonColor.blue + 26);

	rgb_color buttonColorShadow1(buttonColor);
	buttonColorShadow1.red = std::max(0, buttonColor.red - 21),
	buttonColorShadow1.green = std::max(0, buttonColor.green - 21),
	buttonColorShadow1.blue = std::max(0, buttonColor.blue - 21);

	BAutolock locker(sBitmapListLock);

	// search our list for a matching bitmap
	// TODO: use a hash map instead?
	decorator_bitmap* current = sBitmapList;
	while (current) {
		if (current->item == item && current->down == down
			&& current->width == width && current->height == height
			&& current->baseColor == colors[COLOR_BUTTON]
			&& current->lightColor == colors[COLOR_BUTTON_LIGHT]) {
			return current->bitmap;
		}

		current = current->next;
	}

	static BitmapDrawingEngine* sBitmapDrawingEngine = NULL;

	// didn't find any bitmap, create a new one
	if (sBitmapDrawingEngine == NULL)
		sBitmapDrawingEngine = new(std::nothrow) BitmapDrawingEngine();
	if (sBitmapDrawingEngine == NULL
		|| sBitmapDrawingEngine->SetSize(width, height) != B_OK) {
		return NULL;
	}

	BRect rect(0, 0, width - 1, height - 1);

	STRACE(("B6Decorator creating bitmap for %s %s at size %ldx%ld\n",
		item == COMPONENT_CLOSE_BUTTON ? "close" : "zoom",
		down ? "down" : "up", width, height));
	switch (item) {
		case COMPONENT_CLOSE_BUTTON:
		{
			rgb_color buttonColorShadow2(buttonColor);
			buttonColorShadow2.red = std::max(0, buttonColor.red - 72),
			buttonColorShadow2.green = std::max(0, buttonColor.green - 72),
			buttonColorShadow2.blue = std::max(0, buttonColor.blue - 72);

			// fill the background
			sBitmapDrawingEngine->FillRect(rect, buttonColor);

			// draw outer bevel
			_DrawBevelRect(sBitmapDrawingEngine, rect, tab->closePressed,
				buttonColorLight2, buttonColorShadow2);

			if (fCStatus != B_OK) {
				// If we ran out of memory while initializing bitmaps
				// fall back to a linear gradient.
				rect.InsetBy(1, 1);
				_DrawBlendedRect(sBitmapDrawingEngine, rect, tab->closePressed,
					buttonColorLight2, buttonColorLight1, buttonColor,
					buttonColorShadow1);

				break;
			}

			// inset by bevel
			rect.InsetBy(2, 2);

			// fill bg
			sBitmapDrawingEngine->FillRect(rect, buttonColorLight1);

			// treat background color as transparent
			sBitmapDrawingEngine->SetDrawingMode(B_OP_OVER);
			sBitmapDrawingEngine->SetLowColor(buttonColorLight1);

			if (tab->closePressed) {
				// Draw glint in bottom right, then combined inner and outer
				// shadow in top left.
				// Read the source bitmap in forward while writing the
				// destination in reverse to rotate the bitmap by 180°.

				data = fGlintBitmap->Bits();
				size = sizeof(kGlintBits);
				for (size_t i = 0; i < size; i++) {
					offset = (size - 1 - i) * 4;
					if (kGlintBits[i] == 0) {
						// draw glint color
						data[offset + 0] = buttonColorLight2.blue;
						data[offset + 1] = buttonColorLight2.green;
						data[offset + 2] = buttonColorLight2.red;
					} else {
						// draw background color
						data[offset + 0] = buttonColorLight1.blue;
						data[offset + 1] = buttonColorLight1.green;
						data[offset + 2] = buttonColorLight1.red;
					}
				}
				// glint is 3x3
				const BRect rightBottom(BRect(rect.right - 2, rect.bottom - 2,
					rect.right, rect.bottom));
				sBitmapDrawingEngine->DrawBitmap(fGlintBitmap,
					fGlintBitmap->Bounds(), rightBottom);

				data = fCloseBitmap->Bits();
				size = sizeof(kOuterShadowBits);
				for (size_t i = 0; i < size; i++) {
					offset = (size - 1 - i) * 4;
					if (kOuterShadowBits[i] == 0) {
						// draw outer shadow
						data[offset + 0] = buttonColorShadow1.blue;
						data[offset + 1] = buttonColorShadow1.green;
						data[offset + 2] = buttonColorShadow1.red;
					} else if (kInnerShadowBits[i] == 0) {
						// draw inner shadow
						data[offset + 0] = buttonColor.blue;
						data[offset + 1] = buttonColor.green;
						data[offset + 2] = buttonColor.red;
					} else {
						// draw background color
						data[offset + 0] = buttonColorLight1.blue;
						data[offset + 1] = buttonColorLight1.green;
						data[offset + 2] = buttonColorLight1.red;
					}
				}
				// shadow is 10x10
				const BRect leftTop(rect.left, rect.top,
					rect.left + 9, rect.top + 9);
				sBitmapDrawingEngine->DrawBitmap(fCloseBitmap,
					fCloseBitmap->Bounds(), leftTop);
			} else {
				// draw glint, then draw combined outer and inner shadows

				data = fGlintBitmap->Bits();
				size = sizeof(kGlintBits);
				for (size_t i = 0; i < size; i++) {
					offset = i * 4 + 0;
					if (kGlintBits[i] == 0) {
						// draw glint color
						data[offset + 0] = buttonColorLight2.blue;
						data[offset + 1] = buttonColorLight2.green;
						data[offset + 2] = buttonColorLight2.red;
					} else {
						// draw background color
						data[offset + 0] = buttonColorLight1.blue;
						data[offset + 1] = buttonColorLight1.green;
						data[offset + 2] = buttonColorLight1.red;
					}
				}
				// glint is 3x3
				const BRect leftTop(rect.left, rect.top,
					rect.left + 2, rect.top + 2);
				sBitmapDrawingEngine->DrawBitmap(fGlintBitmap,
					fGlintBitmap->Bounds(), leftTop);

				data = fCloseBitmap->Bits();
				size = sizeof(kOuterShadowBits);
				for (size_t i = 0; i < size; i++) {
					offset = i * 4 + 0;
					if (kOuterShadowBits[i] == 0) {
						// draw outer shadow
						data[offset + 0] = buttonColorShadow1.blue;
						data[offset + 1] = buttonColorShadow1.green;
						data[offset + 2] = buttonColorShadow1.red;
					} else if (kInnerShadowBits[i] == 0) {
						// draw inner shadow
						data[offset + 0] = buttonColor.blue;
						data[offset + 1] = buttonColor.green;
						data[offset + 2] = buttonColor.red;
					} else {
						// draw background color
						data[offset + 0] = buttonColorLight1.blue;
						data[offset + 1] = buttonColorLight1.green;
						data[offset + 2] = buttonColorLight1.red;
					}
				}
				// shadow is 10x10
				const BRect rightBottom(BRect(rect.right - 9, rect.bottom - 9,
					rect.right, rect.bottom));
				sBitmapDrawingEngine->DrawBitmap(fCloseBitmap,
					fCloseBitmap->Bounds(), rightBottom);
			}

			// restore drawing mode
			sBitmapDrawingEngine->SetDrawingMode(B_OP_COPY);

			break;
		}

		case COMPONENT_ZOOM_BUTTON:
		{
			rgb_color buttonColorShadow2(buttonColor);
			buttonColorShadow2.red = std::max(0, buttonColor.red - 45),
			buttonColorShadow2.green = std::max(0, buttonColor.green - 45),
			buttonColorShadow2.blue = std::max(0, buttonColor.blue - 45);

			// fill the background
			sBitmapDrawingEngine->FillRect(rect, buttonColor);

			// big rect
			BRect bigRect(rect);
			bigRect.left += floorf(width * 3.0f / 14.0f);
			bigRect.top += floorf(height * 3.0f / 14.0f);

			// small rect
			BRect smallRect(rect);
			smallRect.right -= floorf(width * 5.0f / 14.0f);
			smallRect.bottom -= floorf(height * 5.0f / 14.0f);

			// draw big rect bevel
			_DrawBevelRect(sBitmapDrawingEngine, bigRect, tab->zoomPressed,
				buttonColorLight2, buttonColorShadow2);

			if (fCStatus != B_OK) {
				// If we ran out of memory while initializing bitmaps
				// fall back to a linear gradient.

				// already drew bigRect bevel, fill with linear gradient
				bigRect.InsetBy(1, 1);
				_DrawBlendedRect(sBitmapDrawingEngine, bigRect,
					tab->zoomPressed, buttonColorLight2, buttonColorLight1,
					buttonColor, buttonColorShadow1);

				// draw small rect bevel then fill with linear gradient
				_DrawBevelRect(sBitmapDrawingEngine, smallRect,
					tab->zoomPressed, buttonColorLight2, buttonColorShadow2);
				if (!tab->zoomPressed) {
					// undraw bottom left and top right corners
					sBitmapDrawingEngine->StrokePoint(smallRect.LeftBottom(),
						buttonColor);
					sBitmapDrawingEngine->StrokePoint(smallRect.RightTop(),
						buttonColor);
				}
				smallRect.InsetBy(1, 1);
				_DrawBlendedRect(sBitmapDrawingEngine, smallRect,
					tab->zoomPressed, buttonColorLight2, buttonColorLight1,
					buttonColor, buttonColorShadow1);

				break;
			}

			// inset past bevel
			bigRect.InsetBy(2, 2);

			// fill big rect bg
			sBitmapDrawingEngine->FillRect(bigRect, buttonColorLight1);

			// some elements are covered by the small rect
			// so only draw the parts that get shown
			if (tab->zoomPressed) {
				// draw glint
				// Read the source bitmap in forward while writing the
				// destination in reverse to rotate the bitmap by 180°.
				data = fGlintBitmap->Bits();
				size = sizeof(kGlintBits);
				for (size_t i = 0; i < sizeof(kGlintBits); i++) {
					offset = (size - 1 - i) * 4;
					if (kGlintBits[i] == 0) {
						// draw glint
						data[offset + 0] = buttonColorLight2.blue;
						data[offset + 1] = buttonColorLight2.green;
						data[offset + 2] = buttonColorLight2.red;
					} else {
						// draw background color
						data[offset + 0] = buttonColorLight1.blue;
						data[offset + 1] = buttonColorLight1.green;
						data[offset + 2] = buttonColorLight1.red;
					}
				}
				// glint is 3x3
				const BRect rightBottom(BRect(bigRect.right - 2,
					bigRect.bottom - 2, bigRect.right, bigRect.bottom));
				sBitmapDrawingEngine->DrawBitmap(fGlintBitmap,
					fGlintBitmap->Bounds(), rightBottom);
			} else {
				// draw combined inner and outer shadow
				data = fBigZoomBitmap->Bits();
				for (size_t i = 0; i < sizeof(kBigOuterShadowBits); i++) {
					offset = i * 4;
					if (kBigOuterShadowBits[i] == 0) {
						// draw outer shadow
						data[offset + 0] = buttonColorShadow1.blue;
						data[offset + 1] = buttonColorShadow1.green;
						data[offset + 2] = buttonColorShadow1.red;
					} else if (kBigInnerShadowBits[i] == 0) {
						// draw inner shadow
						data[offset + 0] = buttonColor.blue;
						data[offset + 1] = buttonColor.green;
						data[offset + 2] = buttonColor.red;
					} else {
						// draw background color
						data[offset + 0] = buttonColorLight1.blue;
						data[offset + 1] = buttonColorLight1.green;
						data[offset + 2] = buttonColorLight1.red;
					}
				}
				// shadow is 7x7
				const BRect rightBottom(BRect(bigRect.right - 6,
					bigRect.bottom - 6, bigRect.right, bigRect.bottom));
				sBitmapDrawingEngine->DrawBitmap(fBigZoomBitmap,
					fBigZoomBitmap->Bounds(), rightBottom);
			}

			sBitmapDrawingEngine->SetDrawingMode(B_OP_COPY);

			// draw small rect bevel
			_DrawBevelRect(sBitmapDrawingEngine, smallRect, tab->zoomPressed,
				buttonColorLight2, buttonColorShadow2);

			if (!tab->zoomPressed) {
				// undraw bottom left and top right corners
				sBitmapDrawingEngine->StrokePoint(smallRect.LeftBottom(),
					buttonColor);
				sBitmapDrawingEngine->StrokePoint(smallRect.RightTop(),
					buttonColor);
			}

			// inset past bevel
			smallRect.InsetBy(2, 2);

			// fill small rect bg
			sBitmapDrawingEngine->FillRect(smallRect, buttonColorLight1);

			// treat background color as transparent
			sBitmapDrawingEngine->SetDrawingMode(B_OP_OVER);
			sBitmapDrawingEngine->SetLowColor(buttonColorLight1);

			// draw small bitmap
			data = fSmallZoomBitmap->Bits();
			size = sizeof(kSmallOuterShadowBits);
			if (tab->zoomPressed) {
				// draw combined inner and outer shadow
				// Read the source bitmap in forward while writing the
				// destination in reverse to rotate the bitmap by 180°.
				for (size_t i = 0; i < size; i++) {
					offset = (size - 1 - i) * 4;
					if (kSmallOuterShadowBits[i] == 0) {
						// draw outer shadow
						data[offset + 0] = buttonColorShadow1.blue;
						data[offset + 1] = buttonColorShadow1.green;
						data[offset + 2] = buttonColorShadow1.red;
					} else if (kSmallInnerShadowBits[i] == 0) {
						// draw inner shadow
						data[offset + 0] = buttonColor.blue;
						data[offset + 1] = buttonColor.green;
						data[offset + 2] = buttonColor.red;
					} else {
						// draw background color
						data[offset + 0] = buttonColorLight1.blue;
						data[offset + 1] = buttonColorLight1.green;
						data[offset + 2] = buttonColorLight1.red;
					}
				}
				// shadow is 5x5
				const BRect smallLeftTop(BRect(smallRect.left,
					smallRect.top, smallRect.left + 4, smallRect.top + 4));
				sBitmapDrawingEngine->DrawBitmap(fSmallZoomBitmap,
					fSmallZoomBitmap->Bounds(), smallLeftTop);
			} else {
				// draw combined inner and outer shadow
				for (size_t i = 0; i < size; i++) {
					offset = i * 4;
					if (kSmallOuterShadowBits[i] == 0) {
						// draw outer shadow
						data[offset + 0] = buttonColorShadow1.blue;
						data[offset + 1] = buttonColorShadow1.green;
						data[offset + 2] = buttonColorShadow1.red;
					} else if (kSmallInnerShadowBits[i] == 0) {
						// draw inner shadow
						data[offset + 0] = buttonColor.blue;
						data[offset + 1] = buttonColor.green;
						data[offset + 2] = buttonColor.red;
					} else {
						// draw background color
						data[offset + 0] = buttonColorLight1.blue;
						data[offset + 1] = buttonColorLight1.green;
						data[offset + 2] = buttonColorLight1.red;
					}
				}
				// shadow is 5x5
				const BRect smallRightBottom(BRect(smallRect.right - 4,
					smallRect.bottom - 4, smallRect.right, smallRect.bottom));
				sBitmapDrawingEngine->DrawBitmap(fSmallZoomBitmap,
					fSmallZoomBitmap->Bounds(), smallRightBottom);
			}

			// draw glint last (single pixel)
			sBitmapDrawingEngine->StrokePoint(tab->zoomPressed
					? smallRect.RightBottom() : smallRect.LeftTop(),
				buttonColorLight2);

			// restore drawing mode
			sBitmapDrawingEngine->SetDrawingMode(B_OP_COPY);

			break;
		}

		default:
			break;
	}

	UtilityBitmap* bitmap = sBitmapDrawingEngine->ExportToBitmap(width, height,
		B_RGBA32);
	if (bitmap == NULL)
		return NULL;

	_MaskToCircle(bitmap, width, height);
		// b6 buttons are round: reuses the existing square bevel/shadow
		// drawing above unchanged, and just punches out the corners
		// outside the button's inscribed circle afterward, so the flag
		// cap's own background shows through them instead

	// bitmap ready, put it into the list
	decorator_bitmap* entry = new(std::nothrow) decorator_bitmap;
	if (entry == NULL) {
		delete bitmap;
		return NULL;
	}

	entry->item = item;
	entry->down = down;
	entry->width = width;
	entry->height = height;
	entry->bitmap = bitmap;
	entry->baseColor = colors[COLOR_BUTTON];
	entry->lightColor = colors[COLOR_BUTTON_LIGHT];
	entry->next = sBitmapList;
	sBitmapList = entry;
	return bitmap;
}


/*!	\brief Punches out the corners of a square button bitmap outside its
		inscribed circle (setting their alpha to 0), so a bevel drawn as
		a plain square (as _GetBitmapForButton() draws it, unmodified)
		reads as a round button once composited with B_OP_OVER: the
		masked-out corners simply reveal whatever is drawn underneath,
		i.e. the flag cap's own background.
*/
void
B6Decorator::_MaskToCircle(ServerBitmap* bitmap, int32 width,
	int32 height) const
{
	if (bitmap == NULL)
		return;

	uint8* bits = bitmap->Bits();
	int32 bytesPerRow = bitmap->BytesPerRow();
	float radius = std::min(width, height) / 2.0f;
	float centerX = width / 2.0f;
	float centerY = height / 2.0f;

	// Feather the last pixel of the radius into a soft, antialiased
	// edge instead of a hard on/off cutoff: a jagged cut against the
	// smoothly (natively) antialiased cap fill it sits on reads as a
	// harsh line even though neither side is drawn in a harsh color.
	const float kFeather = 1.0f;

	for (int32 y = 0; y < height; y++) {
		uint8* row = bits + y * bytesPerRow;
		for (int32 x = 0; x < width; x++) {
			float dx = (x + 0.5f) - centerX;
			float dy = (y + 0.5f) - centerY;
			float distance = sqrtf(dx * dx + dy * dy);
			float alpha = (radius + kFeather / 2.0f - distance) / kFeather;
			if (alpha < 0.0f)
				alpha = 0.0f;
			else if (alpha > 1.0f)
				alpha = 1.0f;
			row[x * 4 + 3] = (uint8)(alpha * 255.0f);
		}
	}
}


ServerBitmap*
B6Decorator::_CreateTemporaryBitmap(BRect bounds) const
{
	UtilityBitmap* bitmap = new(std::nothrow) UtilityBitmap(bounds,
		B_RGB32, 0);
	if (bitmap == NULL)
		return NULL;

	if (!bitmap->IsValid()) {
		delete bitmap;
		return NULL;
	}

	memset(bitmap->Bits(), 0, bitmap->BitsLength());
		// background opacity is 0

	return bitmap;
}


void
B6Decorator::_GetComponentColors(Component component,
	ComponentColors _colors, Decorator::Tab* tab)
{
	// get the highlight for our component
	Region region = REGION_NONE;
	switch (component) {
		case COMPONENT_TAB:
			region = REGION_TAB;
			break;
		case COMPONENT_CLOSE_BUTTON:
			region = REGION_CLOSE_BUTTON;
			break;
		case COMPONENT_ZOOM_BUTTON:
			region = REGION_ZOOM_BUTTON;
			break;
		case COMPONENT_LEFT_BORDER:
			region = REGION_LEFT_BORDER;
			break;
		case COMPONENT_RIGHT_BORDER:
			region = REGION_RIGHT_BORDER;
			break;
		case COMPONENT_TOP_BORDER:
			region = REGION_TOP_BORDER;
			break;
		case COMPONENT_BOTTOM_BORDER:
			region = REGION_BOTTOM_BORDER;
			break;
		case COMPONENT_RESIZE_CORNER:
			region = REGION_RIGHT_BOTTOM_CORNER;
			break;
	}

	return GetComponentColors(component, RegionHighlight(region), _colors, tab);
}


extern "C" DecorAddOn* (instantiate_decor_addon)(image_id id, const char* name)
{
	return new (std::nothrow)B6DecorAddOn(id, name);
}
