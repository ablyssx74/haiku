/*
 * Rounded outer corners for the Deskbar's window (SnakeTracker): see SnakeCorners.h.
 */


#include "SnakeCorners.h"

#include <Bitmap.h>
#include <FindDirectory.h>
#include <InterfaceDefs.h>
#include <Path.h>
#include <Screen.h>
#include <View.h>
#include <Window.h>

#include <math.h>
#include <stdio.h>
#include <string.h>


static const int kRadius = 7;		// the corner's radius, in pixels


// Tracker's settings file is plain text, one "Name value" per line: the corners follow "Enable rounded corners" and the
// Snake look (both on unless the file says otherwise).
static bool
Enabled()
{
	bool round = true, look = true;
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK && path.Append("Tracker/TrackerSettings") == B_OK) {
		FILE* file = fopen(path.Path(), "r");
		if (file != NULL) {
			char line[256];
			while (fgets(line, sizeof(line), file) != NULL) {
				const bool off = strstr(line, " off") != NULL || strstr(line, " 0") != NULL
					|| strstr(line, " false") != NULL;
				if (strncmp(line, "SnakeRoundCorners ", 18) == 0)
					round = !off;
				else if (strncmp(line, "SnakeLook ", 10) == 0)
					look = !off;
			}
			fclose(file);
		}
	}
	return round && look;
}


// A corner of the window: the picture has the screen behind the Deskbar outside the arc (and the outline's color along it),
// and nothing inside, where the Deskbar's own panel shows.
class SnakeCornerView : public BView {
public:
	SnakeCornerView(const BRect& frame, uint32 resizing)
		:
		BView(frame, "snake corner", resizing, B_WILL_DRAW),
		fPicture(NULL)
	{
		SetViewColor(B_TRANSPARENT_COLOR);
	}

	~SnakeCornerView()
	{
		delete fPicture;
	}

	void SetPicture(BBitmap* picture)
	{
		delete fPicture;
		fPicture = picture;
		Invalidate();
	}

	virtual void Draw(BRect)
	{
		if (fPicture == NULL)
			return;
		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		DrawBitmap(fPicture, BPoint(0, 0));
	}

private:
	BBitmap*	fPicture;
};


// One pixel wide: the line along a side of the window that is no longer the window system's border.
class SnakeEdgeView : public BView {
public:
	SnakeEdgeView(const BRect& frame, uint32 resizing, const rgb_color& color)
		:
		BView(frame, "snake edge", resizing, B_WILL_DRAW),
		fColor(color)
	{
		SetViewColor(B_TRANSPARENT_COLOR);
	}

	void SetColor(const rgb_color& color)
	{
		fColor = color;
		Invalidate();
	}

	virtual void Draw(BRect)
	{
		SetHighColor(fColor);
		FillRect(Bounds());
	}

private:
	rgb_color	fColor;
};


SnakeCorners::SnakeCorners(BWindow* deskbar)
	:
	fDeskbar(deskbar),
	fActive(false),
	fWorkspace(-1)
{
	for (int i = 0; i < kCornerCount; i++) {
		fCorners[i] = NULL;
		fRects[i] = BRect();
		fWanted[i] = false;
	}
	for (int i = 0; i < kEdgeCount; i++)
		fEdges[i] = NULL;
}


SnakeCorners::~SnakeCorners()
{
	// (the window is locked by its caller; the views go with the window)
}


// The window gets its own border back and the views go.
void
SnakeCorners::_Remove()
{
	for (int i = 0; i < kCornerCount; i++) {
		if (fCorners[i] != NULL) {
			fCorners[i]->RemoveSelf();
			delete fCorners[i];
			fCorners[i] = NULL;
		}
		fWanted[i] = false;
	}
	for (int i = 0; i < kEdgeCount; i++) {
		if (fEdges[i] != NULL) {
			fEdges[i]->RemoveSelf();
			delete fEdges[i];
			fEdges[i] = NULL;
		}
	}
	fActive = false;
}


static float
Clamp01(float v)
{
	return v < 0 ? 0 : (v > 1 ? 1 : v);
}


static rgb_color
PixelAt(BBitmap* bitmap, const BRect& bitmapRect, int x, int y)
{
	// x, y: screen coordinates
	int px = x - (int)bitmapRect.left, py = y - (int)bitmapRect.top;
	const int width = (int)bitmapRect.Width() + 1, height = (int)bitmapRect.Height() + 1;
	if (px < 0)
		px = 0;
	if (py < 0)
		py = 0;
	if (px >= width)
		px = width - 1;
	if (py >= height)
		py = height - 1;
	const uint8* row = (const uint8*)bitmap->Bits() + py * bitmap->BytesPerRow();
	return make_color(row[px * 4 + 2], row[px * 4 + 1], row[px * 4], 255);
}


// The R x R picture of one corner (rect: where it is on the screen): outside the arc the screen behind the Deskbar, along it a
// line of the border's color, and inside it nothing.
BBitmap*
SnakeCorners::_Picture(int corner, const BRect& rect, BBitmap* behind, const BRect& behindRect, const rgb_color& border)
{
	const bool left = corner == kTopLeft || corner == kBottomLeft;
	const bool top = corner == kTopLeft || corner == kTopRight;
	BBitmap* picture = new BBitmap(BRect(0, 0, kRadius - 1, kRadius - 1), B_RGBA32);
	if (picture->InitCheck() != B_OK) {
		delete picture;
		return NULL;
	}
	uint8* bits = (uint8*)picture->Bits();
	for (int y = 0; y < kRadius; y++) {
		for (int x = 0; x < kRadius; x++) {
			// the arc's centre is kRadius pixels in from the corner, on both axes
			const float cx = left ? x + 0.5f : (kRadius - x) - 0.5f;
			const float cy = top ? y + 0.5f : (kRadius - y) - 0.5f;
			const float d = hypotf(kRadius - cx, kRadius - cy);
			const float inside = Clamp01(kRadius - d + 0.5f);			// 1 within the arc
			const float edge = Clamp01(d - (kRadius - 1) + 0.5f);		// 1 on the outline
			const rgb_color back = PixelAt(behind, behindRect, (int)rect.left + x, (int)rect.top + y);
			const float aBack = 1 - inside;			// the screen behind, outside the arc
			const float aLine = inside * edge;		// the outline, on the arc
			const float total = aBack + aLine;
			uint8* pixel = bits + y * picture->BytesPerRow() + x * 4;
			if (total <= 0.001f) {
				pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
				continue;
			}
			pixel[0] = (uint8)((back.blue * aBack + border.blue * aLine) / total + 0.5f);
			pixel[1] = (uint8)((back.green * aBack + border.green * aLine) / total + 0.5f);
			pixel[2] = (uint8)((back.red * aBack + border.red * aLine) / total + 0.5f);
			pixel[3] = (uint8)(Clamp01(total) * 255 + 0.5f);
		}
	}
	return picture;
}


// The outline along each side that is not at a screen edge, between the corners.
void
SnakeCorners::_PlaceEdges(const bool* want, bool atLeft, bool atRight, bool atTop, bool atBottom, const rgb_color& line)
{
	const BRect bounds = fDeskbar->Bounds();
	// (the outline runs along every side, along the ones at a screen edge too)
	(void)atLeft;
	(void)atRight;
	(void)atTop;
	(void)atBottom;
	const bool sides[kEdgeCount] = { true, true, true, true };
	for (int s = 0; s < kEdgeCount; s++) {
		const bool horizontal = s == kTop || s == kBottom;
		const float from = horizontal ? (want[s == kTop ? kTopLeft : kBottomLeft] ? kRadius : 0)
			: (want[s == kLeft ? kTopLeft : kTopRight] ? kRadius : 0);
		const float to = horizontal ? bounds.right - (want[s == kTop ? kTopRight : kBottomRight] ? kRadius : 0)
			: bounds.bottom - (want[s == kLeft ? kBottomLeft : kBottomRight] ? kRadius : 0);
		BRect place;
		uint32 resizing;
		switch (s) {
			case kTop:
				place = BRect(from, 0, to, 0);
				resizing = B_FOLLOW_LEFT | B_FOLLOW_TOP;
				break;
			case kBottom:
				place = BRect(from, bounds.bottom, to, bounds.bottom);
				resizing = B_FOLLOW_LEFT | B_FOLLOW_TOP;
				break;
			case kLeft:
				place = BRect(0, from, 0, to);
				resizing = B_FOLLOW_LEFT | B_FOLLOW_TOP;
				break;
			default:
				place = BRect(bounds.right, from, bounds.right, to);
				resizing = B_FOLLOW_LEFT | B_FOLLOW_TOP;
				break;
		}
		if (!sides[s] || to < from) {
			if (fEdges[s] != NULL) {
				fEdges[s]->RemoveSelf();
				delete fEdges[s];
				fEdges[s] = NULL;
			}
			continue;
		}
		if (fEdges[s] == NULL) {
			fEdges[s] = new SnakeEdgeView(place, resizing, line);
			fDeskbar->AddChild(fEdges[s]);
		} else {
			fEdges[s]->MoveTo(place.LeftTop());
			fEdges[s]->ResizeTo(place.Width(), place.Height());
			fEdges[s]->SetColor(line);
		}
	}
}


void
SnakeCorners::Update(bool hidden)
{
	BScreen screen(fDeskbar);
	if (!screen.IsValid())
		return;
	const BRect screenFrame = screen.Frame();
	const BRect frame = fDeskbar->Frame();

	bool want[kCornerCount];
	BRect rects[kCornerCount];
	for (int i = 0; i < kCornerCount; i++)
		want[i] = false;
	bool atLeft = false, atRight = false, atTop = false, atBottom = false;
	const bool shown = !hidden && !fDeskbar->IsHidden() && frame.Width() > 2 * kRadius + 2
		&& frame.Height() > 2 * kRadius + 2;
	const bool rounded = shown && Enabled();
	if (shown) {
		const float slack = 1.5f;
		atLeft = frame.left <= screenFrame.left + slack;
		atRight = frame.right >= screenFrame.right - slack;
		atTop = frame.top <= screenFrame.top + slack;
		atBottom = frame.bottom >= screenFrame.bottom - slack;
		// a corner that is a corner of the screen stays square; any other is rounded
		want[kTopLeft] = rounded && !(atLeft && atTop);
		want[kTopRight] = rounded && !(atRight && atTop);
		want[kBottomLeft] = rounded && !(atLeft && atBottom);
		want[kBottomRight] = rounded && !(atRight && atBottom);
		const float r = kRadius - 1;
		rects[kTopLeft] = BRect(frame.left, frame.top, frame.left + r, frame.top + r);
		rects[kTopRight] = BRect(frame.right - r, frame.top, frame.right, frame.top + r);
		rects[kBottomLeft] = BRect(frame.left, frame.bottom - r, frame.left + r, frame.bottom);
		rects[kBottomRight] = BRect(frame.right - r, frame.bottom - r, frame.right, frame.bottom);
	}

	bool any = false, same = true;
	const int32 workspace = current_workspace();
	for (int i = 0; i < kCornerCount; i++) {
		any |= want[i];
		if (want[i] != fWanted[i] || (want[i] && rects[i] != fRects[i]))
			same = false;
	}
	if (!shown) {
		_Remove();
		return;
	}
	if (!any) {
		// (square corners: only the outline, no picture of the screen behind is needed)
		const rgb_color line = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_MAX_TINT);
		for (int i = 0; i < kCornerCount; i++) {
			if (fCorners[i] != NULL) {
				fCorners[i]->RemoveSelf();
				delete fCorners[i];
				fCorners[i] = NULL;
			}
			fWanted[i] = false;
		}
		_PlaceEdges(want, atLeft, atRight, atTop, atBottom, line);
		fActive = true;
		return;
	}
	if (same && fActive && workspace == fWorkspace)
		return;

	// The screen behind the Deskbar: hide it for a moment (and take no focus when it shows again).
	BRect area = frame & screenFrame;
	area.left = floorf(area.left);
	area.top = floorf(area.top);
	const uint32 flags = fDeskbar->Flags();
	fDeskbar->SetFlags(flags | B_AVOID_FOCUS);
	fDeskbar->Hide();
	BBitmap* behind = NULL;
	for (int attempt = 0; attempt < 4; attempt++) {
		snooze(attempt == 0 ? 50000 : 30000);
		BBitmap* shot = NULL;
		if (screen.GetBitmap(&shot, false, &area) != B_OK || shot == NULL)
			break;
		// the desktop repaints behind the Deskbar after it hides: take it again until it stops changing
		const bool settled = behind != NULL && behind->BitsLength() == shot->BitsLength()
			&& memcmp(behind->Bits(), shot->Bits(), shot->BitsLength()) == 0;
		delete behind;
		behind = shot;
		if (settled)
			break;
	}
	fDeskbar->Show();
	fDeskbar->SetFlags(flags);
	if (behind == NULL)
		return;

	fActive = true;
	const rgb_color line = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_MAX_TINT);
	const BRect bounds = fDeskbar->Bounds();
	for (int i = 0; i < kCornerCount; i++) {
		if (!want[i]) {
			if (fCorners[i] != NULL) {
				fCorners[i]->RemoveSelf();
				delete fCorners[i];
				fCorners[i] = NULL;
			}
			fWanted[i] = false;
			continue;
		}
		BBitmap* picture = _Picture(i, rects[i], behind, area, line);
		if (picture == NULL)
			continue;
		const bool left = i == kTopLeft || i == kBottomLeft;
		const bool top = i == kTopLeft || i == kTopRight;
		const float r = kRadius - 1;
		const BRect place(left ? 0 : bounds.right - r, top ? 0 : bounds.bottom - r, left ? r : bounds.right,
			top ? r : bounds.bottom);
		const uint32 resizing = B_FOLLOW_LEFT | B_FOLLOW_TOP;	// (placed again whenever the window changes)
		if (fCorners[i] == NULL) {
			fCorners[i] = new SnakeCornerView(place, resizing);
			fDeskbar->AddChild(fCorners[i]);
		} else {
			fCorners[i]->MoveTo(place.LeftTop());
		}
		fCorners[i]->SetPicture(picture);
		fWanted[i] = true;
		fRects[i] = rects[i];
	}

	_PlaceEdges(want, atLeft, atRight, atTop, atBottom, line);
	delete behind;
	fWorkspace = workspace;
}
