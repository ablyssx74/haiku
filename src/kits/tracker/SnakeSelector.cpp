/*
 * See SnakeSelector.h. The selector is rasterised into a bitmap: rounded rectangles (every corner
 * with its own radius), the elbow bar and its fillets are unioned in a coverage map, and the light top
 * edge and dark bottom edge fall out of comparing the map with copies of itself shifted a pixel down
 * and up. That keeps the shape the same however the pieces overlap, anti-aliased.
 */


#include "SnakeSelector.h"

#include <algorithm>
#include <math.h>
#include <vector>

#include <Bitmap.h>
#include <Autolock.h>
#include <Locker.h>
#include <Menu.h>
#include <map>
#include <MenuItem.h>
#include <View.h>
#include <Window.h>

#include "NavMenu.h"


namespace SnakeSelector {

static const float kBarW = 7.0f;		// the elbow bar down a submenu's edge
static const float kSelR = 4.0f;		// corner radius of a selector row
static const float kBarR = 3.0f;
static const float kFilletR = 3.0f;


rgb_color
Accent()
{
	// TODO: follow hDesktop's Selector Color when it is running.
	return make_color(70, 110, 200);
}


static rgb_color
Mix(rgb_color c, float toward, float amount)
{
	return make_color((uint8)(c.red + (toward - c.red) * amount),
		(uint8)(c.green + (toward - c.green) * amount),
		(uint8)(c.blue + (toward - c.blue) * amount));
}


rgb_color
Light(rgb_color c)
{
	return Mix(c, 255, 0.35f);
}


rgb_color
Dark(rgb_color c)
{
	return Mix(c, 0, 0.38f);
}


// White on dark accents, near-black on light ones.
rgb_color
TextOn(rgb_color c)
{
	float lum = (0.2126f * c.red + 0.7152f * c.green + 0.0722f * c.blue) / 255.0f;
	return lum > 0.62f ? make_color(20, 22, 28) : make_color(255, 255, 255);
}


bool
MenuDrawsTrail(BMenu* menu)
{
	return dynamic_cast<BNavMenu*>(menu) != NULL;
}


void
DrawLoneSelector(BView* view, BRect frame)
{
	rgb_color accent = Accent();
	BRect r = frame.InsetByCopy(2, 1);

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


// What a submenu knows about the menu it was opened from. Kept here, not in BNavMenu, so BNavMenu's
// size stays the same for programs built against the old header.
struct ParentLink {
	ParentLink() : valid(false), rowTop(0), rowBottom(0), windowLeft(0) {}

	bool	valid;
	float	rowTop;			// the parent's open row, in screen coordinates
	float	rowBottom;
	float	windowLeft;		// the parent window's left edge, in screen coordinates
};

static BLocker sLinkLock("snake links");
static std::map<BMenu*, ParentLink> sLinks;


static void
InvalidateParent(BMenu* parent)
{
	// only when this thread holds the parent's window (it does while the tracking thread opens or
	// closes a submenu); otherwise the next selection change redraws it anyway
	if (parent != NULL && parent->Window() != NULL && parent->Window()->IsLocked())
		parent->Invalidate();
}


void
AttachLink(BMenu* submenu)
{
	BMenu* parent = submenu->Supermenu();
	BMenuItem* item = submenu->Superitem();
	ParentLink link;
	if (parent != NULL && item != NULL && parent->Window() != NULL && MenuDrawsTrail(parent)
		&& parent->Window()->IsLocked()) {
		BRect row = parent->ConvertToScreen(item->Frame());
		link.valid = true;
		link.rowTop = row.top;
		link.rowBottom = row.bottom + 1;
		link.windowLeft = parent->Window()->Frame().left;
	}
	{
		BAutolock lock(sLinkLock);
		sLinks[submenu] = link;
	}
	InvalidateParent(parent);
}


void
DetachLink(BMenu* submenu)
{
	{
		BAutolock lock(sLinkLock);
		sLinks.erase(submenu);
	}
	InvalidateParent(submenu->Supermenu());
}


void
DrawTrail(BMenu* menu)
{
	ParentLink link;
	{
		BAutolock lock(sLinkLock);
		std::map<BMenu*, ParentLink>::iterator it = sLinks.find(menu);
		if (it != sLinks.end())
			link = it->second;
	}

	BRect bounds = menu->Bounds();
	const int w = (int)bounds.Width() + 1, h = (int)bounds.Height() + 1;
	if (w < 8 || h < 8 || menu->Window() == NULL)
		return;
	const float vt = bounds.top;	// view y of the bitmap's first row

	// the selected row and whether its submenu is open
	BMenuItem* selected = NULL;
	for (int32 i = 0; i < menu->CountItems(); i++) {
		BMenuItem* item = menu->ItemAt(i);
		if (item->IsSelected()) {
			selected = item;
			break;
		}
	}
	const bool active = selected != NULL && (selected->IsEnabled() || selected->Submenu() != NULL);
	BMenu* child = active ? selected->Submenu() : NULL;
	const bool open = child != NULL && child->Window() != NULL;
	const float myLeft = menu->Window()->Frame().left;
	const bool childOnRight = open && child->Window()->Frame().left > myLeft;
	const bool parentOnLeft = link.valid && link.windowLeft < myLeft;

	std::vector<Piece> pieces;
	std::vector<Fillet> fillets;

	// the parent's open row, in this menu's bitmap coordinates
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
			auto cornerR = [&](bool flush, bool exposed) { return (!flush || exposed) ? kSelR : 0.0f; };
			float tl = cutTop ? 0.0f : cornerR(leftFlush, onTrail && parentOnLeft && expTop);
			float tr = cutTop ? 0.0f : cornerR(rightFlush, onTrail && !parentOnLeft && expTop);
			float br = cutBottom ? 0.0f : cornerR(rightFlush, onTrail && !parentOnLeft && expBottom);
			float bl = cutBottom ? 0.0f : cornerR(leftFlush, onTrail && parentOnLeft && expBottom);
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

	// union coverage map
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
	auto at = [&](int x, int y) -> float {
		return (x < 0 || y < 0 || x >= w || y >= h) ? 0.0f : cover[(size_t)y * w + x];
	};
	for (int y = 0; y < h; ++y) {
		uint8* row = bits + y * bpr;
		for (int x = 0; x < w; ++x) {
			float c0 = at(x, y);
			uint8* px = row + x * 4;
			if (c0 <= 0.0f) {
				px[0] = px[1] = px[2] = px[3] = 0;
				continue;
			}
			float a2 = c0 * at(x, y - 1), a3 = a2 * at(x, y + 1);
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

}	// namespace SnakeSelector
