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

#include <vector>

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
	virtual	bool				_MoveTab(int32 from, int32 to, bool isMoving, BRegion* updateRegion = NULL);
	virtual	void				_SetFocus(Decorator::Tab* tab);
	virtual	void				_GetFootprint(BRegion* region);

	virtual	void				_DrawFrame(BRect rect);
	virtual	void				_DrawTab(Decorator::Tab* tab, BRect rect);
	virtual	void				_DrawTitle(Decorator::Tab* tab, BRect rect);
	virtual	void				_DrawButtons(Decorator::Tab* tab, const BRect& invalid);
	virtual	void				_DrawClose(Decorator::Tab* tab, bool direct, BRect rect);
	virtual	void				_DrawZoom(Decorator::Tab* tab, bool direct, BRect rect);
	virtual	void				_DrawMinimize(Decorator::Tab* tab, bool direct, BRect rect);

private:
							BRegion				fShown;
							BRegion				fUnreported;
								// areas the bar covered earlier that no change has told the desktop about yet
								// what the tabs covered after the last layout: the old area to clean up when the title bar changes size
			// where everything of the title bar goes, worked out from the frame and the theme
			struct TabSlot {
				int32	x;				// left end of the tab
				int32	width;
				int32	textLeft;		// where the title text may go: between the buttons
				int32	textWidth;
				int32	fillLeft;		// the stretched middle of the tab, which the buttons sit on
				int32	fillWidth;
			};

			struct BarLayout {
				int32	x0;				// left end of the cap (outside the window's left border)
				int32	y;				// top of the title bar
				int32	right;			// the window's outermost visible right column
				int32	frameEnd;		// the right end of xfwm4's frame (exclusive)
				int32	topRightX;
				int32	restStart;		// where the last tab ends and the plain bar begins
				std::vector<TabSlot>	slots;	// one per tab, in order
			};

			bool				_HasTab() const;
			bool				_IsModal() const;
			bool				_IsBordered() const;
			void				_ComputeBar(BarLayout& bar) const;
			void				_LayoutTabs();
			void				_LayoutButtons(Decorator::Tab* tab, const BarLayout& bar);
			// where the buttons of the front tab take over the bar: the title text goes between them
			void				_ButtonExtents(int32& leftEnd, int32& rightStart) const;
			void				_DrawBarEnd(const BarLayout& bar);
			void				_IncludeTab(BRegion& region, Decorator::Tab* tab, const BarLayout& bar) const;
			int32				_IndexOf(Decorator::Tab* tab) const;
			void				_Blit(const XfwmImage& image, BPoint at);
			void				_BlitTiled(const XfwmImage& image, BRect area, bool horizontal);

			// where the border pictures go: each is anchored by the edge that touches the window's content, so
			// no gap opens between the border and the content however wide the other borders are
			BRect				_LeftArea(bool active) const;
			BRect				_RightArea(bool active) const;
			BRect				_BottomArea(bool active) const;
			BRect				_TopFrameArea(bool active) const;
			// adds the opaque pixels of a tiled border picture, clipped to its area, to a region
			void				_IncludeTiled(BRegion& region, const XfwmImage& image, BRect area,
									bool horizontal) const;
			void				_DrawButton(Decorator::Tab* tab, int32 button, bool pressed, BRect rect, bool direct);
			bool				_Active(Decorator::Tab* tab) const { return tab->isFocused || tab->buttonFocus; }

			const XfwmTheme*	fTheme;
};


#endif	// XFWM_DECORATOR_H
