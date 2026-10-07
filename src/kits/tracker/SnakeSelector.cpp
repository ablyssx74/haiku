/*
 * See SnakeSelector.h. The selector is rasterised into a bitmap: rounded rectangles (every corner
 * with its own radius), the elbow bar and its fillets are unioned in a coverage map, and the light top
 * edge and dark bottom edge fall out of comparing the map with copies of itself shifted a pixel down
 * and up. That keeps the shape the same however the pieces overlap, anti-aliased.
 */


#include "SnakeSelector.h"

#include <algorithm>
#include <math.h>
#include <typeinfo>
#include <vector>

#include <Bitmap.h>
#include <InterfaceDefs.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>
#include <Roster.h>
#include <stdio.h>
#include <string.h>
#include <typeinfo>

#include <ControlLook.h>
#include <time.h>
#include <Autolock.h>
#include <Locker.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <Menu.h>
#include <map>
#include <MenuItem.h>
#include <View.h>
#include <Window.h>

#include "LiveMenu.h"
#include "MountMenu.h"
#include "NavMenu.h"
#include "SlowMenu.h"
#include "SnakeMenuItem.h"
#include "TemplatesMenu.h"
#include "TrackerSettings.h"


using namespace BPrivate;


namespace SnakeSelector {

static const float kBarW = 7.0f;		// the elbow bar down a submenu's edge
static const float kSelR = 4.0f;		// corner radius of a selector row
static const float kBarR = 3.0f;
static const float kFilletR = 3.0f;
static const bool kRoundedFrame = false;	// see the comment in DrawTrail()


// The selector colour and whether the trail is drawn: Tracker's own preferences (Settings > Windows),
// or hDesktop's Selector Color and Snake Trail, read from its settings file (a flattened BMessage)
// while hDesktop is running.
static const char* kHDesktopSignature = "application/x-vnd.hdesktop";
static const bigtime_t kCheckInterval = 1000000;

// Heap-allocated and never freed, so nothing here runs at program exit (see SnakeControlLook.cpp).
static BLocker&
SettingsLock()
{
	static BLocker* lock = new BLocker("snake settings");
	return *lock;
}

static rgb_color sAccent = {70, 110, 200, 255};
static bool sTrail = true;
static bigtime_t sLastCheck = 0;


static void
RefreshSettings()
{
	bigtime_t now = system_time();
	BAutolock lock(SettingsLock());
	if (now - sLastCheck < kCheckInterval)
		return;
	sLastCheck = now;

	// Tracker's own preferences (Settings > Windows); overridden by hDesktop's while it runs
	TrackerSettings tracker;
	rgb_color accent = tracker.SnakeAccent();
	bool trail = tracker.SnakeTrail();
	time_t modTime = 0;

	BPath path;
	if (be_roster != NULL && be_roster->IsRunning(kHDesktopSignature)
		&& find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK
		&& path.Append("hdesktop_settings") == B_OK) {
		BEntry entry(path.Path());
		if (entry.GetModificationTime(&modTime) != B_OK)
			modTime = 0;
		if (modTime != 0) {
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
	}

	sAccent = accent;
	sTrail = trail;
}


rgb_color
Accent()
{
	RefreshSettings();
	BAutolock lock(SettingsLock());
	return sAccent;
}


bool
TrailEnabled()
{
	RefreshSettings();
	BAutolock lock(SettingsLock());
	return sTrail;
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


// Tracker's own menu classes draw the trail from their DrawBackground(); stock menus (menu bars and
// the like) don't. The menu must also hold nothing but snake items (and separators): a stock item
// would get no highlight at all, as the trail menu leaves the selector to its items.
bool
MenuDrawsTrail(BMenu* menu)
{
	if (menu == NULL)
		return false;
	if (dynamic_cast<BSlowMenu*>(menu) == NULL && dynamic_cast<TLiveMenu*>(menu) == NULL
		&& dynamic_cast<TLivePopUpMenu*>(menu) == NULL && dynamic_cast<TemplatesMenu*>(menu) == NULL
		&& dynamic_cast<MountMenu*>(menu) == NULL && dynamic_cast<SnakeMenu*>(menu) == NULL
		&& dynamic_cast<SnakePopUpMenu*>(menu) == NULL) {
		return false;
	}

	for (int32 i = 0; i < menu->CountItems(); i++) {
		BMenuItem* item = menu->ItemAt(i);
		if (dynamic_cast<SnakeMenuItem*>(item) == NULL && dynamic_cast<BSeparatorItem*>(item) == NULL)
			return false;
	}
	return true;
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


// (plain functions rather than lambdas: Haiku's old gcc 2 cannot compile lambdas)
static float
CornerRadius(bool flush, bool exposed)
{
	return (!flush || exposed) ? kSelR : 0.0f;
}


static float
CoverAt(const std::vector<float>& cover, int w, int h, int x, int y)
{
	return (x < 0 || y < 0 || x >= w || y >= h) ? 0.0f : cover[(size_t)y * w + x];
}


static BLocker& LinkLock();


// A one pixel column of the window system's border runs between two menu windows that sit side by side, and
// shows as a dark seam through the joined selector. It can't be painted over from inside either window, so a
// borderless window one pixel wide is put over it, in the selector's colours, for as long as the submenu is
// shown.
class SeamBridge;
static void ForgetBridge(SeamBridge* bridge);


class SeamBridge : public BWindow {
public:
	SeamBridge(BWindow* child)
		:
		BWindow(BRect(0, 0, 0, 7), "seam", B_NO_BORDER_WINDOW_LOOK, (window_feel)1025,
			B_NOT_MOVABLE | B_NOT_CLOSABLE | B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE | B_NOT_RESIZABLE
				| B_AVOID_FOCUS),
		fChild(child),
		fRunner(NULL),
		fAccent(SnakeSelector::Accent()),
		fHeight(8),
		fLeft(-1),
		fTop(-1)
	{
		SetSizeLimits(0, 0, 0, 4000);
		BView* view = new SeamView(this);
		AddChild(view);
		BMessage tick('Tick');
		fRunner = new BMessageRunner(BMessenger(this), &tick, 100000);
	}

	~SeamBridge()
	{
		delete fRunner;
	}

	// from the submenu's thread: where the seam is. The submenu redraws on every hover change, so nothing is
	// touched unless the seam actually moved or changed colour (a needless resize or redraw flickers).
	void Place(BRect strip, rgb_color accent)
	{
		if (LockWithTimeout(20000) != B_OK)
			return;		// busy; the next draw places it
		const int left = (int)strip.left, top = (int)strip.top, height = (int)strip.Height() + 1;
		bool same = !IsHidden() && left == fLeft && top == fTop && height == fHeight
			&& accent.red == fAccent.red && accent.green == fAccent.green
			&& accent.blue == fAccent.blue;
		if (!same) {
			fAccent = accent;
			fHeight = height;
			fLeft = left;
			fTop = top;
			ResizeTo(0, height - 1);
			MoveTo(left, top);
			if (IsHidden())
				Show();
			ChildAt(0)->Invalidate();
		}
		Unlock();
	}

	virtual void MessageReceived(BMessage* message)
	{
		if (message->what == 'Tick' || message->what == 'Hide') {
			// Hide when the submenu's window is, and quit when it is gone for good. The window is kept
			// otherwise: the menu windows are reused, and the same bridge is shown again with the next
			// submenu. Never wait for the submenu's window: its thread may be waiting for this one (Place()),
			// and two threads each waiting for the other froze Tracker. If it is busy, ask again next tick.
			bool hide = message->what == 'Hide';
			bool gone = false;
			status_t status = fChild.LockTargetWithTimeout(0);
			if (status == B_OK) {
				BLooper* looper = NULL;
				fChild.Target(&looper);
				BWindow* window = dynamic_cast<BWindow*>(looper);
				if (window == NULL)
					gone = true;
				else if (window->IsHidden())
					hide = true;
				else {
					// still beside the submenu? (it may have moved since the bridge was placed)
					BRect frame = window->Frame();
					BRect mine = Frame();
					bool beside = (mine.left == frame.left - 1 || mine.left == frame.right + 1)
						&& mine.top >= frame.top - 2 && mine.bottom <= frame.bottom + 2;
					if (!beside)
						hide = true;
				}
				if (looper != NULL)
					looper->Unlock();
			} else if (status == B_TIMED_OUT || status == B_WOULD_BLOCK)
				gone = false;	// busy (a zero timeout reports B_WOULD_BLOCK): ask again next tick
			else
				gone = !fChild.IsValid();

			if (gone) {
				ForgetBridge(this);
				PostMessage(B_QUIT_REQUESTED);
			} else if (hide && !IsHidden())
				Hide();
			return;
		}
		BWindow::MessageReceived(message);
	}

	rgb_color Accent() const { return fAccent; }
	int Height() const { return fHeight; }

private:
	class SeamView : public BView {
	public:
		SeamView(SeamBridge* bridge)
			:
			BView(BRect(0, 0, 0, 7), "seam", B_FOLLOW_ALL, B_WILL_DRAW),
			fBridge(bridge)
		{
			SetViewColor(B_TRANSPARENT_COLOR);
		}

		virtual void Draw(BRect)
		{
			rgb_color accent = fBridge->Accent();
			int h = fBridge->Height();
			for (int y = 0; y < h; y++) {
				if (y == 0)
					SetHighColor(SnakeSelector::Light(accent));
				else if (y == h - 1)
					SetHighColor(SnakeSelector::Dark(accent));
				else
					SetHighColor(accent);
				FillRect(BRect(0, y, 0, y));
			}
		}

	private:
		SeamBridge*	fBridge;
	};

	BMessenger		fChild;
	BMessageRunner*	fRunner;
	rgb_color		fAccent;
	int				fHeight;
	int				fLeft;
	int				fTop;
};


static std::map<BWindow*, SeamBridge*>&
Bridges()
{
	static std::map<BWindow*, SeamBridge*>* bridges = new std::map<BWindow*, SeamBridge*>();
	return *bridges;
}


static void
ForgetBridge(SeamBridge* bridge)
{
	BAutolock lock(LinkLock());
	std::map<BWindow*, SeamBridge*>::iterator it = Bridges().begin();
	while (it != Bridges().end()) {
		if (it->second == bridge)
			Bridges().erase(it++);
		else
			++it;
	}
}


// Opt-in diagnostics: if the file ~/config/settings/snake_debug exists, bridge placements are appended to
// ~/config/settings/snake_debug.log (to find out where a stray seam line comes from on real hardware).
static void
SeamLog(const char* what, BWindow* child, BRect strip, float parentTop, float parentBottom)
{
	static bigtime_t lastCheck = 0;
	static bool enabled = false;
	bigtime_t now = system_time();
	if (now - lastCheck > 2000000) {
		lastCheck = now;
		BPath path;
		enabled = find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK
			&& path.Append("snake_debug") == B_OK && BEntry(path.Path()).Exists();
	}
	if (!enabled)
		return;
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK || path.Append("snake_debug.log") != B_OK)
		return;
	FILE* file = fopen(path.Path(), "a");
	if (file == NULL)
		return;
	BRect frame = child->Frame();
	fprintf(file, "%s child=%g,%g,%g,%g strip=%g,%g,%g,%g parent y=%g..%g\n", what, frame.left, frame.top,
		frame.right, frame.bottom, strip.left, strip.top, strip.right, strip.bottom, parentTop,
		parentBottom);
	fclose(file);
}


static void
PlaceBridge(BMenu* menu, BRect strip, float parentTop, float parentBottom)
{
	BWindow* child = menu->Window();

	// The seam only exists where the two windows are side by side. If the strip is not inside both
	// windows' vertical extent, the geometry is stale (a window still being moved, which was seen as a
	// stray one pixel line on the desktop), so the bridge is hidden rather than placed.
	BRect childFrame = child->Frame();
	if (strip.top < childFrame.top - 2 || strip.bottom > childFrame.bottom + 2
		|| strip.top < parentTop - 2 || strip.bottom > parentBottom + 2) {
		SeamBridge* existing = NULL;
		{
			BAutolock lock(LinkLock());
			std::map<BWindow*, SeamBridge*>::iterator it = Bridges().find(child);
			if (it != Bridges().end())
				existing = it->second;
		}
		if (existing != NULL)
			existing->PostMessage('Hide');
		SeamLog("rejected", child, strip, parentTop, parentBottom);
		return;
	}

	SeamBridge* bridge;
	{
		BAutolock lock(LinkLock());
		std::map<BWindow*, SeamBridge*>::iterator it = Bridges().find(child);
		if (it == Bridges().end()) {
			bridge = new SeamBridge(child);
			bridge->Run();
			Bridges()[child] = bridge;
		} else
			bridge = it->second;
	}
	bridge->Place(strip, Accent());
	SeamLog("placed", child, strip, parentTop, parentBottom);
}


static void
DropBridge(BWindow* child)
{
	SeamBridge* bridge = NULL;
	{
		BAutolock lock(LinkLock());
		std::map<BWindow*, SeamBridge*>::iterator it = Bridges().find(child);
		if (it != Bridges().end())
			bridge = it->second;
	}
	if (bridge != NULL)
		bridge->PostMessage('Hide');
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
	ParentLink() : valid(false), rowTop(0), rowBottom(0), windowLeft(0), parentTop(0), parentBottom(0) {}

	bool	valid;
	float	rowTop;			// the parent's open row, in screen coordinates
	float	rowBottom;
	float	windowLeft;		// the parent window's left edge, in screen coordinates
	float	parentTop;		// and its vertical extent, to check the row against
	float	parentBottom;
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
	if (TrailEnabled() && parent != NULL && item != NULL && parent->Window() != NULL
		&& MenuDrawsTrail(parent)
		&& parent->Window()->IsLocked()) {
		BRect row = parent->ConvertToScreen(item->Frame());
		link.valid = true;
		link.rowTop = row.top;
		link.rowBottom = row.bottom + 1;
		link.windowLeft = parent->Window()->Frame().left;
		link.parentTop = parent->Window()->Frame().top;
		link.parentBottom = parent->Window()->Frame().bottom;
	}
	{
		BAutolock lock(LinkLock());
		Links()[submenu] = link;
	}
	InvalidateParent(parent);
}


void
DetachLink(BMenu* submenu)
{
	if (submenu->Window() != NULL)
		DropBridge(submenu->Window());

	{
		BAutolock lock(LinkLock());
		Links().erase(submenu);
	}
	InvalidateParent(submenu->Supermenu());
}


// SnakeControlLook (when it is the control look) draws the trail and its seam bridge for every menu in the
// program, Tracker's too; drawing them here as well would put two of everything on top of each other.
static bool
ControlLookDraws()
{
	return be_control_look != NULL
		&& strstr(typeid(*be_control_look).name(), "SnakeControlLook") != NULL;
}


void
DrawTrail(BMenu* menu)
{
	if (ControlLookDraws())
		return;

	ParentLink link;
	{
		BAutolock lock(LinkLock());
		std::map<BMenu*, ParentLink>::iterator it = Links().find(menu);
		if (it != Links().end())
			link = it->second;
	}

	if (!MenuDrawsTrail(menu))
		return;

	BRect bounds = menu->Bounds();
	const int w = (int)bounds.Width() + 1, h = (int)bounds.Height() + 1;
	if (w < 8 || h < 8 || menu->Window() == NULL)
		return;
	const float vt = bounds.top;	// view y of the bitmap's first row

	// the selected row and whether its submenu is open
	BMenuItem* selected = NULL;
	for (int32 i = 0; i < menu->CountItems(); i++) {
		SnakeMenuItem* item = dynamic_cast<SnakeMenuItem*>(menu->ItemAt(i));
		if (item != NULL && item->IsItemSelected()) {
			selected = item;
			break;
		}
	}
	const bool active = selected != NULL && (selected->IsEnabled() || selected->Submenu() != NULL);
	BMenu* child = active ? selected->Submenu() : NULL;
	const bool open = child != NULL && child->Window() != NULL && TrailEnabled();
	const float myLeft = menu->Window()->Frame().left;
	const bool childOnRight = open && child->Window()->Frame().left > myLeft;
	const bool parentOnLeft = link.valid && link.windowLeft < myLeft;

	// cover the window border that runs between this menu and its parent, along the parent's open row
	if (link.valid) {
		BRect frame = menu->Window()->Frame();
		float x = parentOnLeft ? frame.left - 1 : frame.right + 1;
		PlaceBridge(menu, BRect(x, link.rowTop + 1, x, link.rowBottom - 2), link.parentTop, link.parentBottom);
	}

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
			float tl = cutTop ? 0.0f : CornerRadius(leftFlush, onTrail && parentOnLeft && expTop);
			float tr = cutTop ? 0.0f : CornerRadius(rightFlush, onTrail && !parentOnLeft && expTop);
			float br = cutBottom ? 0.0f : CornerRadius(rightFlush, onTrail && !parentOnLeft && expBottom);
			float bl = cutBottom ? 0.0f : CornerRadius(leftFlush, onTrail && parentOnLeft && expBottom);
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

	if (!kRoundedFrame && pieces.empty())
		return;

	// Optional rounded frame. The stock background has already drawn a square 1px border; with this on,
	// the corners are painted over with the menu colour (a window can't be see-through) and the border
	// redrawn as a rounded outline. Top and bottom edges only count where the menu meets the window's
	// edge (the rest is a scroll-arrow strip), as in the stock background.
	//
	// Off by default: Tracker's menu windows have a 1px square border drawn by app_server around the
	// client area, so the rounded outline ends up inside a square one, which looks worse than the plain
	// frame. (hDesktop's own menu windows have no such border.)
	const rgb_color bg = ui_color(B_MENU_BACKGROUND_COLOR);
	const rgb_color border = tint_color(bg, B_DARKEN_2_TINT);
	bool frameTop = true, frameBottom = true;
	if (menu->Parent() != NULL) {
		frameTop = menu->Parent()->Frame().top == menu->Window()->Bounds().top;
		frameBottom = menu->Parent()->Frame().bottom == menu->Window()->Bounds().bottom;
	}
	const float kFrameR = 6.5f;
	const float oy0 = frameTop ? 0.0f : -100.0f, oy1 = frameBottom ? (float)h : h + 100.0f;

	// composite: frame first, then the selector on top with its bevel (light where the pixel above is
	// empty, dark where the one below is)
	BBitmap bitmap(BRect(0, 0, w - 1, h - 1), B_RGBA32);
	if (bitmap.InitCheck() != B_OK)
		return;
	const rgb_color base = Accent(), light = Light(base), dark = Dark(base);
	uint8* bits = (uint8*)bitmap.Bits();
	const int32 bpr = bitmap.BytesPerRow();
	bool any = false;
	for (int y = 0; y < h; ++y) {
		uint8* row = bits + y * bpr;
		for (int x = 0; x < w; ++x) {
			// frame layer
			float fa = 0, fr = 0, fg = 0, fb = 0;
			bool nearEdge = kRoundedFrame && (x < 8 || x >= w - 8 || (frameTop && y < 8) || (frameBottom && y >= h - 8));
			if (nearEdge) {
				float outer = RoundRectCoverage(x + 0.5f, y + 0.5f, 0, oy0, (float)w, oy1 - oy0,
					kFrameR, kFrameR, kFrameR, kFrameR);
				float inner = RoundRectCoverage(x + 0.5f, y + 0.5f, 1, oy0 + (frameTop ? 1 : 0),
					w - 2.0f, oy1 - oy0 - (frameTop ? 1 : 0) - (frameBottom ? 1 : 0),
					kFrameR - 1, kFrameR - 1, kFrameR - 1, kFrameR - 1);
				float ring = Clamp01(outer - inner);
				float outside = 1.0f - outer;		// the corner pixels, painted in the menu colour
				fa = 1.0f - (1.0f - outside) * (1.0f - ring);
				if (fa > 0.0f) {
					float wb = outside * (1.0f - ring), wr = ring;
					fr = (bg.red * wb + border.red * wr) / (wb + wr);
					fg = (bg.green * wb + border.green * wr) / (wb + wr);
					fb = (bg.blue * wb + border.blue * wr) / (wb + wr);
				}
			}

			// selector layer
			float c0 = CoverAt(cover, w, h, x, y);
			float sr = 0, sg = 0, sb = 0;
			if (c0 > 0.0f) {
				float a2 = c0 * CoverAt(cover, w, h, x, y - 1), a3 = a2 * CoverAt(cover, w, h, x, y + 1);
				sr = light.red;  sg = light.green;  sb = light.blue;
				sr += (dark.red - sr) * a2;  sg += (dark.green - sg) * a2;  sb += (dark.blue - sb) * a2;
				sr += (base.red - sr) * a3;  sg += (base.green - sg) * a3;  sb += (base.blue - sb) * a3;
			}

			// selector over frame
			float oa = c0 + fa * (1.0f - c0);
			uint8* px = row + x * 4;
			if (oa <= 0.0f) {
				px[0] = px[1] = px[2] = px[3] = 0;
				continue;
			}
			any = true;
			float cr = (sr * c0 + fr * fa * (1.0f - c0)) / oa;
			float cg = (sg * c0 + fg * fa * (1.0f - c0)) / oa;
			float cb = (sb * c0 + fb * fa * (1.0f - c0)) / oa;
			px[0] = (uint8)lroundf(std::min(255.0f, cb));
			px[1] = (uint8)lroundf(std::min(255.0f, cg));
			px[2] = (uint8)lroundf(std::min(255.0f, cr));
			px[3] = (uint8)lroundf(oa * 255.0f);
		}
	}
	if (!any)
		return;

	menu->PushState();
	menu->SetDrawingMode(B_OP_ALPHA);
	menu->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	menu->DrawBitmap(&bitmap, bounds.LeftTop());
	menu->PopState();
}

}	// namespace SnakeSelector
