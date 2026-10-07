/*
 * A window decorator that draws windows with an xfwm4 theme.
 *
 * The theme is chosen by the add-on's file name: "Xfwm-b6" shows the theme folder "b6" from
 * <data>/xfwm4-themes/. One program, many files: copy it once per theme (see decorators-native/).
 *
 * The title bar's tab can't be slid along the title bar: themes with a full-width bar, an extended grab bar and
 * other artwork on it would be pulled apart by it.
 */
#ifndef XFWM_DECORATOR_H
#define XFWM_DECORATOR_H


#include "DecorManager.h"
#include "SATDecorator.h"

#include "XfwmTheme.h"


class Desktop;


class XfwmDecorAddOn : public DecorAddOn {
public:
								XfwmDecorAddOn(image_id id, const char* name);

	virtual status_t			InitCheck() const;

protected:
	virtual Decorator*			_AllocateDecorator(DesktopSettings& settings, BRect rect, Desktop* desktop);

private:
			XfwmTheme			fTheme;
};


class XfwmDecorator : public SATDecorator {
public:
								XfwmDecorator(DesktopSettings& settings, BRect frame, Desktop* desktop,
									const XfwmTheme* theme);
	virtual						~XfwmDecorator();

	virtual	Region				RegionAt(BPoint where, int32& tab) const;

protected:
	virtual	void				_DoLayout();
	virtual	void				_DoTabLayout();
	virtual	void				_ResizeBy(BPoint offset, BRegion* dirty);
	virtual	void				_SetTitle(Decorator::Tab* tab, const char* string, BRegion* updateRegion = NULL);
	virtual	bool				_SetTabLocation(Decorator::Tab* tab, float location, bool isShifting,
									BRegion* updateRegion = NULL);
	virtual	void				_GetFootprint(BRegion* region);

	virtual	void				_DrawFrame(BRect rect);
	virtual	void				_DrawTab(Decorator::Tab* tab, BRect rect);
	virtual	void				_DrawTitle(Decorator::Tab* tab, BRect rect);
	virtual	void				_DrawButtons(Decorator::Tab* tab, const BRect& invalid);
	virtual	void				_DrawClose(Decorator::Tab* tab, bool direct, BRect rect);
	virtual	void				_DrawZoom(Decorator::Tab* tab, bool direct, BRect rect);
	virtual	void				_DrawMinimize(Decorator::Tab* tab, bool direct, BRect rect);

private:
			// where everything of the title bar goes, worked out from the frame and the theme
			struct BarLayout {
				int32	x0;				// left end of the cap (outside the window's left border)
				int32	y;				// top of the title bar
				int32	right;			// the window's outermost visible right column
				int32	textLeft;
				int32	textWidth;
				int32	title4X;
				int32	title5Start;
				int32	topRightX;
			};

			bool				_HasTab() const;
			void				_ComputeBar(Decorator::Tab* tab, BarLayout& bar) const;
			void				_LayoutButtons(Decorator::Tab* tab, const BarLayout& bar);
			void				_Blit(const XfwmImage& image, BPoint at);
			void				_BlitTiled(const XfwmImage& image, BRect area, bool horizontal);
			void				_DrawButton(Decorator::Tab* tab, int32 button, bool pressed, BRect rect, bool direct);
			bool				_Active(Decorator::Tab* tab) const { return tab->isFocused || tab->buttonFocus; }

			const XfwmTheme*	fTheme;
};


#endif	// XFWM_DECORATOR_H
