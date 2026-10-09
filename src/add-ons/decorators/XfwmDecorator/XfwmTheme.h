/*
 * An xfwm4 theme: the XPM pieces of a window frame (title bar, corners, borders, buttons) and the few values of its
 * themerc that matter to a Haiku decorator. See XfwmDecorator.
 */
#ifndef XFWM_THEME_H
#define XFWM_THEME_H


#include <GraphicsDefs.h>
#include <Rect.h>
#include <Region.h>
#include <String.h>

#include <vector>

class ServerBitmap;


// One piece of the frame, loaded from an XPM file ("None" pixels are transparent).
class XfwmImage {
public:
								XfwmImage();
								~XfwmImage();

			bool				Load(const char* path);
			// the same picture upside down (the bottom border as a top border)
			bool				LoadFlipped(const XfwmImage& source);
			// a lighter (or, on a light button, darker) copy of another picture, background only: the hover look of a
			// button the theme has no such picture for
			bool				LoadBrightened(const XfwmImage& source, float amount, const XfwmImage& bar);
			bool				IsValid() const { return fBitmap != NULL; }

			ServerBitmap*		Bitmap() const { return fBitmap; }
			int32				Width() const { return fWidth; }
			int32				Height() const { return fHeight; }

			// the part that is not transparent, as a bounding box
			BRect				OpaqueBounds() const { return fBounds; }

			// adds the opaque pixels of the image, drawn with its top left corner at (x, y), to a region
			// (a row of opaque pixels at a time)
			void				IncludeIn(BRegion& region, int32 x, int32 y) const;

private:
			bool				_Adopt(int32 width, int32 height, const std::vector<uint8>& bgra);

			struct Run {
				int32	row;
				int32	left;
				int32	right;
			};

			ServerBitmap*		fBitmap;
			int32				fWidth;
			int32				fHeight;
			BRect				fBounds;
			std::vector<Run>	fRuns;
};


enum {
	kButtonClose,
	kButtonMaximize,
	kButtonHide,
	kButtonShade,
	kButtonCount
};

enum {
	kStateActive,
	kStateInactive,
	kStatePressed,
	kStatePrelight,
	kStatePrelightInactive,
	kStateCount
};


class XfwmTheme {
public:
								XfwmTheme();

			// name: the folder name of the theme, looked up in the xfwm4-themes data folders
			bool				Load(const char* name);
			// loads the same theme again (after the palette changed)
			bool				Reload() { BString name(fName); return Load(name.String()); }
			bool				IsValid() const { return fValid; }

			// On a dark desktop the pictures' symbolic colours (xfwm4's "s active_color_2" and the like, which xfwm4
			// takes from the GTK theme) are drawn from the desktop's colours instead of the pictures' own, so the
			// borders are dark too. Returns true when this changed what the pictures should look like.
	static	bool				SetDarkPalette(bool dark, rgb_color tab, rgb_color inactiveTab, rgb_color panel);

			const XfwmImage&	TopLeft(bool active) const { return fTopLeft[active ? 0 : 1]; }
			const XfwmImage&	TopRight(bool active) const { return fTopRight[active ? 0 : 1]; }
			const XfwmImage&	Title(int32 index, bool active) const { return fTitle[index][active ? 0 : 1]; }
			const XfwmImage&	Left(bool active) const { return fLeft[active ? 0 : 1]; }
			const XfwmImage&	Right(bool active) const { return fRight[active ? 0 : 1]; }
			const XfwmImage&	Bottom(bool active) const { return fBottom[active ? 0 : 1]; }
			const XfwmImage&	BottomLeft(bool active) const { return fBottomLeft[active ? 0 : 1]; }
			const XfwmImage&	BottomRight(bool active) const { return fBottomRight[active ? 0 : 1]; }
			const XfwmImage&	Button(int32 button, int32 state) const { return fButton[button][state]; }

			// a top border and corners for windows without a title bar: the bottom ones, turned over
			const XfwmImage&	TopFrame(bool active) const { return fTopFrame[active ? 0 : 1]; }
			const XfwmImage&	TopLeftCorner(bool active) const { return fTopLeftCorner[active ? 0 : 1]; }
			const XfwmImage&	TopRightCorner(bool active) const { return fTopRightCorner[active ? 0 : 1]; }

			// the colour of the outermost line of the border (for one pixel frames)
			rgb_color			OutlineColor() const { return fOutline; }
			// the colour of the border's body (for filling in the document window's resize knob)
			rgb_color			FillColor() const { return fFill; }

			// measurements, taken from the pictures
			int32				BorderWidth() const { return fBorderWidth; }
			int32				LeftMargin() const { return fLeftMargin; }	// transparent columns left of the border
			int32				TitleHeight() const { return fTitleHeight; }
			int32				ButtonWidth() const { return fButtonWidth; }

			rgb_color			TextColor(bool active) const { return active ? fActiveText : fInactiveText; }
			int32				TitleOffset(bool active) const { return active ? fOffsetActive : fOffsetInactive; }
			// title_horizontal_offset: how far in from the edge the title starts (left aligned) or ends (right)
			int32				TitleHorizontalOffset() const { return fTitleOffsetX; }
			bool				TitleAlignLeft() const { return fAlignment == 0; }
			int32				TitleAlignment() const { return fAlignment; }
			// the title bar is one tab across the whole window (xfwm4's default), not a tab and a filler
			bool				FullWidthTitle() const { return fFullWidth; }
			bool				ButtonsOnLeft(int32 button) const;
			int32				ButtonOffset() const { return fButtonOffset; }
			int32				ButtonSpacing() const { return fButtonSpacing; }
			const BString&		ButtonLayout() const { return fButtonLayout; }

private:
			void				_ReadThemerc(const char* path);
			// the title text colour for a theme that names none: light on a dark bar, dark on a light one
			rgb_color			_ReadableOn(const XfwmImage& bar, bool active) const;
			bool				_FindFolder(const char* name, BString& path) const;

			bool				fValid;
			XfwmImage			fTopLeft[2];
			XfwmImage			fTopRight[2];
			XfwmImage			fTitle[5][2];
			XfwmImage			fLeft[2];
			XfwmImage			fRight[2];
			XfwmImage			fBottom[2];
			XfwmImage			fBottomLeft[2];
			XfwmImage			fBottomRight[2];
			XfwmImage			fTopFrame[2];
			XfwmImage			fTopLeftCorner[2];
			XfwmImage			fTopRightCorner[2];
			rgb_color			fOutline;
			rgb_color			fFill;
			XfwmImage			fButton[kButtonCount][kStateCount];

			int32				fBorderWidth;
			int32				fLeftMargin;
			int32				fTitleHeight;
			int32				fButtonWidth;

			rgb_color			fActiveText;
			rgb_color			fInactiveText;
			int32				fOffsetActive;
			int32				fTitleOffsetX;
			int32				fOffsetInactive;
			int32				fAlignment;		// 0 left, 1 center, 2 right
			bool				fFullWidth;
			bool				fHasActiveText;
			bool				fHasInactiveText;
			int32				fButtonOffset;
			int32				fButtonSpacing;
			BString				fButtonLayout;
			BString				fName;
};


#endif	// XFWM_THEME_H
