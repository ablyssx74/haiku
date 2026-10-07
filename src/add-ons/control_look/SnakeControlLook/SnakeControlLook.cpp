/*
 * A control look that gives every menu the "snake trail" look of hDesktop: a rounded, beveled
 * selector in an accent colour that runs unbroken from the root menu through every open submenu.
 * Everything else is the stock Haiku look.
 *
 * It works through the control look, so it applies to every program's menus (including tray
 * replicants and programs installed later) without any change to them. Select it in Appearance, or
 * with set_control_look().
 *
 * The accent colour is hDesktop's Selector Color while hDesktop is running (and its Snake Trail
 * switch is honoured); otherwise Tracker's "SnakeAccent" / "SnakeTrail" preferences; otherwise blue.
 *
 * Every menu level is its own window, so each level draws its own piece of the trail and the pieces
 * meet at the shared edge: a row extends to the edge facing its submenu, the submenu carries an
 * "elbow bar" down that edge to its own row, and concave fillets join the two. The selector is
 * rasterised into a bitmap: rounded rectangles (each corner with its own radius), the bar and the
 * fillets are unioned in a coverage map, and the light top edge and dark bottom edge fall out of
 * comparing the map with copies of itself shifted a pixel down and up.
 */


#include <ControlLook.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <algorithm>
#include <map>
#include <new>
#include <vector>

#include <Autolock.h>
#include <Bitmap.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Locker.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <Message.h>
#include <Path.h>
#include <Roster.h>
#include <View.h>
#include <Window.h>

#include "HaikuControlLook.h"


namespace BPrivate {

static const float kBarW = 7.0f;		// the elbow bar down a submenu's edge
static const float kSelR = 4.0f;		// corner radius of a selector row
static const float kBarR = 3.0f;
static const float kFilletR = 3.0f;
static const bigtime_t kCheckInterval = 1000000;
static const char* kHDesktopSignature = "application/x-vnd.hdesktop";


// #pragma mark - settings


// Everything this add-on keeps globally is allocated on the heap and never freed: a global with a
// destructor registers it to run at program exit, and by then the add-on has been unloaded (libbe
// unloads it when the program shuts down), so exit() would jump into memory that is gone. That crashed
// mount_server as the system shut down.
static BLocker&
SettingsLock()
{
	static BLocker* lock = new BLocker("snake control look settings");
	return *lock;
}

static rgb_color sAccent = {70, 110, 200, 255};
static bool sTrail = true;
static bigtime_t sLastCheck = -kCheckInterval;


// Tracker's settings file is plain text, one "Name value" per line.
static void
ReadTrackerSettings(rgb_color& accent, bool& trail)
{
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK
		|| path.Append("Tracker/TrackerSettings") != B_OK) {
		return;
	}
	FILE* file = fopen(path.Path(), "r");
	if (file == NULL)
		return;
	char line[256];
	while (fgets(line, sizeof(line), file) != NULL) {
		if (strncmp(line, "SnakeAccent ", 12) == 0) {
			unsigned long value = strtoul(line + 12, NULL, 0);
			accent = make_color((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF);
		} else if (strncmp(line, "SnakeTrail ", 11) == 0) {
			trail = strncmp(line + 11, "off", 3) != 0 && strncmp(line + 11, "0", 1) != 0
				&& strncmp(line + 11, "false", 5) != 0;
		}
	}
	fclose(file);
}


static void
RefreshSettings()
{
	bigtime_t now = system_time();
	BAutolock lock(SettingsLock());
	if (now - sLastCheck < kCheckInterval)
		return;
	sLastCheck = now;

	rgb_color accent = make_color(70, 110, 200);
	bool trail = true;
	ReadTrackerSettings(accent, trail);

	BPath path;
	if (be_roster != NULL && be_roster->IsRunning(kHDesktopSignature)
		&& find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK
		&& path.Append("hdesktop_settings") == B_OK) {
		BFile file(path.Path(), B_READ_ONLY);
		BMessage settings;
		if (file.InitCheck() == B_OK && settings.Unflatten(&file) == B_OK) {
			int32 packed;
			if (settings.FindInt32("nav_accent", &packed) == B_OK) {
				accent = make_color((packed >> 16) & 0xFF, (packed >> 8) & 0xFF,
					packed & 0xFF);
			}
			bool value;
			if (settings.FindBool("nav_snake_trail", &value) == B_OK)
				trail = value;
		}
	}
	sAccent = accent;
	sTrail = trail;
}


static rgb_color
Accent()
{
	RefreshSettings();
	BAutolock lock(SettingsLock());
	return sAccent;
}


static bool
TrailEnabled()
{
	RefreshSettings();
	BAutolock lock(SettingsLock());
	return sTrail;
}


// #pragma mark - colours and shapes


static rgb_color
Mix(rgb_color c, float toward, float amount)
{
	return make_color((uint8)(c.red + (toward - c.red) * amount),
		(uint8)(c.green + (toward - c.green) * amount),
		(uint8)(c.blue + (toward - c.blue) * amount));
}


static rgb_color Light(rgb_color c) { return Mix(c, 255, 0.35f); }
static rgb_color Dark(rgb_color c) { return Mix(c, 0, 0.38f); }


// White on dark accents, near-black on light ones.
static rgb_color
TextOn(rgb_color c)
{
	float lum = (0.2126f * c.red + 0.7152f * c.green + 0.0722f * c.blue) / 255.0f;
	return lum > 0.62f ? make_color(20, 22, 28) : make_color(255, 255, 255);
}


static inline float
Clamp01(float v)
{
	return v < 0 ? 0 : (v > 1 ? 1 : v);
}


// Coverage (0..1, anti-aliased) of a rounded rectangle at a pixel centre; each corner has its own radius.
static float
RoundRectCoverage(float px, float py, float x, float y, float w, float h, float tl, float tr,
	float br, float bl)
{
	float hx = w / 2, hy = h / 2;
	float lx = px - (x + hx), ly = py - (y + hy);
	float r = (lx > 0) ? ((ly > 0) ? br : tr) : ((ly > 0) ? bl : tl);
	float qx = fabsf(lx) - hx + r, qy = fabsf(ly) - hy + r;
	float outside = sqrtf(std::max(qx, 0.0f) * std::max(qx, 0.0f)
		+ std::max(qy, 0.0f) * std::max(qy, 0.0f));
	float d = std::min(std::max(qx, qy), 0.0f) + outside - r;
	return Clamp01(0.5f - d);
}


// The concave "fillet" where the elbow bar meets a row: a square at the corner point (cx, cy),
// extending toward (dx, dy), minus the circle that rounds it.
static float
FilletCoverage(float px, float py, float cx, float cy, int dx, int dy, float r)
{
	float centreX = cx + dx * r, centreY = cy + dy * r;
	float lx = px - (cx + dx * r / 2), ly = py - (cy + dy * r / 2);
	float qx = fabsf(lx) - r / 2, qy = fabsf(ly) - r / 2;
	float outside = sqrtf(std::max(qx, 0.0f) * std::max(qx, 0.0f)
		+ std::max(qy, 0.0f) * std::max(qy, 0.0f));
	float dSquare = std::min(std::max(qx, qy), 0.0f) + outside;
	float dCircle = r - sqrtf((px - centreX) * (px - centreX) + (py - centreY) * (py - centreY));
	return Clamp01(0.5f - std::max(dSquare, dCircle));
}


struct Piece {
	float x, y, w, h, tl, tr, br, bl;
};

struct Fillet {
	float x, y;
	int dx, dy;
};


// BMenuItem::IsSelected() is protected; a derived class that adds no data can read it from a plain
// BMenuItem (the cast only changes which member functions may be called, not the object).
struct ItemAccess : public BMenuItem {
	ItemAccess() : BMenuItem((const char*)NULL, (BMessage*)NULL) {}

	bool Selected() const { return IsSelected(); }
};


static BMenuItem*
SelectedItem(BMenu* menu)
{
	for (int32 i = 0; i < menu->CountItems(); i++) {
		BMenuItem* item = menu->ItemAt(i);
		if (static_cast<const ItemAccess*>(item)->Selected())
			return item;
	}
	return NULL;
}


// What a submenu knows about the menu it was opened from, refreshed whenever the parent's window can
// be read without waiting (the parent window is locked by the tracking thread, which may be waiting
// for this one, so only a try-lock is safe).
struct ParentLink {
	ParentLink() : valid(false), rowTop(0), rowBottom(0), windowLeft(0) {}

	bool	valid;
	float	rowTop;			// the parent's open row, in screen coordinates
	float	rowBottom;
	float	windowLeft;		// the parent window's left edge, in screen coordinates
};

static BLocker&
LinkLock()
{
	static BLocker* lock = new BLocker("snake links");
	return *lock;
}


static std::map<BMenu*, ParentLink>&
Links()
{
	static std::map<BMenu*, ParentLink>* links = new std::map<BMenu*, ParentLink>();
	return *links;
}


static ParentLink
FindParentLink(BMenu* menu)
{
	BMenu* parent = menu->Supermenu();
	BMenuItem* item = menu->Superitem();
	ParentLink link;
	if (parent == NULL || item == NULL || parent->Window() == NULL || dynamic_cast<BMenuBar*>(parent) != NULL) {
		BAutolock lock(LinkLock());
		Links().erase(menu);
		return link;
	}

	BWindow* parentWindow = parent->Window();
	if (parentWindow->LockWithTimeout(0) == B_OK) {
		BRect row = parent->ConvertToScreen(item->Frame());
		link.valid = true;
		link.rowTop = row.top;
		link.rowBottom = row.bottom + 1;
		link.windowLeft = parentWindow->Frame().left;

		bool isNew;
		{
			BAutolock lock(LinkLock());
			std::map<BMenu*, ParentLink>::iterator it = Links().find(menu);
			isNew = it == Links().end() || !it->second.valid || it->second.rowTop != link.rowTop;
			Links()[menu] = link;
		}
		// the parent's row gets redrawn with the edge that joins this submenu
		if (isNew)
			parent->Invalidate();
		parentWindow->Unlock();
		return link;
	}

	BAutolock lock(LinkLock());
	std::map<BMenu*, ParentLink>::iterator it = Links().find(menu);
	if (it != Links().end())
		link = it->second;
	return link;
}


// What the trail drawn last time depended on. When it changes while only part of the menu is being
// redrawn (stock items only invalidate their own rows), the whole menu is invalidated so the bar and
// fillets between rows are repainted too.
struct DrawState {
	DrawState() : selected(NULL), open(false), parentRowTop(0), parentOnLeft(false) {}

	bool operator!=(const DrawState& o) const
	{
		return selected != o.selected || open != o.open || parentRowTop != o.parentRowTop
			|| parentOnLeft != o.parentOnLeft;
	}

	BMenuItem*	selected;
	bool		open;
	float		parentRowTop;
	bool		parentOnLeft;
};

static std::map<BMenu*, DrawState>&
States()
{
	static std::map<BMenu*, DrawState>* states = new std::map<BMenu*, DrawState>();
	return *states;
}


static void
DrawTrail(BMenu* menu, const BRect& updateRect)
{
	BRect bounds = menu->Bounds();
	const int w = (int)bounds.Width() + 1, h = (int)bounds.Height() + 1;
	if (w < 8 || h < 8 || menu->Window() == NULL)
		return;
	const float vt = bounds.top;	// view y of the bitmap's first row

	const bool trailOn = TrailEnabled();
	BMenuItem* selected = SelectedItem(menu);
	const bool active = selected != NULL && (selected->IsEnabled() || selected->Submenu() != NULL);
	BMenu* child = active ? selected->Submenu() : NULL;
	const bool open = child != NULL && child->Window() != NULL && trailOn;
	const float myLeft = menu->Window()->Frame().left;
	const bool childOnRight = open && child->Window()->Frame().left > myLeft;

	ParentLink link;
	if (trailOn)
		link = FindParentLink(menu);
	const bool parentOnLeft = link.valid && link.windowLeft < myLeft;

	{
		DrawState state;
		state.selected = selected;
		state.open = open;
		state.parentRowTop = link.valid ? link.rowTop : 0;
		state.parentOnLeft = parentOnLeft;
		bool changed;
		{
			BAutolock lock(LinkLock());
			changed = States()[menu] != state;
			States()[menu] = state;
		}
		if (changed && !updateRect.Contains(bounds))
			menu->Invalidate();
	}

	std::vector<Piece> pieces;
	std::vector<Fillet> fillets;

	float pTop = 0, pBottom = 0;
	if (link.valid) {
		pTop = menu->ConvertFromScreen(BPoint(0, link.rowTop)).y - vt + 1;
		pBottom = menu->ConvertFromScreen(BPoint(0, link.rowBottom)).y - vt - 1;
	}

	float ownTop = 0, ownBottom = 0;
	bool hasOwn = false;
	if (active) {
		BRect f = selected->Frame();
		float top = f.top - vt + 1, bottom = f.bottom + 1 - vt - 1;
		ownTop = top;
		ownBottom = bottom;
		hasOwn = true;

		const bool onTrail = link.valid;
		const bool childEdge = open;
		bool cutTop = top < 0, cutBottom = bottom > h;
		float t = std::max(top, 0.0f), b = std::min(bottom, (float)h);
		if (b > t) {
			bool leftFlush = (childEdge && !childOnRight) || (onTrail && parentOnLeft);
			bool rightFlush = (childEdge && childOnRight) || (onTrail && !parentOnLeft);
			float x0 = leftFlush ? 0.0f : 3.0f, x1 = rightFlush ? (float)w : w - 3.0f;
			bool expTop = onTrail && top < pTop - 0.5f, expBottom = onTrail && bottom > pBottom + 0.5f;
			float tl = cutTop ? 0.0f
				: ((!leftFlush || (onTrail && parentOnLeft && expTop)) ? kSelR : 0.0f);
			float tr = cutTop ? 0.0f
				: ((!rightFlush || (onTrail && !parentOnLeft && expTop)) ? kSelR : 0.0f);
			float br = cutBottom ? 0.0f
				: ((!rightFlush || (onTrail && !parentOnLeft && expBottom)) ? kSelR : 0.0f);
			float bl = cutBottom ? 0.0f
				: ((!leftFlush || (onTrail && parentOnLeft && expBottom)) ? kSelR : 0.0f);
			Piece p = {x0, t, x1 - x0, b - t, tl, tr, br, bl};
			pieces.push_back(p);
		}
	}

	// the elbow bar down the edge that faces the parent, from the parent's row to our own
	if (link.valid) {
		float top = pTop, bottom = pBottom;
		if (hasOwn && ownBottom > ownTop) {
			top = std::min(top, ownTop);
			bottom = std::max(bottom, ownBottom);
		}
		top = std::max(top, 0.0f);
		bottom = std::min(bottom, (float)h);
		if (bottom > top) {
			float rt = top < pTop - 0.5f ? kSelR : 0.0f, rb = bottom > pBottom + 0.5f ? kSelR : 0.0f;
			Piece p;
			if (parentOnLeft) {
				Piece q = {0, top, kBarW, bottom - top, rt, kBarR, kBarR, rb};
				p = q;
			} else {
				Piece q = {(float)w - kBarW, top, kBarW, bottom - top, kBarR, rt, rb, kBarR};
				p = q;
			}
			pieces.push_back(p);
			if (hasOwn && ownBottom > ownTop) {
				float ex = parentOnLeft ? kBarW : w - kBarW;
				int dx = parentOnLeft ? 1 : -1;
				if (top < ownTop - 0.5f) {
					Fillet fl = {ex, ownTop, dx, -1};
					fillets.push_back(fl);
				}
				if (bottom > ownBottom + 0.5f) {
					Fillet fl = {ex, ownBottom, dx, 1};
					fillets.push_back(fl);
				}
			}
		}
	}

	if (pieces.empty())
		return;

	std::vector<float> cover((size_t)w * h, 0.0f);
	for (size_t k = 0; k < pieces.size(); k++) {
		const Piece& p = pieces[k];
		int x0 = std::max(0, (int)floorf(p.x) - 1), x1 = std::min(w - 1, (int)ceilf(p.x + p.w) + 1);
		int y0 = std::max(0, (int)floorf(p.y) - 1), y1 = std::min(h - 1, (int)ceilf(p.y + p.h) + 1);
		for (int y = y0; y <= y1; ++y) {
			for (int x = x0; x <= x1; ++x) {
				float c = RoundRectCoverage(x + 0.5f, y + 0.5f, p.x, p.y, p.w, p.h, p.tl, p.tr,
					p.br, p.bl);
				float& dst = cover[(size_t)y * w + x];
				dst = std::max(dst, c);
			}
		}
	}
	for (size_t k = 0; k < fillets.size(); k++) {
		const Fillet& f = fillets[k];
		int x0 = std::max(0, (int)floorf(std::min(f.x, f.x + f.dx * kFilletR)) - 1);
		int x1 = std::min(w - 1, (int)ceilf(std::max(f.x, f.x + f.dx * kFilletR)) + 1);
		int y0 = std::max(0, (int)floorf(std::min(f.y, f.y + f.dy * kFilletR)) - 1);
		int y1 = std::min(h - 1, (int)ceilf(std::max(f.y, f.y + f.dy * kFilletR)) + 1);
		for (int y = y0; y <= y1; ++y) {
			for (int x = x0; x <= x1; ++x) {
				float c = FilletCoverage(x + 0.5f, y + 0.5f, f.x, f.y, f.dx, f.dy, kFilletR);
				float& dst = cover[(size_t)y * w + x];
				dst = std::max(dst, c);
			}
		}
	}

	// composite with the bevel: light where the pixel above is empty, dark where the one below is
	BBitmap bitmap(BRect(0, 0, w - 1, h - 1), B_RGBA32);
	if (bitmap.InitCheck() != B_OK)
		return;
	const rgb_color base = Accent(), light = Light(base), dark = Dark(base);
	uint8* bits = (uint8*)bitmap.Bits();
	const int32 bpr = bitmap.BytesPerRow();
	for (int y = 0; y < h; ++y) {
		uint8* row = bits + y * bpr;
		for (int x = 0; x < w; ++x) {
			float c0 = cover[(size_t)y * w + x];
			uint8* px = row + x * 4;
			if (c0 <= 0.0f) {
				px[0] = px[1] = px[2] = px[3] = 0;
				continue;
			}
			float above = y > 0 ? cover[(size_t)(y - 1) * w + x] : 0.0f;
			float below = y < h - 1 ? cover[(size_t)(y + 1) * w + x] : 0.0f;
			float a2 = c0 * above, a3 = a2 * below;
			float cr = light.red, cg = light.green, cb = light.blue;
			cr += (dark.red - cr) * a2;  cg += (dark.green - cg) * a2;  cb += (dark.blue - cb) * a2;
			cr += (base.red - cr) * a3;  cg += (base.green - cg) * a3;  cb += (base.blue - cb) * a3;
			px[0] = (uint8)lroundf(std::min(255.0f, cb));
			px[1] = (uint8)lroundf(std::min(255.0f, cg));
			px[2] = (uint8)lroundf(std::min(255.0f, cr));
			px[3] = (uint8)lroundf(c0 * 255.0f);
		}
	}

	menu->PushState();
	menu->SetDrawingMode(B_OP_ALPHA);
	menu->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	menu->DrawBitmap(&bitmap, bounds.LeftTop());
	menu->PopState();
}


// A selector for a single row (menu bar titles).
static void
DrawLoneSelector(BView* view, BRect frame)
{
	rgb_color accent = Accent();
	BRect r = frame.InsetByCopy(1, 1);

	view->PushState();
	view->SetDrawingMode(B_OP_ALPHA);
	view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	view->SetHighColor(Light(accent));
	view->FillRoundRect(r.OffsetByCopy(0, -1), kSelR, kSelR);
	view->SetHighColor(Dark(accent));
	view->FillRoundRect(r.OffsetByCopy(0, 1), kSelR, kSelR);
	view->SetHighColor(accent);
	view->FillRoundRect(r, kSelR, kSelR);
	view->PopState();
}


// #pragma mark - the control look


class SnakeControlLook : public HaikuControlLook {
public:
	SnakeControlLook() {}
	virtual ~SnakeControlLook() {}

	virtual	void DrawMenuBackground(BView* view, BRect& rect, const BRect& updateRect,
		const rgb_color& base, uint32 flags = 0, uint32 borders = B_ALL_BORDERS)
	{
		HaikuControlLook::DrawMenuBackground(view, rect, updateRect, base, flags, borders);

		BMenu* menu = dynamic_cast<BMenu*>(view);
		if (menu != NULL && dynamic_cast<BMenuBar*>(menu) == NULL)
			DrawTrail(menu, updateRect);
	}

	virtual	void DrawMenuItemBackground(BView* view, BRect& rect, const BRect& updateRect,
		const rgb_color& base, uint32 flags = 0, uint32 borders = B_ALL_BORDERS)
	{
		if ((flags & B_ACTIVATED) == 0) {
			HaikuControlLook::DrawMenuItemBackground(view, rect, updateRect, base, flags, borders);
			return;
		}

		// The selector was drawn under the items by DrawMenuBackground(); the item only needs the
		// right text colour. Menu bar titles get a rounded selector of their own.
		BMenu* menu = dynamic_cast<BMenu*>(view);
		if (menu == NULL || dynamic_cast<BMenuBar*>(menu) != NULL) {
			if (view->Bounds().IsValid() && ShouldDraw(view, rect, updateRect))
				DrawLoneSelector(view, rect);
		}
		view->SetLowColor(Accent());
		view->SetHighColor(TextOn(Accent()));
	}
};

}	// namespace BPrivate


extern "C" BControlLook*
instantiate_control_look(image_id id)
{
	return new (std::nothrow) BPrivate::SnakeControlLook();
}
