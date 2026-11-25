/*
 * A stand-in for app_server's private ServerBitmap and UtilityBitmap, so that XfwmTheme.cpp (which keeps the pictures of a
 * theme in them) compiles unchanged into a decorator style, which links with nothing of app_server's. Only what the theme
 * code uses: a block of pixels.
 */
#ifndef XFWM_STYLE_SERVER_BITMAP_H
#define XFWM_STYLE_SERVER_BITMAP_H


#include <GraphicsDefs.h>
#include <Rect.h>
#include <SupportDefs.h>

#include <stdlib.h>
#include <string.h>


class ServerBitmap {
public:
	ServerBitmap(BRect bounds, color_space space, uint32 flags)
		:
		fBits(NULL),
		fWidth((int32)bounds.Width() + 1),
		fHeight((int32)bounds.Height() + 1),
		fBytesPerRow(0)
	{
		if (space == B_RGBA32 && fWidth > 0 && fHeight > 0) {
			fBytesPerRow = fWidth * 4;
			fBits = (uint8*)calloc((size_t)fBytesPerRow * fHeight, 1);
		}
	}

	virtual ~ServerBitmap() { free(fBits); }

	bool	IsValid() const { return fBits != NULL; }
	uint8*	Bits() const { return fBits; }
	int32	BytesPerRow() const { return fBytesPerRow; }
	BRect	Bounds() const { return BRect(0, 0, fWidth - 1, fHeight - 1); }
	void	ReleaseReference() { delete this; }

private:
	uint8*	fBits;
	int32	fWidth;
	int32	fHeight;
	int32	fBytesPerRow;
};


class UtilityBitmap : public ServerBitmap {
public:
	UtilityBitmap(BRect bounds, color_space space, uint32 flags)
		: ServerBitmap(bounds, space, flags) {}
};


#endif	// XFWM_STYLE_SERVER_BITMAP_H
