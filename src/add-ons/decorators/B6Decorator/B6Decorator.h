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
#ifndef B6_DECORATOR_H
#define B6_DECORATOR_H


#include "DecorManager.h"
#include "SATDecorator.h"


class Desktop;
class ServerBitmap;


class B6DecorAddOn : public DecorAddOn {
public:
								B6DecorAddOn(image_id id, const char* name);

protected:
	virtual Decorator*			_AllocateDecorator(DesktopSettings& settings,
										BRect rect, Desktop* desktop);
};


class B6Decorator: public SATDecorator {
public:
								B6Decorator(DesktopSettings& settings,
									BRect frame, Desktop* desktop);
	virtual						~B6Decorator();

	virtual	void				GetComponentColors(Component component,
									uint8 highlight, ComponentColors _colors,
									Decorator::Tab* tab = NULL);

	virtual	Region				RegionAt(BPoint where, int32& tab) const;

protected:
	virtual	void				_DoTabLayout();

	virtual	void				_DrawFrame(BRect rect);

	virtual	void				_DrawTab(Decorator::Tab* tab, BRect rect);
	virtual	void				_DrawTitle(Decorator::Tab* tab, BRect rect);
	virtual	void				_DrawClose(Decorator::Tab* tab, bool direct,
									BRect rect);
	virtual	void				_DrawZoom(Decorator::Tab* tab, bool direct,
									BRect rect);
	virtual	void				_DrawMinimize(Decorator::Tab* tab, bool direct,
									BRect rect);

	virtual	void				_GetButtonSizeAndOffset(const BRect& tabRect,
									float* offset, float* size,
									float* inset) const;

private:
			void				_DrawBevelRect(DrawingEngine* engine,
									const BRect rect, bool down,
									rgb_color light, rgb_color shadow);
			void				_DrawBlendedRect(DrawingEngine* engine,
									const BRect rect, bool down,
									rgb_color colorA, rgb_color colorB,
									rgb_color colorC, rgb_color colorD);
			void				_DrawButtonBitmap(ServerBitmap* bitmap,
									bool direct, BRect rect);
			void				_DrawGrabBar(BRect rect, rgb_color base,
									rgb_color light, rgb_color shadow);
			ServerBitmap*		_GetBitmapForButton(Decorator::Tab* tab,
									Component item, bool down, int32 width,
									int32 height);
			ServerBitmap* 		_CreateTemporaryBitmap(BRect bounds) const;
			ServerBitmap*		_CreateBitmapFromRGBA(int32 width,
									int32 height,
									const unsigned char* bgraData) const;
			void				_GetComponentColors(Component component,
									ComponentColors _colors,
									Decorator::Tab* tab = NULL);
			BRect				_OverhangRect(Decorator::Tab* tab) const;

private:
			status_t			fCStatus;

			ServerBitmap*		fCloseBitmap;
			ServerBitmap*		fBigZoomBitmap;
			ServerBitmap*		fSmallZoomBitmap;
			ServerBitmap*		fGlintBitmap;

			// b6 theme tab corner artwork, taken directly from the b6
			// xfwm4 theme's bitmaps, that overhangs the window's
			// top-left edge (see _OverhangRect()). The grab bar is
			// drawn procedurally instead (see _DrawGrabBar()).
			ServerBitmap*		fTopLeftActiveBitmap;
			ServerBitmap*		fTopLeftInactiveBitmap;
};


#endif	// B6_DECORATOR_H
