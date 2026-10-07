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
	kStateCount
};


class XfwmTheme {
public:
								XfwmTheme();

			// name: the folder name of the theme, looked up in the xfwm4-themes data folders
			bool				Load(const char* name);
			bool				IsValid() const { return fValid; }

			const XfwmImage&	TopLeft(bool active) const { return fTopLeft[active ? 0 : 1]; }
			const XfwmImage&	TopRight(bool active) const { return fTopRight[active ? 0 : 1]; }
			const XfwmImage&	Title(int32 index, bool active) const { return fTitle[index][active ? 0 : 1]; }
			const XfwmImage&	Left(bool active) const { return fLeft[active ? 0 : 1]; }
			const XfwmImage&	Right(bool active) const { return fRight[active ? 0 : 1]; }
			const XfwmImage&	Bottom(bool active) const { return fBottom[active ? 0 : 1]; }
			const XfwmImage&	BottomLeft(bool active) const { return fBottomLeft[active ? 0 : 1]; }
			const XfwmImage&	BottomRight(bool active) const { return fBottomRight[active ? 0 : 1]; }
			const XfwmImage&	Button(int32 button, int32 state) const { return fButton[button][state]; }

			// measurements, taken from the pictures
			int32				BorderWidth() const { return fBorderWidth; }
			int32				LeftMargin() const { return fLeftMargin; }	// transparent columns left of the border
			int32				TitleHeight() const { return fTitleHeight; }
			int32				ButtonWidth() const { return fButtonWidth; }

			rgb_color			TextColor(bool active) const { return active ? fActiveText : fInactiveText; }
			int32				TitleOffset(bool active) const { return active ? fOffsetActive : fOffsetInactive; }
			bool				TitleAlignLeft() const { return fAlignment == 0; }
			bool				ButtonsOnLeft(int32 button) const;
			int32				ButtonOffset() const { return fButtonOffset; }
			int32				ButtonSpacing() const { return fButtonSpacing; }
			const BString&		ButtonLayout() const { return fButtonLayout; }

private:
			void				_ReadThemerc(const char* path);
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
			XfwmImage			fButton[kButtonCount][kStateCount];

			int32				fBorderWidth;
			int32				fLeftMargin;
			int32				fTitleHeight;
			int32				fButtonWidth;

			rgb_color			fActiveText;
			rgb_color			fInactiveText;
			int32				fOffsetActive;
			int32				fOffsetInactive;
			int32				fAlignment;		// 0 left, 1 center, 2 right
			int32				fButtonOffset;
			int32				fButtonSpacing;
			BString				fButtonLayout;
};


#endif	// XFWM_THEME_H
