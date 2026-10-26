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

#include <OS.h>

#include <Messenger.h>

#include <unistd.h>

#include <math.h>
#include <algorithm>
#include <stdio.h>
#include <string.h>


static const uint32 kSnakeCornerPixels = 'SnkP';		// (the same code Tracker answers to)

static const int kRadius = 7;


// When ~/config/settings/snaketracker_wallpaper_debug exists, what the corners do is written to snaketracker_wallpaper.log.
static void
DebugLog(const char* text)
{
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK || path.Append("snaketracker_wallpaper_debug") != B_OK
		|| access(path.Path(), F_OK) != 0 || find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK
		|| path.Append("snaketracker_wallpaper.log") != B_OK)
		return;
	FILE* file = fopen(path.Path(), "a");
	if (file != NULL) {
		fprintf(file, "Deskbar: %s\n", text);
		fclose(file);
	}
}		// the corner's radius, in pixels


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
				if (strncmp(line, "SnakeDeskbarRoundCorners ", 25) == 0)
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


static void
FreeStrips(BBitmap** strips, int count)
{
	for (int i = 0; i < count; i++) {
		delete strips[i];
		strips[i] = NULL;
	}
}


SnakeCorners::SnakeCorners(BWindow* deskbar)
	:
	fDeskbar(deskbar),
	fActive(false),
	fWorkspace(-1),
	fAheadWorkspace(-1),
	fTrackerAnswers(false)
{
	for (int i = 0; i < kCornerCount; i++) {
		fCorners[i] = NULL;
		fRects[i] = BRect();
		fWanted[i] = false;
	}
	for (int i = 0; i < kEdgeCount; i++)
		fEdges[i] = NULL;
	for (int i = 0; i < kBackdropCount; i++) {
		for (int j = 0; j < kStripCount; j++)
			fBackdrops[i].strips[j] = NULL;
		fBackdrops[i].workspace = -1;
		fBackdrops[i].when = 0;
	}
	for (int i = 0; i < kCornerCount; i++)
		fAhead[i] = NULL;
}


SnakeCorners::~SnakeCorners()
{
	// (the window is locked by its caller; the views go with the window)
	for (int i = 0; i < kBackdropCount; i++)
		FreeStrips(fBackdrops[i].strips, kStripCount);
	_DropAhead();
}


// The kept strips for a workspace, if the screen is still the same size. (They are taken again whenever the Deskbar is hidden
// anyway, so they don't grow old.)
SnakeCorners::Backdrop*
SnakeCorners::_Backdrop(int32 workspace, const BRect& screenFrame)
{
	for (int i = 0; i < kBackdropCount; i++) {
		Backdrop& b = fBackdrops[i];
		if (b.strips[0] == NULL || b.workspace != workspace)
			continue;
		if (b.rects[0].top != screenFrame.top || b.rects[0].bottom != screenFrame.bottom) {
			FreeStrips(b.strips, kStripCount);
			return NULL;
		}
		return &b;
	}
	return NULL;
}


// The strip that holds rect, or -1.
int
SnakeCorners::_Strip(const Backdrop* backdrop, const BRect& rect)
{
	for (int i = 0; i < kStripCount; i++) {
		if (backdrop->strips[i] != NULL && backdrop->rects[i].Contains(rect))
			return i;
	}
	return -1;
}


void
SnakeCorners::_DropAhead()
{
	FreeStrips(fAhead, kCornerCount);
}


// The four corner places of a frame.
static void
CornerRects(const BRect& frame, BRect* rects)
{
	const float r = kRadius - 1;
	rects[0] = BRect(frame.left, frame.top, frame.left + r, frame.top + r);
	rects[1] = BRect(frame.right - r, frame.top, frame.right, frame.top + r);
	rects[2] = BRect(frame.left, frame.bottom - r, frame.left + r, frame.bottom);
	rects[3] = BRect(frame.right - r, frame.bottom - r, frame.right, frame.bottom);
}


void
SnakeCorners::WallpaperChanged()
{
	for (int i = 0; i < kBackdropCount; i++)
		FreeStrips(fBackdrops[i].strips, kStripCount);
	_DropAhead();
	fWorkspace = -1;		// (every corner takes its picture again at the next Update)
}


// Asks Tracker for the pixels of its wallpaper under the corners that need a picture (a bitmap for each, in shots; false when it
// could not tell, and none is made then).
bool
SnakeCorners::_FromTracker(const bool* redo, const BRect* rects, int32 workspace, BBitmap** shots)
{
	BMessage request(kSnakeCornerPixels), reply;
	request.AddInt32("workspace", workspace);
	int wanted = 0;
	for (int i = 0; i < kCornerCount; i++) {
		if (redo[i]) {
			request.AddRect("rect", rects[i]);
			wanted++;
		}
	}
	if (wanted == 0)
		return true;
	BMessenger tracker("application/x-vnd.Be-TRAK");
	if (!tracker.IsValid() || tracker.SendMessage(&request, &reply, 500000, 1500000) != B_OK)
		return false;

	int32 index = 0;
	for (int i = 0; i < kCornerCount; i++) {
		if (!redo[i])
			continue;
		BRect rect;
		const void* data;
		ssize_t size;
		int32 bytesPerRow;
		BBitmap* bitmap = new BBitmap(BRect(0, 0, rects[i].IntegerWidth(), rects[i].IntegerHeight()), B_RGB32);
		if (bitmap->InitCheck() != B_OK || reply.FindRect("rect", index, &rect) != B_OK || rect != rects[i]
			|| reply.FindData("pixels", B_RAW_TYPE, index, &data, &size) != B_OK
			|| reply.FindInt32("bytesperrow", index, &bytesPerRow) != B_OK || bytesPerRow <= 0
			|| size < (ssize_t)bytesPerRow * (rects[i].IntegerHeight() + 1)) {
			delete bitmap;
			for (int j = 0; j < kCornerCount; j++) {
				delete shots[j];
				shots[j] = NULL;
			}
			return false;
		}
		const int rows = rects[i].IntegerHeight() + 1;
		const int copy = std::min(bitmap->BytesPerRow(), bytesPerRow);
		for (int row = 0; row < rows; row++)
			memcpy((uint8*)bitmap->Bits() + row * bitmap->BytesPerRow(), (const uint8*)data + row * bytesPerRow, copy);
		shots[i] = bitmap;
		index++;
	}
	return true;
}


void
SnakeCorners::Prepare(const BRect& newFrame)
{
	_DropAhead();
	// (the grab below is only for when Tracker can't tell what is behind the corners)
	if (fTrackerAnswers)
		return;
	if (!fActive || !Enabled())
		return;
	BScreen screen(fDeskbar);
	if (!screen.IsValid())
		return;
	const BRect current = fDeskbar->Frame();
	BRect rects[kCornerCount];
	CornerRects(newFrame, rects);
	for (int i = 0; i < kCornerCount; i++) {
		// only ground the Deskbar does not cover now (a few pixels each)
		if (rects[i].Intersects(current) || !screen.Frame().Contains(rects[i]))
			continue;
		BRect area = rects[i];
		if (screen.GetBitmap(&fAhead[i], false, &area) != B_OK)
			fAhead[i] = NULL;
		else
			fAheadRects[i] = area;
	}
	fAheadWorkspace = current_workspace();
}


// Hides the Deskbar for a moment and takes the columns and rows of the screen along its four sides: a few thousand pixels,
// not the screen. (The Deskbar takes no focus when it shows again.)
SnakeCorners::Backdrop*
SnakeCorners::_Capture(int32 workspace, const BRect& frame, const BRect& screenFrame)
{
	const float r = kRadius - 1;
	BRect wanted[kStripCount] = {
		BRect(frame.left, screenFrame.top, frame.left + r, screenFrame.bottom),
		BRect(frame.right - r, screenFrame.top, frame.right, screenFrame.bottom),
		BRect(screenFrame.left, frame.top, screenFrame.right, frame.top + r),
		BRect(screenFrame.left, frame.bottom - r, screenFrame.right, frame.bottom)
	};
	BBitmap* strips[kStripCount];
	BBitmap* previous[kStripCount];
	for (int i = 0; i < kStripCount; i++) {
		strips[i] = previous[i] = NULL;
		wanted[i] = wanted[i] & screenFrame;
		wanted[i].left = floorf(wanted[i].left);
		wanted[i].top = floorf(wanted[i].top);
	}

	BScreen screen(fDeskbar);
	const uint32 flags = fDeskbar->Flags();
	fDeskbar->SetFlags(flags | B_AVOID_FOCUS);
	fDeskbar->Hide();
	bool ok = true;
	for (int attempt = 0; attempt < 3 && ok; attempt++) {
		snooze(attempt == 0 ? 40000 : 20000);
		bool settled = attempt > 0;
		for (int i = 0; i < kStripCount && ok; i++) {
			delete previous[i];
			previous[i] = strips[i];
			strips[i] = NULL;
			if (!wanted[i].IsValid())
				continue;
			if (screen.GetBitmap(&strips[i], false, &wanted[i]) != B_OK || strips[i] == NULL) {
				strips[i] = NULL;
				ok = false;
				break;
			}
			// the desktop repaints behind the Deskbar after it hides: take it again until it stops changing
			if (previous[i] == NULL || previous[i]->BitsLength() != strips[i]->BitsLength()
					|| memcmp(previous[i]->Bits(), strips[i]->Bits(), strips[i]->BitsLength()) != 0)
				settled = false;
		}
		if (settled)
			break;
	}
	fDeskbar->Show();
	fDeskbar->SetFlags(flags);
	FreeStrips(previous, kStripCount);
	if (!ok) {
		FreeStrips(strips, kStripCount);
		return NULL;
	}

	int slot = -1;
	for (int i = 0; i < kBackdropCount && slot < 0; i++) {
		if (fBackdrops[i].strips[0] == NULL || fBackdrops[i].workspace == workspace)
			slot = i;
	}
	if (slot < 0) {
		slot = 0;	// (the oldest goes)
		for (int i = 1; i < kBackdropCount; i++) {
			if (fBackdrops[i].when < fBackdrops[slot].when)
				slot = i;
		}
	}
	Backdrop& b = fBackdrops[slot];
	FreeStrips(b.strips, kStripCount);
	b.workspace = workspace;
	b.when = system_time();
	for (int i = 0; i < kStripCount; i++) {
		b.strips[i] = strips[i];
		b.rects[i] = wanted[i];
	}
	return &b;
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
		// (and the bottom corner of a Deskbar that hangs from the top of the screen, on the side of the screen's edge, is square too)
		want[kBottomLeft] = rounded && !(atLeft && atBottom) && !(atLeft && atTop);
		want[kBottomRight] = rounded && !(atRight && atBottom) && !(atRight && atTop);
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

	// A corner that stayed where it was keeps its picture. For the others Tracker is asked for the wallpaper behind them (so the
	// Deskbar need not hide to look). If it can't tell, a corner takes one of ground it was seen on before the window changed
	// (Prepare), or else out of the strips kept for this workspace; only when none of those reaches it is the Deskbar hidden for
	// a moment to take the strips again.
	const bool sameWorkspace = fActive && workspace == fWorkspace;
	const bool aheadOk = fAheadWorkspace == workspace;
	bool redo[kCornerCount], fromAhead[kCornerCount];
	BBitmap* tracked[kCornerCount];
	for (int i = 0; i < kCornerCount; i++) {
		redo[i] = fromAhead[i] = false;
		tracked[i] = NULL;
		if (want[i] && !(sameWorkspace && fWanted[i] && fRects[i] == rects[i] && fCorners[i] != NULL))
			redo[i] = true;
	}
	fTrackerAnswers = _FromTracker(redo, rects, workspace, tracked);
	DebugLog(fTrackerAnswers ? "corners from Tracker's wallpaper" : "Tracker could not tell");

	Backdrop* backdrop = _Backdrop(workspace, screenFrame);
	bool needHide = false;
	for (int i = 0; i < kCornerCount; i++) {
		if (!redo[i] || tracked[i] != NULL)
			continue;
		if (aheadOk && fAhead[i] != NULL && fAheadRects[i] == rects[i])
			fromAhead[i] = true;
		else if (backdrop == NULL || _Strip(backdrop, rects[i]) < 0)
			needHide = true;
	}
	if (needHide) {
		DebugLog("hiding to look at the screen behind the corners");
		backdrop = _Capture(workspace, frame, screenFrame);
		if (backdrop == NULL) {
			for (int i = 0; i < kCornerCount; i++)
				delete tracked[i];
			return;
		}
	}

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
		const bool left = i == kTopLeft || i == kBottomLeft;
		const bool top = i == kTopLeft || i == kTopRight;
		const float r = kRadius - 1;
		const BRect place(left ? 0 : bounds.right - r, top ? 0 : bounds.bottom - r, left ? r : bounds.right,
			top ? r : bounds.bottom);
		const uint32 resizing = B_FOLLOW_LEFT | B_FOLLOW_TOP;	// (placed again whenever the window changes)
		if (!redo[i]) {
			fCorners[i]->MoveTo(place.LeftTop());
			continue;
		}
		BBitmap* picture = NULL;
		if (tracked[i] != NULL)
			picture = _Picture(i, rects[i], tracked[i], rects[i], line);
		else if (fromAhead[i])
			picture = _Picture(i, rects[i], fAhead[i], fAheadRects[i], line);
		else {
			const int strip = _Strip(backdrop, rects[i]);
			if (strip >= 0)
				picture = _Picture(i, rects[i], backdrop->strips[strip], backdrop->rects[strip], line);
		}
		delete tracked[i];
		tracked[i] = NULL;
		if (picture == NULL)
			continue;
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
	_DropAhead();
	fWorkspace = workspace;
}
