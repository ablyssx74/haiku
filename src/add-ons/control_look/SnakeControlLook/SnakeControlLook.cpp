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
#include <utility>
#include <new>
#include <typeinfo>
#include <vector>

#include <Application.h>
#include <Autolock.h>
#include <Bitmap.h>
#include <Entry.h>
#include <GradientLinear.h>
#include <File.h>
#include <FindDirectory.h>
#include <Locker.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <Menu.h>
#include <Shape.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Message.h>
#include <MessageFilter.h>
#include <Path.h>
#include <Roster.h>
#include <View.h>
#include <Window.h>

#include "HaikuControlLook.h"


namespace BPrivate {

static const bool kBulge = true;	// a tab of the selector outside the menu's outer edge, in an overlay window
static const int kBulgeW = 3;
static const int kVBulgeW = 4;		// the same, and the column of window border between the two menus
static const bool kVBulge = true;	// the vertical part of the trail also bulges, into the parent menu
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
static bool sFlat = true;
static bool sArrows = false;
static bigtime_t sLastCheck = -kCheckInterval;


// Tracker's settings file is plain text, one "Name value" per line.
static void
ReadTrackerSettings(rgb_color& accent, bool& trail, bool& flat, bool& arrows)
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
		} else if (strncmp(line, "SnakeFlat ", 10) == 0) {
			flat = strncmp(line + 10, "off", 3) != 0 && strncmp(line + 10, "0", 1) != 0
				&& strncmp(line + 10, "false", 5) != 0;
		} else if (strncmp(line, "SnakeArrows ", 12) == 0) {
			arrows = strncmp(line + 12, "off", 3) != 0 && strncmp(line + 12, "0", 1) != 0
				&& strncmp(line + 12, "false", 5) != 0;
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
	bool flat = true;
	bool arrows = false;
	ReadTrackerSettings(accent, trail, flat, arrows);

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
			if (settings.FindBool("nav_snake_flat", &value) == B_OK)
				flat = value;
		}
	}
	sAccent = accent;
	sTrail = trail;
	sFlat = flat;
	sArrows = arrows;
}


static rgb_color
Accent()
{
	RefreshSettings();
	BAutolock lock(SettingsLock());
	return sAccent;
}


static bool
ShowArrows()
{
	RefreshSettings();
	BAutolock lock(SettingsLock());
	return sArrows;
}


static bool
FlatFill()
{
	RefreshSettings();
	BAutolock lock(SettingsLock());
	return sFlat;
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


static rgb_color
MixColors(rgb_color a, rgb_color b, float amount)
{
	return make_color((uint8)(a.red + (b.red - a.red) * amount),
		(uint8)(a.green + (b.green - a.green) * amount),
		(uint8)(a.blue + (b.blue - a.blue) * amount));
}


static rgb_color Light(rgb_color c) { return Mix(c, 255, 0.35f); }
static rgb_color Dark(rgb_color c) { return Mix(c, 0, 0.38f); }
static rgb_color Outline(rgb_color c) { return Mix(c, 0, 0.72f); }


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
	ParentLink() : valid(false), rowTop(0), rowBottom(0), windowLeft(0), windowRight(0), parentTop(0), parentBottom(0) {}

	bool	valid;
	float	rowTop;			// the parent's open row, in screen coordinates
	float	rowBottom;
	float	windowLeft;		// the parent window's left edge, in screen coordinates
	float	windowRight;	// and its right edge
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


// The part of a menu's edge that the trail's vertical bar covers (the pill look), in the menu's own coordinates: an arrow
// that points into a submenu and lies within it is hidden, the snake passes through it.
struct TrailBar {
	TrailBar() : valid(false), onRight(false), top(0), bottom(0) {}

	bool	valid;
	bool	onRight;
	float	top;
	float	bottom;
};

static std::map<BMenu*, TrailBar>&
TrailBars()
{
	static std::map<BMenu*, TrailBar>* bars = new std::map<BMenu*, TrailBar>();
	return *bars;
}


static ParentLink
FindParentLink(BMenu* menu)
{
	BMenu* parent = menu->Supermenu();
	BMenuItem* item = menu->Superitem();
	ParentLink link;
	// (the window menu of a program in the Deskbar's list opens beside the list as a menu of its own: no trail, no overlap)
	if (parent == NULL || item == NULL || parent->Window() == NULL || dynamic_cast<BMenuBar*>(parent) != NULL
		|| strstr(typeid(*parent).name(), "TTeamMenu") != NULL) {
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
		link.windowRight = parentWindow->Frame().right;
		link.parentTop = parentWindow->Frame().top;
		link.parentBottom = parentWindow->Frame().bottom;

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


// The Deskbar's own menu (the leaf menu) has no parent menu, but its trail starts at the Deskbar. The menu opens
// beside the Deskbar when that is a vertical, expanded one, and below or above the leaf (a menu bar title) in
// the other layouts; the strip of the trail runs down the edge that faces the Deskbar, or the side the leaf is
// on, and starts at the menu's top (or ends at its bottom, when the menu opens upwards).
static bool IsPillMenuBar(BView* view);

enum DeskbarPlace { kNotDeskbar, kBesideDeskbar, kBelowLeaf, kAboveLeaf };

struct LeafInfo {
	BRect	leaf;
	bool	pill;
};

static DeskbarPlace
FindDeskbarPlace(BMenu* menu, bool* stripOnLeft, BRect* leafOut = NULL)
{
	if (menu->Window() == NULL)
		return kNotDeskbar;
	// the Deskbar's own menu, or a menu that hangs below a title of a pill menu bar
	const bool deskbarMenu = strstr(typeid(*menu).name(), "TDeskbarMenu") != NULL;
	BMenu* bar = menu->Supermenu();
	BMenuItem* title = menu->Superitem();
	if (!deskbarMenu && (bar == NULL || dynamic_cast<BMenuBar*>(bar) == NULL))
		return kNotDeskbar;
	if (bar == NULL || title == NULL || bar->Window() == NULL || bar->Window() == menu->Window())
		return kNotDeskbar;
	BRect desk = bar->Window()->Frame();
	BRect frame = menu->Window()->Frame();
	// the leaf's place on the screen: the Deskbar's window has to be locked to ask the menu bar
	// (the bar's window is busy while the mouse is tracked, so the last answer is remembered per menu)
	static std::map<BMenu*, LeafInfo>* sLeaves = new std::map<BMenu*, LeafInfo>();
	BRect leaf;
	if (bar->Window()->LockWithTimeout(0) == B_OK) {
		// (asking the bar anything needs its window locked)
		LeafInfo info;
		info.pill = deskbarMenu || IsPillMenuBar(bar);
		info.leaf = bar->ConvertToScreen(title->Frame());
		bar->Window()->Unlock();
		BAutolock lock(LinkLock());
		(*sLeaves)[menu] = info;
	}
	{
		BAutolock lock(LinkLock());
		std::map<BMenu*, LeafInfo>::iterator it = sLeaves->find(menu);
		if (it != sLeaves->end()) {
			if (!it->second.pill)
				return kNotDeskbar;
			leaf = it->second.leaf;
		} else if (deskbarMenu)
			leaf = desk;
		else
			return kNotDeskbar;
	}
	if (leafOut != NULL)
		*leafOut = leaf;
	const float slack = 8;

	// beside the Deskbar
	if (deskbarMenu && frame.bottom >= desk.top && frame.top <= desk.bottom) {
		if (fabsf(frame.right + 1 - desk.left) <= slack) {
			*stripOnLeft = false;
			return kBesideDeskbar;
		}
		if (fabsf(desk.right + 1 - frame.left) <= slack) {
			*stripOnLeft = true;
			return kBesideDeskbar;
		}
	}

	// below or above the leaf, on the side of the menu the leaf is on
	if (frame.right >= leaf.left && frame.left <= leaf.right) {
		*stripOnLeft = fabsf(frame.left - leaf.left) <= fabsf(frame.right - leaf.right);
		if (fabsf(frame.top - (leaf.bottom + 1)) <= slack
			|| (deskbarMenu && fabsf(frame.top - (desk.bottom + 1)) <= slack))
			return kBelowLeaf;
		if (deskbarMenu && (fabsf(frame.bottom + 1 - leaf.top) <= slack || fabsf(frame.bottom + 1 - desk.top) <= slack))
			return kAboveLeaf;
	}
	return kNotDeskbar;
}


// A one pixel column of the window system's border runs between two menu windows that sit side by side, and
// shows as a dark seam through the joined selector. It can't be painted over from inside either window, so a
// borderless window one pixel wide is put over it, in the selector's colours, for as long as the submenu is
// shown.
class SeamBridge;
static void ForgetBridge(SeamBridge* bridge);


class SeamBridge : public BWindow {
public:
	SeamBridge(BWindow* child, int width = 1, bool bulge = false, bool horizontal = false)
		:
		BWindow(BRect(0, 0, width - 1, 7), "seam", B_NO_BORDER_WINDOW_LOOK, (window_feel)1025,
			B_NOT_MOVABLE | B_NOT_CLOSABLE | B_NOT_ZOOMABLE | B_NOT_MINIMIZABLE | B_NOT_RESIZABLE
				| B_AVOID_FOCUS),
		fChild(child),
		fRunner(NULL),
		fAccent(Accent()),
		fWidth(width),
		fBulge(bulge),
		fBulgeRight(true),
		fEdgeTop(true),
		fEdgeBottom(true),
		fSkipFrom(-1),
		fSkipTo(-1),
		fRounded(false),
		fHeight(8),
		fLeft(-1),
		fTop(-1),
		fHorizontal(horizontal),
		fFill(make_color(0, 0, 0, 0)),
		fHotFrom(0),
		fHotTo(1000000)
	{
		SetSizeLimits(0, width - 1, 0, 4000);
		BView* view = new SeamView(this);
		AddChild(view);
		BMessage tick('Tick');
		fRunner = new BMessageRunner(BMessenger(this), &tick, 25000);
	}

	~SeamBridge()
	{
		delete fRunner;
	}

	// from the submenu's thread: where the seam is. The submenu redraws on every hover change, so nothing is
	// touched unless the seam actually moved or changed colour (a needless resize or redraw flickers).
	void Place(BRect strip, rgb_color accent, bool bulgeRight = true, bool edgeTop = true, bool edgeBottom = true,
		int skipFrom = -1, int skipTo = -1, bool rounded = false, rgb_color fill = make_color(0, 0, 0, 0),
		int hotFrom = 0, int hotTo = 1000000)
	{
		if (LockWithTimeout(20000) != B_OK)
			return;		// busy; the next draw places it
		const int left = (int)strip.left, top = (int)strip.top, height = (int)strip.Height() + 1;
		bool same = !IsHidden() && left == fLeft && top == fTop && height == fHeight
			&& bulgeRight == fBulgeRight && edgeTop == fEdgeTop && edgeBottom == fEdgeBottom && skipFrom == fSkipFrom
			&& skipTo == fSkipTo && rounded == fRounded && accent.red == fAccent.red && accent.green == fAccent.green
			&& accent.blue == fAccent.blue && hotFrom == fHotFrom && hotTo == fHotTo && fill.red == fFill.red
			&& fill.green == fFill.green && fill.blue == fFill.blue && fill.alpha == fFill.alpha;
		if (!same) {
			fFill = fill;
			fHotFrom = hotFrom;
			fHotTo = hotTo;
			fAccent = accent;
			fBulgeRight = bulgeRight;
			fEdgeTop = edgeTop;
			fEdgeBottom = edgeBottom;
			fSkipFrom = skipFrom;
			fSkipTo = skipTo;
			fRounded = rounded;
			fHeight = height;
			fLeft = left;
			fTop = top;
			ResizeTo(fWidth - 1, height - 1);
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
					bool beside = ((mine.right <= frame.left - 1 && mine.right >= frame.left - 6)
						|| (mine.left >= frame.right + 1 && mine.left <= frame.right + 6))
						&& mine.top >= frame.top - 2 && mine.bottom <= frame.bottom + 2;
					if (!beside && !fHorizontal)
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

	rgb_color Color() const { return fAccent; }
	int Height() const { return fHeight; }
	int Width() const { return fWidth; }
	bool IsBulge() const { return fBulge; }
	bool BulgeRight() const { return fBulgeRight; }
	bool EdgeTop() const { return fEdgeTop; }
	bool EdgeBottom() const { return fEdgeBottom; }
	int SkipFrom() const { return fSkipFrom; }
	int SkipTo() const { return fSkipTo; }
	bool Rounded() const { return fRounded; }
	bool Horizontal() const { return fHorizontal; }
	rgb_color FillColor() const { return fFill; }
	int HotFrom() const { return fHotFrom; }
	int HotTo() const { return fHotTo; }

private:
	class SeamView : public BView {
	public:
		SeamView(SeamBridge* bridge)
			:
			BView(BRect(0, 0, bridge->Width() - 1, 7), "seam", B_FOLLOW_ALL, B_WILL_DRAW),
			fBridge(bridge)
		{
			SetViewColor(B_TRANSPARENT_COLOR);
		}

		virtual void Draw(BRect)
		{
			rgb_color accent = fBridge->Color();
			int h = fBridge->Height();
			if (fBridge->IsBulge()) {
				// a tab of the selector sticking out of the menu: the side against the window is joined to
				// the row, the other three sides are outlined
				int w = fBridge->Width();
				SetHighColor(accent);
				FillRect(BRect(0, 0, w - 1, h - 1));
				SetHighColor(Outline(accent));
				if (fBridge->EdgeTop())
					StrokeLine(BPoint(0, 0), BPoint(w - 1, 0));
				if (fBridge->EdgeBottom())
					StrokeLine(BPoint(0, h - 1), BPoint(w - 1, h - 1));
				float far = fBridge->BulgeRight() ? w - 1 : 0;
				// the side away from the window it hangs on, except where it runs along a lit row
				int from = 0;
				int skipFrom = fBridge->SkipFrom(), skipTo = fBridge->SkipTo();
				if (skipFrom >= 0 && skipTo >= skipFrom) {
					if (skipFrom > 0)
						StrokeLine(BPoint(far, 0), BPoint(far, skipFrom - 1));
					from = skipTo + 1;
				}
				if (from < h)
					StrokeLine(BPoint(far, from), BPoint(far, h - 1));

				// Rounded corners at the free ends. This tab hangs over the body of a menu, whose colour is
				// known, so the pixels outside the curve are painted in it.
				if (fBridge->Rounded()) {
					const rgb_color back = ui_color(B_MENU_BACKGROUND_COLOR);
					const rgb_color edge = Outline(accent);
					for (int end = 0; end < 2; end++) {
						if (end == 0 ? !fBridge->EdgeTop() : !fBridge->EdgeBottom())
							continue;
						for (int u = 0; u < 3; u++) {
							for (int v = 0; v < 3; v++) {
								float dx = u + 0.5f - 3.0f, dy = v + 0.5f - 3.0f;
								float d = sqrtf(dx * dx + dy * dy);
								float outer = std::min(1.0f, std::max(0.0f, 3.0f - d + 0.5f));
								float inner = std::min(1.0f, std::max(0.0f, 2.0f - d + 0.5f));
								float r = back.red + (edge.red + (accent.red - edge.red) * inner - back.red) * outer;
								float g = back.green + (edge.green + (accent.green - edge.green) * inner - back.green) * outer;
								float b = back.blue + (edge.blue + (accent.blue - edge.blue) * inner - back.blue) * outer;
								int x = fBridge->BulgeRight() ? w - 1 - u : u;
								int y = end == 0 ? v : h - 1 - v;
								SetHighColor((uint8)r, (uint8)g, (uint8)b);
								FillRect(BRect(x, y, x, y));
							}
						}
					}
				}
				return;
			}
			const bool flat = FlatFill();
			const bool horizontal = fBridge->Horizontal();
			const bool twoColours = fBridge->FillColor().alpha != 0;
			const int length = horizontal ? fBridge->Width() : h;
			for (int y = 0; y < length; y++) {
				if (twoColours)
					SetHighColor((y < fBridge->HotFrom() || y > fBridge->HotTo()) ? fBridge->FillColor() : accent);
				else if (y == 0 && !flat)
					SetHighColor(Light(accent));
				else if (y == length - 1 && !flat)
					SetHighColor(Dark(accent));
				else
					SetHighColor(accent);
				FillRect(horizontal ? BRect(y, 0, y, 0) : BRect(0, y, 0, y));
			}
		}

	private:
		SeamBridge*	fBridge;
	};

	BMessenger		fChild;
	BMessageRunner*	fRunner;
	rgb_color		fAccent;
	int				fWidth;
	bool			fBulge;
	bool			fBulgeRight;
	bool			fEdgeTop;
	bool			fEdgeBottom;
	int				fSkipFrom;
	int				fSkipTo;
	bool			fRounded;
	int				fHeight;
	int				fLeft;
	int				fTop;
	bool			fHorizontal;
	rgb_color		fFill;		// the colour outside the lit part, when there is one (alpha 0: none)
	int				fHotFrom;
	int				fHotTo;
};


static std::map<BWindow*, SeamBridge*>&
Bridges()
{
	static std::map<BWindow*, SeamBridge*>* bridges = new std::map<BWindow*, SeamBridge*>();
	return *bridges;
}


static std::map<BWindow*, SeamBridge*>&
Bulges()
{
	static std::map<BWindow*, SeamBridge*>* bulges = new std::map<BWindow*, SeamBridge*>();
	return *bulges;
}


static std::map<BWindow*, SeamBridge*>&
TopCovers()
{
	static std::map<BWindow*, SeamBridge*>* covers = new std::map<BWindow*, SeamBridge*>();
	return *covers;
}


static std::map<BWindow*, SeamBridge*>&
BottomCovers()
{
	static std::map<BWindow*, SeamBridge*>* covers = new std::map<BWindow*, SeamBridge*>();
	return *covers;
}


static std::map<BWindow*, SeamBridge*>&
VBulges()
{
	static std::map<BWindow*, SeamBridge*>* bulges = new std::map<BWindow*, SeamBridge*>();
	return *bulges;
}


static void
ForgetBridge(SeamBridge* bridge)
{
	BAutolock lock(LinkLock());
	for (std::map<BWindow*, SeamBridge*>::iterator it = TopCovers().begin(); it != TopCovers().end();) {
		if (it->second == bridge)
			TopCovers().erase(it++);
		else
			++it;
	}
	for (std::map<BWindow*, SeamBridge*>::iterator it = BottomCovers().begin(); it != BottomCovers().end();) {
		if (it->second == bridge)
			BottomCovers().erase(it++);
		else
			++it;
	}
	for (std::map<BWindow*, SeamBridge*>::iterator it = VBulges().begin(); it != VBulges().end();) {
		if (it->second == bridge)
			VBulges().erase(it++);
		else
			++it;
	}
	for (std::map<BWindow*, SeamBridge*>::iterator it = Bulges().begin(); it != Bulges().end();) {
		if (it->second == bridge)
			Bulges().erase(it++);
		else
			++it;
	}
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
HideBridge(BWindow* window)
{
	SeamBridge* existing = NULL;
	{
		BAutolock lock(LinkLock());
		std::map<BWindow*, SeamBridge*>::iterator it = Bridges().find(window);
		if (it != Bridges().end())
			existing = it->second;
	}
	if (existing != NULL)
		existing->PostMessage('Hide');
}


// Menu windows are reused, and one that is shown again comes up in front of the covers made for it earlier, so its border
// would show over them. A cover that is already up is therefore shown again now and then, which puts it back in front
// (at most every so often per window: it is a brief flicker of a one pixel window).
struct RaisedCover {
	RaisedCover() : frame(), time(0) {}

	BRect		frame;
	bigtime_t	time;
};


static void
KeepInFront(BWindow* child, SeamBridge* cover)
{
	static std::map<std::pair<BWindow*, SeamBridge*>, RaisedCover>* last
		= new std::map<std::pair<BWindow*, SeamBridge*>, RaisedCover>();
	const bigtime_t now = system_time();
	const BRect frame = child->Frame();
	{
		BAutolock lock(LinkLock());
		RaisedCover& done = (*last)[std::make_pair(child, cover)];
		// at once when this window is not showing what it showed when the cover was last brought forward (it is being
		// used for another submenu), otherwise now and then
		if (done.time != 0 && done.frame == frame && now - done.time < 150000)
			return;
		done.frame = frame;
		done.time = now;
	}
	if (cover->LockWithTimeout(20000) == B_OK) {
		if (!cover->IsHidden()) {
			cover->Hide();
			cover->Show();
		}
		cover->Unlock();
	}
}


// A one pixel strip, in a flat colour, over a line of the window system's border (the pill look, where menus overlap).
static void
PlaceCover(BMenu* menu, std::map<BWindow*, SeamBridge*>& covers, BRect strip, rgb_color color)
{
	BWindow* child = menu->Window();
	SeamBridge* cover;
	{
		BAutolock lock(LinkLock());
		std::map<BWindow*, SeamBridge*>::iterator it = covers.find(child);
		if (it == covers.end()) {
			cover = new SeamBridge(child, 17, false, true);
			cover->Run();
			covers[child] = cover;
		} else
			cover = it->second;
	}
	cover->Place(strip, color);
	KeepInFront(child, cover);
}


static void
HideCovers(BWindow* child)
{
	SeamBridge* top = NULL;
	SeamBridge* bottom = NULL;
	{
		BAutolock lock(LinkLock());
		std::map<BWindow*, SeamBridge*>::iterator it = TopCovers().find(child);
		if (it != TopCovers().end())
			top = it->second;
		it = BottomCovers().find(child);
		if (it != BottomCovers().end())
			bottom = it->second;
	}
	if (top != NULL)
		top->PostMessage('Hide');
	if (bottom != NULL)
		bottom->PostMessage('Hide');
}


static void
PlaceBridge(BMenu* menu, BRect strip, float parentTop, float parentBottom, rgb_color color,
	rgb_color fill = make_color(0, 0, 0, 0), int hotFrom = 0, int hotTo = 1000000)
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
	bridge->Place(strip, color, true, true, true, -1, -1, false, fill, hotFrom, hotTo);
	KeepInFront(child, bridge);
	SeamLog("placed", child, strip, parentTop, parentBottom);
}


static void
HideBulge(BWindow* child)
{
	SeamBridge* bulge = NULL;
	{
		BAutolock lock(LinkLock());
		std::map<BWindow*, SeamBridge*>::iterator it = Bulges().find(child);
		if (it != Bulges().end())
			bulge = it->second;
	}
	if (bulge != NULL)
		bulge->PostMessage('Hide');
}


static void
PlaceBulge(BMenu* menu, BRect strip, bool right)
{
	BWindow* child = menu->Window();
	BRect childFrame = child->Frame();
	if (strip.top < childFrame.top - 2 || strip.bottom > childFrame.bottom + 2) {
		HideBulge(child);
		return;
	}

	SeamBridge* bulge;
	{
		BAutolock lock(LinkLock());
		std::map<BWindow*, SeamBridge*>::iterator it = Bulges().find(child);
		if (it == Bulges().end()) {
			bulge = new SeamBridge(child, kBulgeW, true);
			bulge->Run();
			Bulges()[child] = bulge;
		} else
			bulge = it->second;
	}
	bulge->Place(strip, Accent(), right);
}


static void
HideVBulge(BWindow* child)
{
	SeamBridge* bulge = NULL;
	{
		BAutolock lock(LinkLock());
		std::map<BWindow*, SeamBridge*>::iterator it = VBulges().find(child);
		if (it != VBulges().end())
			bulge = it->second;
	}
	if (bulge != NULL)
		bulge->PostMessage('Hide');
}


// The vertical part of the trail, from the parent's row to the row of the submenu, hangs a few pixels into the
// parent menu, as the selected row hangs out of its menu.
static void
PlaceVBulge(BMenu* menu, BRect strip, bool right, bool edgeTop, bool edgeBottom, int skipFrom, int skipTo,
	bool rounded)
{
	BWindow* child = menu->Window();
	BRect childFrame = child->Frame();
	// (the strip may run past the parent menu, over the desktop: only the child's own extent has to fit)
	if (strip.top < childFrame.top - 2 || strip.bottom > childFrame.bottom + 2) {
		HideVBulge(child);
		return;
	}

	SeamBridge* bulge;
	{
		BAutolock lock(LinkLock());
		std::map<BWindow*, SeamBridge*>::iterator it = VBulges().find(child);
		if (it == VBulges().end()) {
			bulge = new SeamBridge(child, kVBulgeW, true);
			bulge->Run();
			VBulges()[child] = bulge;
		} else
			bulge = it->second;
	}
	bulge->Place(strip, Accent(), right, edgeTop, edgeBottom, skipFrom, skipTo, rounded);
}


static void
DropBridge(BWindow* child)
{
	HideVBulge(child);
	SeamBridge* bridge = NULL;
	{
		BAutolock lock(LinkLock());
		std::map<BWindow*, SeamBridge*>::iterator bulgeIt = Bulges().find(child);
		if (bulgeIt != Bulges().end()) {
			bulgeIt->second->PostMessage(B_QUIT_REQUESTED);
			Bulges().erase(bulgeIt);
		}
		std::map<BWindow*, SeamBridge*>::iterator it = Bridges().find(child);
		if (it != Bridges().end()) {
			bridge = it->second;
			Bridges().erase(it);
		}
	}
	if (bridge != NULL)
		bridge->PostMessage(B_QUIT_REQUESTED);
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


// #pragma mark - the pill look

// Tracker takes a picture of the screen just before it opens one of its popup menus and hands it over
// (snake_set_menu_backdrop()). A menu window is a rectangle, so the round ends of a pill would show square corners; with
// the picture, each menu level paints the part of the screen behind it as its own background and a solid pill over
// that, which looks see-through (the same way hDesktop's menus are drawn). Menus of other programs, and menu bars,
// have no picture and are drawn as before.
struct BackdropState {
	BackdropState() : bitmap(NULL), time(0), root(NULL), eligible(NULL) {}

	BBitmap*	bitmap;
	bigtime_t	time;
	BMenu*		root;		// the menu this picture was taken for (set when it is first drawn)
	bool		(*eligible)(BMenu*);	// Tracker's own test of whether a menu holds nothing but its items
};

static BLocker&
BackdropLock()
{
	static BLocker* lock = new BLocker("snake menu backdrop");
	return *lock;
}


static BackdropState&
Backdrop()
{
	static BackdropState* state = new BackdropState();
	return *state;
}


static const bigtime_t kBackdropValid = 60000000;	// a minute


static float
Luminance(const rgb_color& c)
{
	return 0.299f * c.red + 0.587f * c.green + 0.114f * c.blue;
}


// The pill's colour (the Selector Color in a light tint), the lit row's (the Selector Color), and the text on the pill.
static void
PillColors(rgb_color* fill, rgb_color* hot, rgb_color* textOnFill)
{
	const rgb_color white = make_color(255, 255, 255);
	const bool darkTheme = Luminance(ui_color(B_MENU_BACKGROUND_COLOR)) < 128.0f;
	*fill = MixColors(Accent(), white, darkTheme ? 0.62f : 0.42f);
	*hot = MixColors(Accent(), white, darkTheme ? 0.18f : 0.0f);
	*textOnFill = Luminance(*fill) > 140.0f ? make_color(20, 22, 28) : white;
}


static BMenu*
RootMenu(BMenu* menu)
{
	// (the top of the chain of menus: a menu bar's own menus start one, the bar is not part of it)
	while (menu->Supermenu() != NULL && dynamic_cast<BMenuBar*>(menu->Supermenu()) == NULL)
		menu = menu->Supermenu();
	return menu;
}


// Whether this menu level is drawn in the pill look.
static bool
PillMenu(BMenu* menu)
{
	if (menu == NULL || menu->Window() == NULL || dynamic_cast<BMenuBar*>(menu) != NULL)
		return false;
	BMenu* root = RootMenu(menu);
	// a popup menu, or the Deskbar's own menu
	// (or the Deskbar's list of programs, when it is a menu, or the window menu of a program in it)
	if (dynamic_cast<BPopUpMenu*>(root) == NULL && strstr(typeid(*root).name(), "TDeskbarMenu") == NULL
		&& strstr(typeid(*root).name(), "TWindowMenu") == NULL && strstr(typeid(*root).name(), "TTeamMenu") == NULL)
		return false;

	BAutolock lock(BackdropLock());
	BackdropState& state = Backdrop();
	if (state.bitmap == NULL)
		return false;
	// A picture is taken for one menu chain: it is claimed by the first menu drawn after it (which must be soon after it
	// was taken, so a picture left over from an old chain is never picked up by a later menu), and is good for as long as
	// that chain is open, however long that is.
	if (state.root == NULL && system_time() - state.time > kBackdropValid)
		return false;
	// the menu must hold nothing but Tracker's own items (and separators): they draw their text in the pill's colours
	if (state.eligible == NULL || !state.eligible(menu))
		return false;
	if (state.root == NULL)
		state.root = root;
	else if (state.root != root || root->Window() == NULL || root->Window()->IsHidden())
		return false;

	// (a menu taller than the screen, with scroll arrows, only partly fits in the picture: that is drawn too)
	const BRect bounds = menu->Bounds();
	const BPoint origin = menu->ConvertToScreen(bounds.LeftTop());
	return (state.bitmap->Bounds() & BRect(origin.x, origin.y, origin.x + bounds.Width(), origin.y + bounds.Height()))
		.IsValid();
}


// Which side the menu this one was opened from is on: -1 left, 1 right, 0 for a root menu. The corner of a submenu
// that sits against its parent's row is square.
static int
ParentSide(BMenu* menu)
{
	BMenu* parent = menu->Supermenu();
	if (parent != NULL && menu->Window() != NULL && dynamic_cast<BMenuBar*>(parent) != NULL
		&& strstr(typeid(*menu).name(), "TDeskbarMenu") != NULL) {
		// the Deskbar's menu beside the Deskbar: its corner against the Deskbar is square too
		bool stripOnLeft = false;
		if (FindDeskbarPlace(menu, &stripOnLeft) == kBesideDeskbar)
			return stripOnLeft ? -1 : 1;
		return 0;
	}
	if (parent == NULL || parent->Window() == NULL || menu->Window() == NULL || dynamic_cast<BMenuBar*>(parent) != NULL)
		return 0;
	return parent->Window()->Frame().left < menu->Window()->Frame().left ? -1 : 1;
}


// Whether the bottom of this submenu lies within its parent's height: its corner on the parent's side then lies over the
// parent's panel, and is square too (a round one would show the picture of the empty desktop there).
static bool
BottomOverParent(BMenu* menu)
{
	BMenu* parent = menu->Supermenu();
	if (parent == NULL || parent->Window() == NULL || menu->Window() == NULL)
		return false;
	return menu->Window()->Frame().bottom <= parent->Window()->Frame().bottom;
}


// Which corners of a menu's pill are square: the one against its parent (or against the Deskbar), and the one at the bottom
// when it ends within its parent's height; below or above the Deskbar's leaf, the corner on the side of the snake's bar.
static void
SquareCorners(BMenu* menu, bool* tl, bool* tr, bool* br, bool* bl)
{
	*tl = *tr = *br = *bl = false;
	const int side = ParentSide(menu);
	const bool squareBottom = side != 0 && BottomOverParent(menu);
	if (side < 0) {
		*tl = true;
		*bl = squareBottom;
	} else if (side > 0) {
		*tr = true;
		*br = squareBottom;
	} else if (strstr(typeid(*menu).name(), "TDeskbarMenu") != NULL || strstr(typeid(*menu).name(), "TTeamMenu") != NULL) {
		bool stripOnLeft = false;
		const DeskbarPlace place = FindDeskbarPlace(menu, &stripOnLeft);
		if (place == kBelowLeaf) {
			*tl = stripOnLeft;
			*tr = !stripOnLeft;
		} else if (place == kAboveLeaf) {
			*bl = stripOnLeft;
			*br = !stripOnLeft;
		}
	}
}


// The menu's background in the pill look: the screen behind it, and the solid pill over that.
static void
DrawPillBase(BMenu* menu)
{
	const BRect bounds = menu->Bounds();
	const BPoint origin = menu->ConvertToScreen(bounds.LeftTop());
	const BRect source(origin.x, origin.y, origin.x + bounds.Width(), origin.y + bounds.Height());

	float radius = 8.0f;
	for (int32 i = 0; i < menu->CountItems(); i++) {
		BMenuItem* item = menu->ItemAt(i);
		if (dynamic_cast<BSeparatorItem*>(item) == NULL) {
			radius = floorf((item->Frame().Height() + 1.0f - 2.0f) / 2.0f);
			break;
		}
	}
	if (radius < 3.0f)
		radius = 3.0f;

	rgb_color fill, hot, text;
	PillColors(&fill, &hot, &text);

	menu->PushState();
	menu->SetDrawingMode(B_OP_COPY);
	{
		BAutolock lock(BackdropLock());
		BackdropState& state = Backdrop();
		if (state.bitmap != NULL) {
			// only the part of the menu that is on the screen has a picture
			const BRect shown = source & state.bitmap->Bounds();
			if (shown.IsValid()) {
				const BRect target(bounds.left + (shown.left - source.left), bounds.top + (shown.top - source.top),
					bounds.left + (shown.right - source.left), bounds.top + (shown.bottom - source.top));
				menu->DrawBitmap(state.bitmap, shown, target);
			}
		}
	}
	menu->SetDrawingMode(B_OP_ALPHA);
	menu->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	menu->SetHighColor(fill);
	menu->FillRoundRect(bounds, radius, radius);
	bool sqTL, sqTR, sqBR, sqBL;
	SquareCorners(menu, &sqTL, &sqTR, &sqBR, &sqBL);
	if (sqTL)
		menu->FillRect(BRect(bounds.left, bounds.top, bounds.left + radius, bounds.top + radius));
	if (sqTR)
		menu->FillRect(BRect(bounds.right - radius, bounds.top, bounds.right, bounds.top + radius));
	if (sqBL)
		menu->FillRect(BRect(bounds.left, bounds.bottom - radius, bounds.left + radius, bounds.bottom));
	if (sqBR)
		menu->FillRect(BRect(bounds.right - radius, bounds.bottom - radius, bounds.right, bounds.bottom));
	menu->PopState();
}


// In the pill look a submenu is moved over its parent, far enough to cover the arrow that marks the row it was opened
// from, so the two windows overlap and the border between them lies under the submenu.
static void
OverlapParent(BMenu* menu, const ParentLink& link)
{
	BWindow* window = menu->Window();
	if (window == NULL || !link.valid)
		return;
	const float overlap = 16.0f;
	const BRect frame = window->Frame();
	// where the parent is now (it may itself have been moved since the link was recorded)
	float parentLeft = link.windowLeft, parentRight = link.windowRight;
	BMenu* parentMenu = menu->Supermenu();
	if (parentMenu != NULL && parentMenu->Window() != NULL) {
		const BRect parentFrame = parentMenu->Window()->Frame();
		parentLeft = parentFrame.left;
		parentRight = parentFrame.right;
	}
	const bool parentOnLeft = parentLeft < frame.left;
	const float delta = parentOnLeft ? (parentRight + 1.0f - overlap) - frame.left
		: (parentLeft - 1.0f + overlap) - frame.right;
	// only the small gap between the windows; anything else is some other geometry and is left alone
	if (fabsf(delta) >= 1.0f && fabsf(delta) <= 24.0f)
		window->MoveBy(delta, 0);
	// the window opens a pixel below its parent's row; it is lifted so that its first row lines up with the parent's
	const float lift = frame.top - link.rowTop;
	if (lift >= 1.0f && lift <= 3.0f)
		window->MoveBy(0, -lift);
}


// The menu code moves a submenu's window now and then (when its items have been listed, say) after it was last drawn,
// which would undo the overlap with nothing to redo it. So the window is watched: when it is moved, the overlap is
// applied again, and the menu is drawn again so the strips over its border follow.
class OverlapFilter : public BMessageFilter {
public:
	OverlapFilter() : BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE, B_WINDOW_MOVED) {}

	virtual filter_result Filter(BMessage* message, BHandler** target)
	{
		BWindow* window = dynamic_cast<BWindow*>(Looper());
		if (window == NULL)
			return B_DISPATCH_MESSAGE;
		for (int32 i = 0; i < window->CountChildren(); i++) {
			BMenu* menu = dynamic_cast<BMenu*>(window->ChildAt(i));
			if (menu == NULL || !PillMenu(menu))
				continue;
			ParentLink link;
			{
				BAutolock lock(LinkLock());
				std::map<BMenu*, ParentLink>::iterator it = Links().find(menu);
				if (it != Links().end())
					link = it->second;
			}
			if (link.valid) {
				OverlapParent(menu, link);
				menu->Invalidate();
			}
		}
		return B_DISPATCH_MESSAGE;
	}
};


static void
EnsureOverlapFilter(BWindow* window)
{
	BList* filters = window->CommonFilterList();
	if (filters != NULL) {
		for (int32 i = 0; i < filters->CountItems(); i++) {
			if (dynamic_cast<OverlapFilter*>((BMessageFilter*)filters->ItemAt(i)) != NULL)
				return;
		}
	}
	window->AddCommonFilter(new OverlapFilter());
}


// For Tracker, which loads this add-on into its own program: it hands over the picture of the screen (this takes it)...
extern "C" _EXPORT void
snake_set_menu_backdrop(BBitmap* screen, bool (*eligible)(BMenu*))
{
	BAutolock lock(BackdropLock());
	BackdropState& state = Backdrop();
	delete state.bitmap;
	state.bitmap = screen;
	state.eligible = eligible;
	state.time = system_time();
	state.root = NULL;
}


// The Deskbar draws its own panel in the same colours: the pill, the lit row, the dark box for a program's windows, the
// text on the pill, and the text on the box.
extern "C" _EXPORT void
snake_pill_colors(rgb_color* fill, rgb_color* hot, rgb_color* box, rgb_color* text, rgb_color* boxText)
{
	rgb_color f, h, t;
	PillColors(&f, &h, &t);
	if (fill != NULL)
		*fill = f;
	if (hot != NULL)
		*hot = h;
	if (box != NULL)
		*box = MixColors(Accent(), make_color(0, 0, 0), 0.38f);
	if (text != NULL)
		*text = t;
	if (boxText != NULL)
		*boxText = make_color(245, 243, 252);
}


static inline uint8 Shaded(uint8 c, float s);
static void RaisedShade(const std::vector<float>& lit, int w, int h, std::vector<float>& out, bool edgeLeft, bool edgeRight,
	int edgeFrom, bool edgeTop, bool edgeBottom, int openFrom, int openTo);


// The Deskbar draws its block at the top (behind the leaf) the way a lit row of the trail is: the selector colour, rounded
// and shaded like a raised bar. `round` says which corners are round (1 top left, 2 top right, 4 bottom right, 8 bottom
// left) and `closed` which sides are edges of the shape (1 left, 2 right, 4 top, 8 bottom) as opposed to running on.
extern "C" _EXPORT void
snake_draw_raised(BView* view, BRect rect, float radius, uint32 round, uint32 closed, int openFrom, int openTo)
{
	const int w = (int)rect.Width() + 1, h = (int)rect.Height() + 1;
	if (view == NULL || w < 2 || h < 2)
		return;
	BBitmap bitmap(BRect(0, 0, w - 1, h - 1), B_RGBA32);
	if (bitmap.InitCheck() != B_OK)
		return;
	rgb_color fill, hot, text;
	PillColors(&fill, &hot, &text);
	std::vector<float> lit((size_t)w * h), shade;
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x)
			lit[(size_t)y * w + x] = RoundRectCoverage(x + 0.5f, y + 0.5f, 0, 0, (float)w, (float)h,
				(round & 1) ? radius : 0.0f, (round & 2) ? radius : 0.0f, (round & 4) ? radius : 0.0f,
				(round & 8) ? radius : 0.0f);
	}
	RaisedShade(lit, w, h, shade, (closed & 1) != 0, (closed & 2) != 0, 0, (closed & 4) != 0, (closed & 8) != 0, openFrom, openTo);
	uint8* bits = (uint8*)bitmap.Bits();
	const int32 bpr = bitmap.BytesPerRow();
	for (int y = 0; y < h; ++y) {
		uint8* row = bits + y * bpr;
		for (int x = 0; x < w; ++x) {
			const float s = shade[(size_t)y * w + x];
			uint8* px = row + x * 4;
			px[0] = Shaded(hot.blue, s);
			px[1] = Shaded(hot.green, s);
			px[2] = Shaded(hot.red, s);
			px[3] = (uint8)lroundf(lit[(size_t)y * w + x] * 255.0f);
		}
	}
	view->PushState();
	view->SetDrawingMode(B_OP_ALPHA);
	view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	view->DrawBitmap(&bitmap, rect.LeftTop());
	view->PopState();
}


// ...and asks whether the arrow of one of its items is under the trail (so it is not drawn).
extern "C" _EXPORT bool
snake_arrow_hidden(BMenu* menu, BMenuItem* item)
{
	if (menu == NULL || item == NULL)
		return false;
	TrailBar bar;
	{
		BAutolock lock(LinkLock());
		std::map<BMenu*, TrailBar>::iterator it = TrailBars().find(menu);
		if (it == TrailBars().end())
			return false;
		bar = it->second;
	}
	const BRect frame = item->Frame();
	const float middle = (frame.top + frame.bottom) / 2;
	return bar.valid && bar.onRight && middle >= bar.top && middle <= bar.bottom;
}


// ...and asks whether a menu is drawn in the pill look, and with what colours (for the text of its items).
extern "C" _EXPORT bool
snake_pill_menu(BMenu* menu, rgb_color* fill, rgb_color* text)
{
	if (!PillMenu(menu))
		return false;
	rgb_color hot;
	rgb_color f, t;
	PillColors(&f, &hot, &t);
	if (fill != NULL)
		*fill = f;
	if (text != NULL)
		*text = t;
	return true;
}


// Raises the lit trail like a round bar (a quarter circle across its width, lit from the top left), for a touch of depth.
// `lit` is the coverage of the lit shapes (0..1); the result at each pixel is how much lighter (+) or darker (-) than flat
// it is, about -0.3..0.3.
static inline uint8
Shaded(uint8 c, float s)
{
	float v = s >= 0.0f ? c + (255.0f - c) * s : c * (1.0f + s);
	return (uint8)std::max(0.0f, std::min(255.0f, v + 0.5f));
}


static inline float
HeightAt(const std::vector<float>& map, int w, int h, int x, int y)
{
	x = std::max(0, std::min(w - 1, x));
	y = std::max(0, std::min(h - 1, y));
	return map[(size_t)y * w + x];
}


static void
RaisedShade(const std::vector<float>& lit, int w, int h, std::vector<float>& out, bool edgeLeft = false,
	bool edgeRight = false, int edgeFrom = 0, bool edgeTop = false, bool edgeBottom = false, int openFrom = 0, int openTo = -1)
{
	out.assign((size_t)w * h, 0.0f);
	// distance from each lit pixel to the nearest edge of the lit shape (the window's own edges are not edges)
	const size_t n = (size_t)w * h;
	std::vector<float> d(n, 0.0f);
	const float big = 1e4f;
	// (an edge of the window that is the menu's own edge, against the Deskbar, counts as an edge of the shape)
	for (size_t k = 0; k < n; ++k) {
		const int x = (int)(k % w);
		const int y = (int)(k / w);
		const bool atEdge = (y >= edgeFrom && ((edgeLeft && x == 0) || (edgeRight && x == w - 1)))
			|| ((edgeTop && y == 0) || (edgeBottom && y == h - 1)) && !(x >= openFrom && x <= openTo);
		d[k] = lit[k] > 0.5f ? (atEdge ? 1.0f : big) : 0.0f;
	}
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x) {
			float& v = d[(size_t)y * w + x];
			if (v == 0.0f) continue;
			if (x > 0) v = std::min(v, d[(size_t)y * w + x - 1] + 1.0f);
			if (y > 0) v = std::min(v, d[(size_t)(y - 1) * w + x] + 1.0f);
			if (x > 0 && y > 0) v = std::min(v, d[(size_t)(y - 1) * w + x - 1] + 1.4142f);
			if (x + 1 < w && y > 0) v = std::min(v, d[(size_t)(y - 1) * w + x + 1] + 1.4142f);
		}
	for (int y = h - 1; y >= 0; --y)
		for (int x = w - 1; x >= 0; --x) {
			float& v = d[(size_t)y * w + x];
			if (v == 0.0f) continue;
			if (x + 1 < w) v = std::min(v, d[(size_t)y * w + x + 1] + 1.0f);
			if (y + 1 < h) v = std::min(v, d[(size_t)(y + 1) * w + x] + 1.0f);
			if (x + 1 < w && y + 1 < h) v = std::min(v, d[(size_t)(y + 1) * w + x + 1] + 1.4142f);
			if (x > 0 && y + 1 < h) v = std::min(v, d[(size_t)(y + 1) * w + x - 1] + 1.4142f);
		}
	// the height of a round bar: a quarter circle of radius R from the edge in
	const float R = 7.0f;
	std::vector<float> hgt(n, 0.0f);
	for (size_t k = 0; k < n; ++k) {
		if (d[k] <= 0.0f) continue;
		float t = std::min(1.0f, (d[k] - 0.5f) / R);
		if (t < 0.0f) t = 0.0f;
		hgt[k] = R * sqrtf(std::max(0.0f, 1.0f - (1.0f - t) * (1.0f - t)));
	}
	std::vector<float> sm(n, 0.0f);   // a light blur takes the angles out of the distance map
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x) {
			float sum = 0.0f;
			int cnt = 0;
			for (int dy = -1; dy <= 1; ++dy)
				for (int dx = -1; dx <= 1; ++dx) {
					int xx = x + dx, yy = y + dy;
					if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
					sum += hgt[(size_t)yy * w + xx];
					++cnt;
				}
			sm[(size_t)y * w + x] = sum / cnt;
		}
	// light from the top left; the result is how much lighter (+) or darker (-) than flat each pixel is
	const float lx = -0.45f, ly = -0.75f, lz = 0.55f;
	const float ln = sqrtf(lx * lx + ly * ly + lz * lz);
	const float strength = 0.55f;
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x) {
			size_t k = (size_t)y * w + x;
			if (lit[k] <= 0.0f) continue;
			float gx = (HeightAt(sm, w, h, x + 1, y) - HeightAt(sm, w, h, x - 1, y)) * 0.5f;
			float gy = (HeightAt(sm, w, h, x, y + 1) - HeightAt(sm, w, h, x, y - 1)) * 0.5f;
			float nn = sqrtf(gx * gx + gy * gy + 1.0f);
			float dot = (-gx * lx - gy * ly + lz) / (nn * ln);
			out[k] = (dot - lz / ln) * strength * lit[k];
		}
}


// A submenu that is drawn while its parent's window is busy (a big folder takes a while to list, and the parent window
// is locked meanwhile) cannot read where the parent's row is yet, so it is drawn without the trail. It asks to be drawn
// again shortly, a few times, until it can; the parent, which draws its row differently while a submenu is open, is
// drawn again too.
class RefreshFilter : public BMessageFilter {
public:
	RefreshFilter() : BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE, 'SnRf') {}

	virtual filter_result Filter(BMessage* message, BHandler** target)
	{
		BWindow* window = dynamic_cast<BWindow*>(Looper());
		if (window == NULL)
			return B_SKIP_MESSAGE;
		for (int32 i = 0; i < window->CountChildren(); i++) {
			BMenu* menu = dynamic_cast<BMenu*>(window->ChildAt(i));
			if (menu == NULL)
				continue;
			menu->Invalidate();
			BMenu* parent = menu->Supermenu();
			if (parent != NULL && parent->Window() != NULL && parent->Window()->LockWithTimeout(0) == B_OK) {
				parent->Invalidate();
				parent->Window()->Unlock();
			}
		}
		return B_SKIP_MESSAGE;
	}
};


static bool
HasRefreshFilter(BWindow* window)
{
	BList* filters = window->CommonFilterList();
	if (filters == NULL)
		return false;
	for (int32 i = 0; i < filters->CountItems(); i++) {
		if (dynamic_cast<RefreshFilter*>((BMessageFilter*)filters->ItemAt(i)) != NULL)
			return true;
	}
	return false;
}


static std::map<BMenu*, int>&
LinkRetries()
{
	static std::map<BMenu*, int>* retries = new std::map<BMenu*, int>();
	return *retries;
}


static void
DrawTrail(BMenu* menu, const BRect& updateRect, bool pill = false)
{
	BRect bounds = menu->Bounds();
	const int w = (int)bounds.Width() + 1, h = (int)bounds.Height() + 1;
	if (w < 8 || h < 8 || menu->Window() == NULL)
		return;
	const float vt = bounds.top;	// view y of the bitmap's first row

	// The pill look (a captured screen behind the menu, see PillMenu()): one solid pill per menu level, the trail's
	// bends all with the radius of the pill's round ends (half a row's height, less its two outer pixels), and
	// no tabs or strips in windows of their own.
	float pillR = 0;
	{
		BAutolock lock(LinkLock());
		TrailBars().erase(menu);
	}
	if (pill) {
		for (int32 i = 0; i < menu->CountItems(); i++) {
			BMenuItem* it = menu->ItemAt(i);
			if (dynamic_cast<BSeparatorItem*>(it) == NULL) {
				pillR = floorf((it->Frame().Height() + 1.0f - 2.0f) / 2.0f);
				break;
			}
		}
		if (pillR < 3.0f)
			pillR = 3.0f;
		HideBulge(menu->Window());
		HideVBulge(menu->Window());
	}
	bool bridged = false;
	const float selR = pill ? pillR : kSelR, barW = pill ? pillR : kBarW, barR = pill ? pillR : kBarR;
	const float filletR = pill ? pillR : kFilletR;
	// (in the pill look a row that is a pixel or two off its parent's row still counts as level with it)
	const float tol = pill ? 2.5f : 0.5f;

	const bool trailOn = TrailEnabled();
	BMenuItem* selected = SelectedItem(menu);
	const bool active = selected != NULL && (selected->IsEnabled() || selected->Submenu() != NULL);
	BMenu* child = active ? selected->Submenu() : NULL;
	const bool open = child != NULL && child->Window() != NULL && trailOn;
	const float myLeft = menu->Window()->Frame().left;
	const bool childOnRight = open && child->Window()->Frame().left > myLeft;

	// While this menu is drawn its window is locked by this thread, so it can say where its open row is: the submenu,
	// which cannot always read that from its own thread (a big folder is drawn while this window is busy), falls back on it.
	if (open) {
		BRect row = menu->ConvertToScreen(selected->Frame());
		ParentLink fromParent;
		fromParent.valid = true;
		fromParent.rowTop = row.top;
		fromParent.rowBottom = row.bottom + 1;
		fromParent.windowLeft = menu->Window()->Frame().left;
		fromParent.windowRight = menu->Window()->Frame().right;
		fromParent.parentTop = menu->Window()->Frame().top;
		fromParent.parentBottom = menu->Window()->Frame().bottom;
		BAutolock lock(LinkLock());
		Links()[child] = fromParent;
	}

	ParentLink link;
	if (trailOn)
		link = FindParentLink(menu);
	if (trailOn && menu->Supermenu() != NULL && menu->Superitem() != NULL
		&& dynamic_cast<BMenuBar*>(menu->Supermenu()) == NULL) {
		BAutolock lock(LinkLock());
		int& tries = LinkRetries()[menu];
		if (link.valid)
			tries = 0;
		else if (++tries <= 30) {
			BWindow* window = menu->Window();
			if (!HasRefreshFilter(window))
				window->AddCommonFilter(new RefreshFilter());
			BMessage again('SnRf');
			new BMessageRunner(BMessenger(window), &again, 80000, 1);	// (one shot; it goes with the window)
		}
	}
	// the trail starts at the Deskbar for its leaf menu: a parent row that sits at the menu's top
	bool deskbarRoot = false, deskbarBeside = false, deskbarAbove = false;
	BRect deskbarLeaf;
	if (trailOn && !link.valid) {
		bool stripOnLeft = false;
		DeskbarPlace place = FindDeskbarPlace(menu, &stripOnLeft, &deskbarLeaf);
		if (place != kNotDeskbar) {
			deskbarRoot = true;
			deskbarBeside = place == kBesideDeskbar;
			deskbarAbove = place == kAboveLeaf;
			if (pill && deskbarBeside) {
				// the menu opens lower than the Deskbar's block; it is lifted three pixels and moved a pixel over the Deskbar's edge, once for each time
				// it is shown (a window that has already been lifted is at the place it was left)
				static std::map<BWindow*, float>* lifted = new std::map<BWindow*, float>();
				BWindow* window = menu->Window();
				const float top = window->Frame().top;
				BAutolock lock(LinkLock());
				std::map<BWindow*, float>::iterator it = lifted->find(window);
				if (it == lifted->end() || it->second != top) {
					window->MoveBy(stripOnLeft ? -1 : 1, -3);
					(*lifted)[window] = top - 3;
				}
			}
			link.valid = true;
			link.windowLeft = stripOnLeft ? myLeft - 1 : myLeft + 1;
			link.rowTop = menu->Window()->Frame().top;
			link.rowBottom = link.rowTop + 1;
		}
	}
	if (pill && link.valid && !deskbarRoot) {
		OverlapParent(menu, link);
		EnsureOverlapFilter(menu->Window());
	}
	const bool parentOnLeft = link.valid && link.windowLeft < myLeft;
	// the side the selected row's bulge sticks out of: away from the neighbouring menu
	const bool bulgeRight = link.valid ? parentOnLeft : (open ? !childOnRight : true);

	// cover the window border that runs between this menu and its parent, along the parent's open row
	if (link.valid && !deskbarRoot && !pill) {
		BRect frame = menu->Window()->Frame();
		float x = parentOnLeft ? frame.left - 1 : frame.right + 1;
		PlaceBridge(menu, BRect(x, link.rowTop + 1, x, link.rowBottom - 2), link.parentTop, link.parentBottom, Accent());
	}

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
		if (deskbarRoot) {
			// the "parent's row" is the leaf: at the top of the menu, or at its bottom when it opens upwards
			pTop = deskbarAbove ? (float)h : 0.0f;
			// beside the Deskbar the trail starts halfway up the Deskbar's leaf bar, where it runs into the bar's own
			// colour (the edge between the two windows is covered along the strip), not at the top of the menu
			if (deskbarBeside && !pill && deskbarLeaf.IsValid()) {
				const float y = menu->ConvertFromScreen(BPoint(0, deskbarLeaf.top + floorf(deskbarLeaf.Height() * 0.55f))).y
					- vt;
				pTop = std::max(0.0f, std::min(y, (float)h));
			}
			pBottom = pTop;
		}
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
			bool leftFlush = (childEdge && !childOnRight) || (onTrail && parentOnLeft) || (kBulge && !pill && !bulgeRight);
			bool rightFlush = (childEdge && childOnRight) || (onTrail && !parentOnLeft) || (kBulge && !pill && bulgeRight);
			float x0 = leftFlush ? 0.0f : 3.0f, x1 = rightFlush ? (float)w : w - 3.0f;
			bool expTop = onTrail && top < pTop - tol, expBottom = onTrail && bottom > pBottom + tol;
			float tl = cutTop ? 0.0f
				: ((!leftFlush || (onTrail && parentOnLeft && expTop)) ? selR : 0.0f);
			float tr = cutTop ? 0.0f
				: ((!rightFlush || (onTrail && !parentOnLeft && expTop)) ? selR : 0.0f);
			float br = cutBottom ? 0.0f
				: ((!rightFlush || (onTrail && !parentOnLeft && expBottom)) ? selR : 0.0f);
			float bl = cutBottom ? 0.0f
				: ((!leftFlush || (onTrail && parentOnLeft && expBottom)) ? selR : 0.0f);
			Piece p = {x0, t, x1 - x0, b - t, tl, tr, br, bl};
			pieces.push_back(p);
		}
	}

	// the bulge: a tab of the selector outside the edge away from the neighbouring menu
	if (kBulge && !pill) {
		BRect row;
		if (hasOwn && ownBottom > ownTop && ownTop >= 0 && ownBottom <= h)
			row = menu->ConvertToScreen(selected->Frame());
		if (row.IsValid()) {
			BRect frame = menu->Window()->Frame();
			float x = bulgeRight ? frame.right + 1 : frame.left - kBulgeW;
			PlaceBulge(menu, BRect(x, row.top + 1, x + kBulgeW - 1, row.bottom - 1), bulgeRight);
		} else
			HideBulge(menu->Window());
	}

	// the window border between the Deskbar and its menu, covered along the trail's strip
	if (deskbarRoot && deskbarBeside && !pill) {
		BRect frame = menu->Window()->Frame();
		float x = parentOnLeft ? frame.left - 1 : frame.right + 1;
		if (hasOwn && ownBottom > ownTop) {
			float bottom = menu->ConvertToScreen(BPoint(0, ownBottom + vt)).y - 1;
			float top = menu->ConvertToScreen(BPoint(0, std::min(pTop, ownTop) + vt)).y;
			PlaceBridge(menu, BRect(x, std::max(frame.top, top), x, std::min(bottom, frame.bottom)), frame.top, frame.bottom,
				Accent());
		} else
			HideBridge(menu->Window());
	}

	// below or above the leaf (a horizontal Deskbar) the border between the leaf's block and the menu is covered along the
	// snake's bar, so the snake runs into the block
	if (pill && deskbarRoot && !deskbarBeside) {
		BRect frame = menu->Window()->Frame();
		rgb_color fillColor, hotColor, textUnused;
		PillColors(&fillColor, &hotColor, &textUnused);
		const float x0 = parentOnLeft ? frame.left : frame.right - barW + 1;
		if (deskbarAbove)
			PlaceCover(menu, BottomCovers(), BRect(x0, frame.bottom + 1, x0 + barW - 1, frame.bottom + 1), hotColor);
		else
			PlaceCover(menu, TopCovers(), BRect(x0, frame.top - 1, x0 + barW - 1, frame.top - 1), hotColor);
		bridged = true;
	}

	// beside the Deskbar the window border between the menu and the Deskbar is covered along the Deskbar's leaf, so the
	// snake runs into it
	if (pill && deskbarRoot && deskbarBeside && deskbarLeaf.IsValid()) {
		BRect frame = menu->Window()->Frame();
		const float x = parentOnLeft ? frame.left - 1 : frame.right + 1;
		float y1 = std::min(frame.bottom, deskbarLeaf.bottom);
		if (hasOwn && ownBottom > ownTop) {
			// (on down to the lit row, along the whole of the trail's bar)
			y1 = std::max(y1, std::min(frame.bottom, menu->ConvertToScreen(BPoint(0, ownBottom + vt)).y - 1));
		}
		if (y1 > frame.top) {
			rgb_color fillColor, hotColor, textUnused;
			PillColors(&fillColor, &hotColor, &textUnused);
			PlaceBridge(menu, BRect(x, frame.top, x, y1), frame.top, frame.bottom, hotColor);
			bridged = true;
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
			float rt = top < pTop - tol ? selR : 0.0f, rb = bottom > pBottom + tol ? selR : 0.0f;
			// the strip hanging into the parent carries the curve at the free end of the vertical part
			if (kVBulge && !pill) {
				rt = 0.0f;
				rb = 0.0f;
			}
			Piece p;
			if (pill) {
				// where the bar runs past the parent's row its free end (the corner on the parent's side) is round like
				// a pill's end; otherwise the corner beside the lit row is
				const bool extTop = top < pTop - tol, extBottom = bottom > pBottom + tol;
				const float outerT = extTop ? pillR : 0.0f, innerT = extTop ? 0.0f : pillR;
				const float outerB = extBottom ? pillR : 0.0f, innerB = extBottom ? 0.0f : pillR;
				if (parentOnLeft) {
					Piece q = {0, top, barW, bottom - top, outerT, innerT, innerB, outerB};
					p = q;
				} else {
					Piece q = {(float)w - barW, top, barW, bottom - top, innerT, outerT, outerB, innerB};
					p = q;
				}
				// Each menu window has the window system's one pixel border outside its frame. Where the menus overlap it
				// lies over the parent's panel: it is covered by a one pixel window along the submenu's edge (in the
				// pill's colour, and in the trail's colour along the bar, but not along the round ends of a bar that runs
				// past the parent's row, where the join is open anyway), and by one over the top and one over the bottom
				// border where they cross the parent.
				if (!deskbarRoot) {
					BRect frame = menu->Window()->Frame();
					const float x = parentOnLeft ? frame.left - 1 : frame.right + 1;
					float sTop = menu->ConvertToScreen(BPoint(0, top + vt)).y;
					float sBottom = menu->ConvertToScreen(BPoint(0, bottom + vt)).y - 1;
					if (extTop)
						sTop += pillR;
					if (extBottom)
						sBottom -= pillR;
					const float y0 = std::max(frame.top - 1, link.parentTop), y1 = std::min(frame.bottom + 1, link.parentBottom);
					sTop = std::max(sTop, y0);
					sBottom = std::min(sBottom, y1);
					if (y1 > y0 && sBottom > sTop) {
						rgb_color fillColor, hotColor, textUnused;
						PillColors(&fillColor, &hotColor, &textUnused);
						PlaceBridge(menu, BRect(x, y0, x, y1), link.parentTop, link.parentBottom, hotColor, fillColor,
							(int)(sTop - y0), (int)(sBottom - y0));
						bridged = true;

						const float lx = parentOnLeft ? frame.left - 1 : link.windowLeft;
						if (frame.top - 1 >= link.parentTop && frame.top - 1 <= link.parentBottom)
							PlaceCover(menu, TopCovers(), BRect(lx, frame.top - 1, lx + 16, frame.top - 1), fillColor);
						if (frame.bottom + 1 >= link.parentTop && frame.bottom + 1 <= link.parentBottom)
							PlaceCover(menu, BottomCovers(), BRect(lx, frame.bottom + 1, lx + 16, frame.bottom + 1), fillColor);
					}
				}
			} else if (parentOnLeft) {
				Piece q = {0, top, barW, bottom - top, rt, barR, barR, rb};
				p = q;
			} else {
				Piece q = {(float)w - barW, top, barW, bottom - top, barR, rt, rb, barR};
				p = q;
			}
			if (pill) {
				TrailBar bar;
				bar.valid = true;
				bar.onRight = !parentOnLeft;
				bar.top = top + vt;
				bar.bottom = bottom + vt;
				BAutolock lock(LinkLock());
				TrailBars()[menu] = bar;
			}
			pieces.push_back(p);
			if (hasOwn && ownBottom > ownTop) {
				float ex = parentOnLeft ? barW : w - barW;
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

	if (pill && !bridged) {
		HideBridge(menu->Window());
		HideCovers(menu->Window());
	}

	// the vertical part hangs into the parent menu, where it runs past the parent's row
	if (kVBulge && !pill) {
		bool placed = false;
		if (link.valid && !deskbarRoot && hasOwn && ownBottom > ownTop && (ownTop < pTop - 0.5f || ownBottom > pBottom + 0.5f)) {
			float eTop = std::min(ownTop, pTop), eBottom = std::max(ownBottom, pBottom);
			float sy0 = menu->ConvertToScreen(BPoint(0, eTop + vt)).y;
			float sy1 = menu->ConvertToScreen(BPoint(0, eBottom + vt)).y - 1;
			BRect frame = menu->Window()->Frame();
			float x = parentOnLeft ? frame.left - kVBulgeW : frame.right + 1;
			BRect strip(x, sy0, x + kVBulgeW - 1, sy1);
			if (strip.IsValid()) {
				// the free end is rounded only where it hangs over the parent menu's body (elsewhere it hangs over
				// the desktop, and a window can't be see-through)
				bool overParent = ownTop < pTop - 0.5f ? sy0 >= link.parentTop : sy1 <= link.parentBottom;
				PlaceVBulge(menu, strip, !parentOnLeft, ownTop < pTop - 0.5f, ownBottom > pBottom + 0.5f,
					(int)(pTop - eTop), (int)(pBottom - eTop) - 1, overParent);
				placed = true;
			}
		}
		if (!placed)
			HideVBulge(menu->Window());
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
		int x0 = std::max(0, (int)floorf(std::min(f.x, f.x + f.dx * filletR)) - 1);
		int x1 = std::min(w - 1, (int)ceilf(std::max(f.x, f.x + f.dx * filletR)) + 1);
		int y0 = std::max(0, (int)floorf(std::min(f.y, f.y + f.dy * filletR)) - 1);
		int y1 = std::min(h - 1, (int)ceilf(std::max(f.y, f.y + f.dy * filletR)) + 1);
		for (int y = y0; y <= y1; ++y) {
			for (int x = x0; x <= x1; ++x) {
				float c = FilletCoverage(x + 0.5f, y + 0.5f, f.x, f.y, f.dx, f.dy, filletR);
				float& dst = cover[(size_t)y * w + x];
				dst = std::max(dst, c);
			}
		}
	}

	if (pill) {
		// flat, in the Selector Color, clipped to the pill's rounded corners, with no outline or bevel
		bool sqTL, sqTR, sqBR, sqBL;
		SquareCorners(menu, &sqTL, &sqTR, &sqBR, &sqBL);
		BBitmap flat(BRect(0, 0, w - 1, h - 1), B_RGBA32);
		if (flat.InitCheck() != B_OK)
			return;
		rgb_color hot, fillUnused, textUnused;
		PillColors(&fillUnused, &hot, &textUnused);
		uint8* fbits = (uint8*)flat.Bits();
		const int32 fbpr = flat.BytesPerRow();
		std::vector<float> lit((size_t)w * h), shadow;
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				lit[(size_t)y * w + x] = cover[(size_t)y * w + x]
					* RoundRectCoverage(x + 0.5f, y + 0.5f, 0, 0, (float)w, (float)h, sqTL ? 0.0f : pillR,
						sqTR ? 0.0f : pillR, sqBR ? 0.0f : pillR, sqBL ? 0.0f : pillR);
			}
		}
		// (along the Deskbar's leaf the snake runs into the leaf, flat, with no edge between the two)
		int edgeFrom = 0;
		if (deskbarRoot && deskbarBeside && deskbarLeaf.IsValid())
			edgeFrom = std::max(0, (int)ceilf(menu->ConvertFromScreen(BPoint(0, deskbarLeaf.bottom + 1)).y - vt));
		RaisedShade(lit, w, h, shadow, deskbarRoot && parentOnLeft, deskbarRoot && !parentOnLeft, edgeFrom);
		for (int y = 0; y < h; ++y) {
			uint8* row = fbits + y * fbpr;
			for (int x = 0; x < w; ++x) {
				const float c = lit[(size_t)y * w + x];
				const float s = shadow[(size_t)y * w + x];
				uint8* px = row + x * 4;
				px[0] = Shaded(hot.blue, s);  px[1] = Shaded(hot.green, s);  px[2] = Shaded(hot.red, s);
				px[3] = (uint8)lroundf(c * 255.0f);
			}
		}
		menu->PushState();
		menu->SetDrawingMode(B_OP_ALPHA);
		menu->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		menu->DrawBitmap(&flat, bounds.LeftTop());
		menu->PopState();
		return;
	}

	// composite with the bevel: light where the pixel above is empty, dark where the one below is
	BBitmap bitmap(BRect(0, 0, w - 1, h - 1), B_RGBA32);
	if (bitmap.InitCheck() != B_OK)
		return;
	const bool flatFill = FlatFill();
	const rgb_color base = Accent(), light = flatFill ? base : Light(base), dark = flatFill ? base : Dark(base);
	const rgb_color outline = Mix(base, 0, 0.72f);
	uint8* bits = (uint8*)bitmap.Bits();
	const int32 bpr = bitmap.BytesPerRow();
	for (int y = 0; y < h; ++y) {
		uint8* row = bits + y * bpr;
		for (int x = 0; x < w; ++x) {
			float c0 = cover[(size_t)y * w + x];
			uint8* px = row + x * 4;

			// a one pixel dark outline just outside the shape (not along the window's own edges)
			float ol = 0.0f;
			if (c0 < 1.0f) {
				if (x > 0) ol = std::max(ol, cover[(size_t)y * w + x - 1]);
				if (x < w - 1) ol = std::max(ol, cover[(size_t)y * w + x + 1]);
				if (y > 0) ol = std::max(ol, cover[(size_t)(y - 1) * w + x]);
				if (y < h - 1) ol = std::max(ol, cover[(size_t)(y + 1) * w + x]);
				ol *= (1.0f - c0);
			}
			if (c0 <= 0.0f) {
				if (ol <= 0.0f) {
					px[0] = px[1] = px[2] = px[3] = 0;
				} else {
					px[0] = outline.blue;  px[1] = outline.green;  px[2] = outline.red;
					px[3] = (uint8)lroundf(ol * 255.0f);
				}
				continue;
			}
			float above = y > 0 ? cover[(size_t)(y - 1) * w + x] : 0.0f;
			float below = y < h - 1 ? cover[(size_t)(y + 1) * w + x] : 0.0f;
			float a2 = c0 * above, a3 = a2 * below;
			float cr = light.red, cg = light.green, cb = light.blue;
			cr += (dark.red - cr) * a2;  cg += (dark.green - cg) * a2;  cb += (dark.blue - cb) * a2;
			cr += (base.red - cr) * a3;  cg += (base.green - cg) * a3;  cb += (base.blue - cb) * a3;
			float oa = c0 + ol * (1.0f - c0);
			if (ol > 0.0f) {
				float wo = ol * (1.0f - c0);
				cr = (cr * c0 + outline.red * wo) / oa;
				cg = (cg * c0 + outline.green * wo) / oa;
				cb = (cb * c0 + outline.blue * wo) / oa;
			}
			px[0] = (uint8)lroundf(std::min(255.0f, cb));
			px[1] = (uint8)lroundf(std::min(255.0f, cg));
			px[2] = (uint8)lroundf(std::min(255.0f, cr));
			px[3] = (uint8)lroundf(oa * 255.0f);
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
	if (!FlatFill()) {
		view->SetHighColor(Light(accent));
		view->FillRoundRect(r.OffsetByCopy(0, -1), kSelR, kSelR);
		view->SetHighColor(Dark(accent));
		view->FillRoundRect(r.OffsetByCopy(0, 1), kSelR, kSelR);
	}
	view->SetHighColor(accent);
	view->FillRoundRect(r, kSelR, kSelR);
	view->PopState();
}


// #pragma mark - the control look


// Whether the windows are light or dark, from the panel colour they are painted in (a control's own base colour is
// lighter than its panel, so the two can fall on either side of the line)
static bool
LightPanel()
{
	const rgb_color panel = ui_color(B_PANEL_BACKGROUND_COLOR);
	return (panel.red * 299 + panel.green * 587 + panel.blue * 114) / 1000 >= 128;
}


// The recessed fill of check boxes and radio buttons: well below the panel on a light theme (less so on a very
// light one, so it doesn't turn heavy), a little above it on a dark one, with a touch of the accent.
static rgb_color
RecessedFill(const rgb_color& base)
{
	const float lum = (base.red * 299 + base.green * 587 + base.blue * 114) / 1000.0f;
	rgb_color fill;
	if (lum >= 128) {
		const float t = std::min(1.0f, (lum - 128) / 127.0f);
		const float factor = 0.62f + 0.18f * t;
		fill = make_color((uint8)(base.red * factor), (uint8)(base.green * factor), (uint8)(base.blue * factor));
	} else
		fill = MixColors(base, make_color(255, 255, 255), 0.14f);
	return MixColors(fill, Accent(), 0.07f);
}


// A rounded block in the accent, lit like a cylinder across its short side, rasterised with its own alpha so the
// corners show whatever is behind it (the bar of a slider). `rect` is the area it has; `radius` its corner radius.
static void
DrawAccentBlock(BView* view, const BRect& rect, float radius, bool vertical, bool muted = false,
	const rgb_color* tone = NULL, const BRect* clip = NULL)
{
	const int32 width = (int32)rect.Width() + 1, height = (int32)rect.Height() + 1;
	if (width < 4 || height < 4)
		return;
	BBitmap bitmap(BRect(0, 0, width - 1, height - 1), B_RGBA32);
	if (bitmap.InitCheck() != B_OK)
		return;

	rgb_color accent = tone != NULL ? *tone : Accent();
	if (muted) {
		// disabled: the same block, washed out towards grey
		uint8 grey = (uint8)((accent.red + accent.green + accent.blue) / 3);
		accent = MixColors(accent, make_color(grey, grey, grey), 0.72f);
	}
	const rgb_color edge = Mix(accent, 0, 0.2f);
	const rgb_color middle = Mix(accent, 255, 0.45f);
	const rgb_color outline = Outline(accent);
	const float w = width, h = height;
	radius = std::min(radius, std::min(w, h) / 2);

	struct Shape {
		static bool Contains(float x, float y, float w, float h, float r, float inset)
		{
			x -= inset;
			y -= inset;
			float sw = w - 2 * inset, sh = h - 2 * inset, sr = std::max(0.0f, r - inset);
			if (x < 0 || y < 0 || x > sw || y > sh)
				return false;
			float cx = x < sr ? sr : (x > sw - sr ? sw - sr : x);
			float cy = y < sr ? sr : (y > sh - sr ? sh - sr : y);
			return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= sr * sr;
		}
	};

	uint8* bits = (uint8*)bitmap.Bits();
	for (int32 y = 0; y < height; y++) {
		for (int32 x = 0; x < width; x++) {
			float cr = 0, cg = 0, cb = 0;
			int inside = 0;
			for (int sy = 0; sy < 4; sy++) {
				for (int sx = 0; sx < 4; sx++) {
					float fx = x + (sx + 0.5f) / 4, fy = y + (sy + 0.5f) / 4;
					if (!Shape::Contains(fx, fy, w, h, radius, 0))
						continue;
					rgb_color c;
					if (!Shape::Contains(fx, fy, w, h, radius, 1)) {
						c = outline;
					} else {
						float t = vertical ? fx / w : fy / h;
						if (t < 0.40f)
							c = MixColors(edge, middle, t / 0.40f);
						else if (t < 0.70f)
							c = MixColors(middle, accent, (t - 0.40f) / 0.30f);
						else
							c = MixColors(accent, edge, (t - 0.70f) / 0.30f);
					}
					cr += c.red;
					cg += c.green;
					cb += c.blue;
					inside++;
				}
			}
			uint8* p = bits + y * bitmap.BytesPerRow() + x * 4;
			if (inside == 0) {
				p[0] = p[1] = p[2] = p[3] = 0;
			} else {
				p[0] = (uint8)(cb / inside);
				p[1] = (uint8)(cg / inside);
				p[2] = (uint8)(cr / inside);
				p[3] = (uint8)(inside * 255 / 16);
			}
		}
	}

	view->PushState();
	view->ClipToRect(clip != NULL ? *clip : rect);
	view->SetDrawingMode(B_OP_ALPHA);
	view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	view->DrawBitmap(&bitmap, rect.LeftTop());
	view->PopState();
}


// A check mark drawn as one curved stroke of the same width all the way, round at both ends: the short arm and
// the long arm alike, the long one running out of the top right corner of the box (box: the check box's square).
// It has a soft shadow behind it: two copies of the stroke, offset, fainter the further they are.
static void
DrawTick(BView* view, const BRect& box, rgb_color color)
{
	const float w = box.Width(), h = box.Height();
	static const float kOffset[3][3] = {{2.0f, 2.4f, 36}, {1.2f, 1.6f, 90}, {0.0f, 0.0f, 255}};
	for (int pass = 0; pass < 3; pass++) {
		const float dx = kOffset[pass][0], dy = kOffset[pass][1];
#define TICK_POINT(x, y) BPoint(box.left + (x) * w + dx, box.top + (y) * h + dy)
		BShape line;
		line.MoveTo(TICK_POINT(0.16f, 0.56f));
		line.BezierTo(TICK_POINT(0.24f, 0.64f), TICK_POINT(0.32f, 0.78f), TICK_POINT(0.40f, 0.86f));
		line.BezierTo(TICK_POINT(0.58f, 0.50f), TICK_POINT(0.90f, 0.06f), TICK_POINT(1.22f, -0.22f));
#undef TICK_POINT
		view->PushState();
		view->SetDrawingMode(B_OP_ALPHA);
		view->SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);
		if (pass < 2)
			view->SetHighColor(0, 0, 0, (uint8)kOffset[pass][2]);
		else
			view->SetHighColor(color);
		view->SetLineMode(B_ROUND_CAP, B_ROUND_JOIN);
		view->SetPenSize(std::max(2.0f, w * 0.22f));
		view->MovePenTo(BPoint(0, 0));
		view->StrokeShape(&line);
		view->PopState();
	}
}


// Tracker's list view parts, as they are drawn by the Tracker kit inside other programs (a file panel): the
// column titles and the item count. They are told by the names of their views. Tracker itself asks for them with
// B_FLAT and colours their text to suit; here the text is the panel's own, so the bar takes a shade of the accent
// that it reads on (darker behind light text, lighter behind dark text).
static bool
IsTrackerHeader(BView* view)
{
	const char* name = view->Name();
	return name != NULL && strcmp(name, "TitleView") == 0;
}


// The header of a column list (BColumnListView), which Haiku's applications use (Repositories, Sounds, HaikuDepot):
// it draws its columns one cell at a time.
static bool
IsColumnListHeader(BView* view)
{
	const char* name = view->Name();
	return name != NULL && strcmp(name, "title_view") == 0;
}


static bool
IsTrackerCount(BView* view)
{
	const char* name = view->Name();
	return name != NULL && strcmp(name, "CountVw") == 0;
}


static rgb_color
ToneForText(const rgb_color& text)
{
	const bool lightText = (text.red * 299 + text.green * 587 + text.blue * 114) / 1000 >= 128;
	const rgb_color accent = Accent();
	return lightText ? MixColors(Dark(accent), accent, 0.35f) : Mix(accent, 255, 0.4f);
}


static rgb_color
BarToneForText()
{
	return ToneForText(ui_color(B_PANEL_TEXT_COLOR));
}


// A drop-down field: the accent pill with a divider and a chevron on the right, in the shade that suits the
// menu text the application draws on it.
// The colour a view sits on: that of the nearest view up the tree that has one of its own (a control's own colour is
// its control background, which is lighter than the window it is in)
static rgb_color
SurroundColor(BView* view, const rgb_color& fallback)
{
	for (BView* parent = view->Parent(); parent != NULL; parent = parent->Parent()) {
		const rgb_color color = parent->ViewColor();
		if (color != B_TRANSPARENT_COLOR)
			return color;
	}
	return fallback;
}


static void
DrawFieldPill(BView* view, const BRect& rect, const rgb_color& base, uint32 flags, bool popupIndicator)
{
	const bool disabled = (flags & BControlLook::B_DISABLED) != 0;
	const rgb_color text = ui_color(B_MENU_ITEM_TEXT_COLOR);
	rgb_color tone = ToneForText(text);
	if ((flags & BControlLook::B_HOVER) != 0 && !disabled)
		tone = Mix(tone, 255, 0.1f);

	// the panel colour under it, as the pill's rounded ends are partly see-through
	view->PushState();
	view->ClipToRect(rect);
	view->SetDrawingMode(B_OP_COPY);
	view->SetHighColor(SurroundColor(view, base));
	view->FillRect(rect);
	view->PopState();

	BRect pill = rect;
	pill.InsetBy(0, 0);
	DrawAccentBlock(view, pill, pill.Height() / 2 + 1, false, disabled, &tone);

	if (!popupIndicator)
		return;

	// the divider and the chevron, in the menu text colour
	const float indicator = std::max(14.0f, pill.Height() * 0.9f);
	const float dividerX = pill.right - indicator;
	const float midY = (pill.top + pill.bottom) / 2;
	rgb_color mark = text;
	if (disabled)
		mark = MixColors(tone, text, 0.4f);
	view->PushState();
	view->SetDrawingMode(B_OP_ALPHA);
	view->SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);
	view->SetHighColor(mark.red, mark.green, mark.blue, 70);
	view->StrokeLine(BPoint(dividerX, pill.top + 3), BPoint(dividerX, pill.bottom - 3));
	view->SetHighColor(mark.red, mark.green, mark.blue, 255);
	view->SetLineMode(B_ROUND_CAP, B_ROUND_JOIN);
	view->SetPenSize(1.6f);
	const float cx = dividerX + (pill.right - dividerX) / 2 - 1;
	view->MovePenTo(BPoint(0, 0));
	BShape chevron;
	chevron.MoveTo(BPoint(cx - 3.5f, midY - 1.5f));
	chevron.LineTo(BPoint(cx, midY + 2.0f));
	chevron.LineTo(BPoint(cx + 3.5f, midY - 1.5f));
	view->StrokeShape(&chevron);
	view->PopState();
}


class SnakeControlLook : public HaikuControlLook {
public:
	SnakeControlLook() {}
	virtual ~SnakeControlLook() {}

	virtual	void DrawMenuBackground(BView* view, BRect& rect, const BRect& updateRect,
		const rgb_color& base, uint32 flags = 0, uint32 borders = B_ALL_BORDERS)
	{
		BMenu* menu = dynamic_cast<BMenu*>(view);
		if (menu != NULL && PillMenu(menu)) {
			DrawPillBase(menu);
			DrawTrail(menu, updateRect, true);
			return;
		}

		HaikuControlLook::DrawMenuBackground(view, rect, updateRect, base, flags, borders);

		if (menu != NULL && dynamic_cast<BMenuBar*>(menu) == NULL)
			DrawTrail(menu, updateRect);
	}

	// BScrollBar draws itself disabled both when there is nothing to scroll and when its window is not
	// active; the two are told apart by B_PARTIALLY_ACTIVATED.
	static bool NothingToScroll(uint32 flags)
	{
		return (flags & B_DISABLED) != 0 && (flags & B_PARTIALLY_ACTIVATED) == 0;
	}

	// The scroll bar's thumb: a pill in the accent colour, lit down its middle, instead of a
	// bevelled button. The track is drawn in two pieces either side of the thumb, so the corners
	// the pill leaves open are filled with the track's own colour first.
	virtual	void DrawScrollBarThumb(BView* view, BRect& rect, const BRect& updateRect,
		const rgb_color& base, uint32 flags, orientation orientation, uint32 knobStyle = B_KNOB_NONE)
	{
		if (!rect.IsValid()) {
			HaikuControlLook::DrawScrollBarThumb(view, rect, updateRect, base, flags, orientation,
				knobStyle);
			return;
		}
		if (!ShouldDraw(view, rect, updateRect))
			return;
		if (NothingToScroll(flags)) {
			// the thumb would fill the whole bar: an empty track instead, to match the rest
			view->PushState();
			view->ClipToRect(rect);
			view->SetDrawingMode(B_OP_COPY);
			view->SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), 1.075f));
			view->FillRect(rect);
			view->PopState();
			return;
		}

		rgb_color accent = Accent();
		if ((flags & B_DISABLED) != 0) {
			// the window is not active: the same pill, washed out
			uint8 grey = (uint8)((accent.red + accent.green + accent.blue) / 3);
			accent = MixColors(accent, make_color(grey, grey, grey), 0.7f);
		}
		rgb_color track = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), 1.075f);
		BRect pill = rect;
		if (orientation == B_VERTICAL)
			pill.InsetBy(2, 1);
		else
			pill.InsetBy(1, 2);
		view->PushState();
		view->ClipToRect(rect);

		// Rasterised by hand over the track colour, so the edge is smooth and has no coloured fringe.
		const bool vertical = orientation == B_VERTICAL;
		const int32 width = (int32)rect.Width() + 1, height = (int32)rect.Height() + 1;
		BBitmap bitmap(BRect(0, 0, width - 1, height - 1), B_RGBA32);
		if (bitmap.InitCheck() == B_OK) {
			const float px0 = pill.left - rect.left, py0 = pill.top - rect.top;
			const float pw = pill.Width() + 1, ph = pill.Height() + 1;
			const float radius = (vertical ? pw : ph) / 2;
			rgb_color edge = Mix(accent, 0, 0.2f);
			rgb_color middle = Mix(accent, 255, 0.45f);
			rgb_color outline = Outline(accent);
			// inside a stadium (rounded rect, fully rounded ends) at the given inset
			struct Shape {
				static bool Contains(float x, float y, float x0, float y0, float w, float h,
					float r, float inset)
				{
					x -= x0 + inset;
					y -= y0 + inset;
					float sw = w - 2 * inset, sh = h - 2 * inset, sr = r - inset;
					if (x < 0 || y < 0 || x > sw || y > sh)
						return false;
					float cx = x < sr ? sr : (x > sw - sr ? sw - sr : x);
					float cy = y < sr ? sr : (y > sh - sr ? sh - sr : y);
					return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= sr * sr;
				}
			};
			uint8* bits = (uint8*)bitmap.Bits();
			for (int32 y = 0; y < height; y++) {
				for (int32 x = 0; x < width; x++) {
					float r = 0, g = 0, b = 0;
					for (int sy = 0; sy < 4; sy++) {
						for (int sx = 0; sx < 4; sx++) {
							float fx = x + (sx + 0.5f) / 4, fy = y + (sy + 0.5f) / 4;
							rgb_color c = track;
							if (Shape::Contains(fx, fy, px0, py0, pw, ph, radius, 0)) {
								if (!Shape::Contains(fx, fy, px0, py0, pw, ph, radius, 1)) {
									c = outline;
								} else {
									float t = vertical ? (fx - px0) / pw : (fy - py0) / ph;
									if (t < 0.47f)
										c = MixColors(edge, middle, t / 0.47f);
									else if (t < 0.745f)
										c = MixColors(middle, accent, (t - 0.47f) / 0.275f);
									else
										c = MixColors(accent, edge, (t - 0.745f) / 0.255f);
								}
							}
							r += c.red;
							g += c.green;
							b += c.blue;
						}
					}
					uint8* p = bits + y * bitmap.BytesPerRow() + x * 4;
					p[0] = (uint8)(b / 16);
					p[1] = (uint8)(g / 16);
					p[2] = (uint8)(r / 16);
					p[3] = 255;
				}
			}
			view->SetDrawingMode(B_OP_ALPHA);
			view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
			view->DrawBitmap(&bitmap, rect.LeftTop());
		}
		view->PopState();
	}

	// With "Scroll bar arrows" off in Tracker's preferences the arrow buttons are drawn as plain track. The scroll
	// bar still keeps their room and they still scroll when clicked; they just can't be seen.
	virtual	void DrawScrollBarButton(BView* view, BRect rect, const BRect& updateRect, const rgb_color& base,
		const rgb_color& text, uint32 flags, int32 direction, orientation orientation, bool down = false)
	{
		if (ShowArrows()) {
			HaikuControlLook::DrawScrollBarButton(view, rect, updateRect, base, text, flags, direction,
				orientation, down);
			return;
		}
		if (!ShouldDraw(view, rect, updateRect))
			return;
		view->PushState();
		view->ClipToRect(rect);
		view->SetDrawingMode(B_OP_COPY);
		view->SetHighColor(tint_color(base, 1.075f));
		view->FillRect(rect);
		view->PopState();
	}

	// A flat track, with none of the stock look's edge lines at the ends of each piece: the thumb
	// sits in it as one pill.
	virtual	void DrawScrollBarBackground(BView* view, BRect& rect, const BRect& updateRect,
		const rgb_color& base, uint32 flags, orientation orientation)
	{
		if (!ShouldDraw(view, rect, updateRect))
			return;
		view->PushState();
		view->ClipToRect(rect);
		view->SetDrawingMode(B_OP_COPY);
		view->SetHighColor(tint_color(base, 1.075f));
		view->FillRect(rect);
		view->PopState();
	}

	virtual	void DrawScrollBarBackground(BView* view, BRect& rect1, BRect& rect2,
		const BRect& updateRect, const rgb_color& base, uint32 flags, orientation orientation)
	{
		DrawScrollBarBackground(view, rect1, updateRect, base, flags, orientation);
		DrawScrollBarBackground(view, rect2, updateRect, base, flags, orientation);
	}

	// #pragma mark - the accent in other controls: the focus ring, tabs, check marks and slider fills

	// the mark of a check box or radio button: the accent, dark on a light panel and bright on a dark one,
	// faded towards the panel when disabled or being toggled, the way the stock mark is
	rgb_color AccentMark(const rgb_color& base, rgb_color stockMark, uint32 flags)
	{
		const bool darkPanel = (base.red * 299 + base.green * 587 + base.blue * 114) / 1000 < 128;
		rgb_color mark = darkPanel ? Mix(Accent(), 255, 0.2f) : Dark(Accent());
		if ((flags & B_DISABLED) != 0)
			mark = MixColors(base, mark, 0.4f);
		else if ((flags & B_CLICKED) != 0)
			mark = MixColors(base, mark, 0.7f);
		return mark;
	}

	virtual	void DrawTextControlBorder(BView* view, BRect& rect, const BRect& updateRect,
		const rgb_color& base, uint32 flags = 0, uint32 borders = B_ALL_BORDERS)
	{
		if ((flags & B_FOCUSED) == 0 || (flags & B_DISABLED) != 0 || (flags & B_INVALID) != 0
			|| (flags & B_BLEND_FRAME) != 0) {
			HaikuControlLook::DrawTextControlBorder(view, rect, updateRect, base, flags, borders);
			return;
		}
		if (!ShouldDraw(view, rect, updateRect))
			return;

		// the stock frame, with the accent as the colour of the focus ring
		rgb_color ring = Accent();
		if ((flags & B_CLICKED) != 0) {
			rgb_color border = tint_color(base, 1.50);
			_DrawFrame(view, rect, border, border, tint_color(base, 1.49), tint_color(base, 1.49));
		} else
			_DrawOuterResessedFrame(view, rect, base, flags, borders);
		_DrawFrame(view, rect, ring, ring, ring, ring, borders);
	}

	virtual	void DrawCheckBox(BView* view, BRect& rect, const BRect& updateRect, const rgb_color& base,
		uint32 flags = 0)
	{
		if (!ShouldDraw(view, rect, updateRect))
			return;

		const BRect box = rect;

		// The tick runs out of the box, but a check box only invalidates its box when it changes: so that what
		// is left of the tick outside is cleared (or drawn), the area around the box is invalidated too, once.
		BRect around(box.left - 3, box.top - 6, box.right + 7, box.bottom + 4);
		around = around & view->Bounds();
		if (around.IsValid() && !updateRect.Contains(around))
			view->Invalidate(around);

		rgb_color dark1BorderColor;
		rgb_color dark2BorderColor;
		rgb_color navigationColor = Accent();

		if ((flags & B_DISABLED) != 0) {
			_DrawOuterResessedFrame(view, rect, base, flags);
			dark1BorderColor = tint_color(base, 1.15);
			dark2BorderColor = tint_color(base, 1.15);
		} else if ((flags & B_CLICKED) != 0) {
			dark1BorderColor = tint_color(base, 1.50);
			dark2BorderColor = tint_color(base, 1.48);
			_DrawFrame(view, rect, dark1BorderColor, dark1BorderColor, dark2BorderColor, dark2BorderColor);
			dark2BorderColor = dark1BorderColor;
		} else {
			_DrawOuterResessedFrame(view, rect, base, flags);
			dark1BorderColor = tint_color(base, 1.40);
			dark2BorderColor = tint_color(base, 1.38);
		}

		if ((flags & B_FOCUSED) != 0) {
			dark1BorderColor = navigationColor;
			dark2BorderColor = navigationColor;
		}

		_DrawFrame(view, rect, dark1BorderColor, dark1BorderColor, dark2BorderColor, dark2BorderColor);

		if ((flags & B_DISABLED) != 0)
			_FillGradient(view, rect, base, 0.4, 0.2);
		else {
			// the recessed fill, the same as the radio buttons have
			_FillGradient(view, rect, RecessedFill(base), 1.05, 0.97);
		}

		rgb_color markColor;
		if (_RadioButtonAndCheckBoxMarkColor(base, markColor, flags)) {
			markColor = AccentMark(base, markColor, flags);
			view->PushState();
			view->SetHighColor(markColor);

			BFont font;
			view->GetFont(&font);
			float inset = std::max(2.0f, roundf(font.Size() / 6));
			rect.InsetBy(inset, inset);

			float penSize = std::max(1.0f, ceilf(rect.Width() / 3.5f));
			if (penSize > 1.0f && fmodf(penSize, 2.0f) == 0.0f) {
				rect.right++;
				rect.bottom++;
			}

			view->SetDrawingMode(B_OP_OVER);
			view->SetPenSize(penSize);
			if (flags & B_PARTIALLY_ACTIVATED) {
				float y = (rect.top + rect.bottom) / 2;
				view->StrokeLine(BPoint(rect.left, y), BPoint(rect.right, y));
			} else {
				// a curved tick, drawn from the whole box and not the inset one: it runs out of the top right
				// corner (as far as the control's own area allows)
				DrawTick(view, box, markColor);
			}
			view->PopState();
		}
	}

	virtual	void DrawRadioButton(BView* view, BRect& rect, const BRect& updateRect, const rgb_color& base,
		uint32 flags = 0)
	{
		if (!ShouldDraw(view, rect, updateRect))
			return;

		const bool disabled = (flags & B_DISABLED) != 0;
		const bool darkTheme = !LightPanel();
		const BRect disc = rect;

		// the edge: a dark ring that is heaviest along the bottom, as if the disc were cut into the panel; the
		// accent when the button has the keyboard focus
		rgb_color edge = darkTheme ? make_color(0, 0, 0) : MixColors(base, make_color(0, 0, 0), 0.72f);
		if ((flags & B_FOCUSED) != 0 && !disabled)
			edge = MixColors(Dark(Accent()), edge, 0.2f);
		if (disabled)
			edge = MixColors(base, edge, 0.35f);

		view->PushState();
		view->SetDrawingMode(B_OP_COPY);
		view->SetHighColor(edge);
		view->FillEllipse(disc);

		rgb_color fill = RecessedFill(base);
		if (disabled)
			fill = MixColors(base, fill, 0.55f);
		else if ((flags & B_CLICKED) != 0)
			fill = MixColors(fill, make_color(0, 0, 0), 0.12f);
		BRect inner = disc.InsetByCopy(1, 1);
		inner.OffsetBy(0, -0.5f);
		BGradientLinear gradient;
		gradient.AddColor(MixColors(fill, make_color(0, 0, 0), 0.08f), 0);
		gradient.AddColor(MixColors(fill, make_color(255, 255, 255), 0.05f), 255);
		gradient.SetStart(inner.LeftTop());
		gradient.SetEnd(inner.LeftBottom());
		view->FillEllipse(inner, gradient);

		rgb_color markColor;
		if (_RadioButtonAndCheckBoxMarkColor(base, markColor, flags)) {
			// the dot: a third of the disc, in the accent
			const float size = disc.Width() * 0.38f;
			const BPoint center((disc.left + disc.right) / 2, (disc.top + disc.bottom) / 2 - 0.25f);
			view->SetHighColor(AccentMark(base, markColor, flags));
			view->FillEllipse(center, size / 2, size / 2);
		}
		view->PopState();
	}

	// the filled part of a slider's bar is the accent
	virtual	void DrawSliderBar(BView* view, BRect rect, const BRect& updateRect, const rgb_color& base,
		rgb_color leftFillColor, rgb_color rightFillColor, float sliderScale, uint32 flags,
		orientation orientation)
	{
		const rgb_color highlight = ui_color(B_CONTROL_HIGHLIGHT_COLOR);
		// a slider that names no fill colour of its own has the same on both sides of the thumb: the part before
		// the thumb becomes the accent
		rgb_color fill = Accent();
		if ((flags & B_DISABLED) != 0) {
			uint8 grey = (uint8)((fill.red + fill.green + fill.blue) / 3);
			fill = MixColors(fill, make_color(grey, grey, grey), 0.72f);
		}
		if (leftFillColor == rightFillColor || leftFillColor == highlight)
			leftFillColor = fill;
		if (rightFillColor == highlight)
			rightFillColor = Accent();
		HaikuControlLook::DrawSliderBar(view, rect, updateRect, base, leftFillColor, rightFillColor,
			sliderScale, flags, orientation);
	}

	virtual	void DrawSliderBar(BView* view, BRect rect, const BRect& updateRect, const rgb_color& base,
		rgb_color fillColor, uint32 flags, orientation orientation)
	{
		if (fillColor == ui_color(B_CONTROL_HIGHLIGHT_COLOR))
			fillColor = Accent();
		HaikuControlLook::DrawSliderBar(view, rect, updateRect, base, fillColor, flags, orientation);
	}

	// progress bars are filled in the accent where the caller asked for the stock colour
	virtual	void DrawStatusBar(BView* view, BRect& rect, const BRect& updateRect, const rgb_color& base,
		const rgb_color& barColor, float progressPosition)
	{
		rgb_color color = barColor;
		if (color == ui_color(B_STATUS_BAR_COLOR))
			color = Accent();
		HaikuControlLook::DrawStatusBar(view, rect, updateRect, base, color, progressPosition);
	}

	// buttons are rounder: a larger corner radius for real buttons, where the stock one is 4; the small square
	// buttons of a scroll bar keep the stock look
	// a faint wash of the accent in the button's colour (and so in its edges, which the stock look derives from
	// it): a little more when pressed, and for the default button
	static rgb_color ButtonColor(const rgb_color& base, uint32 flags)
	{
		if ((flags & (B_FLAT | B_DISABLED)) != 0)
			return base;
		const bool lightPanel = LightPanel();
		float amount = 0.09f;
		if ((flags & B_DEFAULT_BUTTON) != 0)
			amount = lightPanel ? 0.0f : 0.14f;
		if ((flags & (B_ACTIVATED | B_CLICKED)) != 0)
			amount = 0.22f;
		// on a light panel the default button has a dark body, with light text (see DrawLabel)
		if ((flags & B_DEFAULT_BUTTON) != 0 && lightPanel)
			return MixColors(base, Dark(Accent()), (flags & (B_ACTIVATED | B_CLICKED)) != 0 ? 0.75f : 0.62f);
		return MixColors(base, Accent(), amount);
	}

	// the highlights of a real button: a light line along the top inside the edge and a faint gloss over its
	// upper half; the default button also gets a dark accent cap on each side
	// a small, square button (the Tracker window's back, forward and up): a round, solid piece of the accent
	static bool SmallRoundButton(BView* view, const BRect& rect, uint32 flags, uint32 borders)
	{
		return borders == B_ALL_BORDERS && (flags & B_FLAT) == 0 && !PlainButton(view) && rect.Height() >= 16
			&& rect.Width() >= 24 && rect.Width() < 34;
	}

	// One colour, the accent; darker while pressed, washed out when disabled
	void DrawSolidButton(BView* view, const BRect& frame, uint32 flags)
	{
		const bool disabled = (flags & B_DISABLED) != 0;
		const bool pressed = (flags & (B_ACTIVATED | B_CLICKED)) != 0;
		const rgb_color accent = Accent();
		const rgb_color panel = ui_color(B_PANEL_BACKGROUND_COLOR);
		const bool lightTheme = (panel.red * 299 + panel.green * 587 + panel.blue * 114) / 1000 >= 128;
		rgb_color body = lightTheme ? MixColors(Dark(accent), accent, 0.55f) : Mix(accent, 255, 0.16f);
		if (pressed)
			body = Mix(body, 0, 0.2f);
		if (disabled)
			body = MixColors(panel, body, 0.45f);

		view->PushState();
		view->ClipToRect(frame);
		view->SetDrawingMode(B_OP_COPY);
		// (the toolbar's own colour, not the button's)
		view->SetHighColor(SurroundColor(view, ui_color(B_PANEL_BACKGROUND_COLOR)));
		view->FillRect(frame);
		// a circle
		BRect disc = frame.InsetByCopy(1, 1);
		const float d = std::min(disc.Width(), disc.Height());
		disc = BRect(floorf((disc.left + disc.right - d) / 2 + 0.5f), floorf((disc.top + disc.bottom - d) / 2 + 0.5f),
			0, 0).OffsetByCopy(0, 0);
		disc.right = disc.left + d;
		disc.bottom = disc.top + d;
		view->SetHighColor(body);
		view->SetDrawingMode(B_OP_ALPHA);
		view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_COMPOSITE);
		view->SetPenSize(1);
		view->FillEllipse(disc);
		view->PopState();
	}

	void ButtonHighlights(BView* view, const BRect& frame, float radius, uint32 flags)
	{
		const bool disabled = (flags & B_DISABLED) != 0;
		view->PushState();
		view->ClipToRect(frame);
		view->SetDrawingMode(B_OP_ALPHA);
		// (composite, not overlay: Qt draws buttons on an offscreen bitmap with an alpha channel, and overlay leaves
		// some of its pixels see-through, which shows as stripes. On an opaque surface the two look the same.)
		view->SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_COMPOSITE);
		const bool pressed = (flags & (B_ACTIVATED | B_CLICKED)) != 0;
		if (!pressed && !disabled) {
			BRect gloss(frame.left + 2, frame.top + 2, frame.right - 2, frame.top + frame.Height() * 0.45f);
			view->SetHighColor(255, 255, 255, 34);
			view->FillRect(gloss.InsetByCopy(radius * 0.5f, 0));
			view->SetHighColor(255, 255, 255, 120);
			view->StrokeLine(BPoint(frame.left + radius, frame.top + 1), BPoint(frame.right - radius, frame.top + 1));
		}

		// an accent cap on each side of every button, the same on all of them (disabled ones too): a five pixel
		// block of the accent, lit from the top, with a darker line on its outer edge and a light one inside
		const rgb_color accent = Accent();
		// darker on a light theme, lighter on a dark one
		const rgb_color panel = ui_color(B_PANEL_BACKGROUND_COLOR);
		const bool lightTheme = (panel.red * 299 + panel.green * 587 + panel.blue * 114) / 1000 >= 128;
		const rgb_color body = lightTheme ? MixColors(Dark(accent), accent, 0.55f) : Mix(accent, 255, 0.16f);
		const rgb_color rim = lightTheme ? MixColors(Outline(accent), accent, 0.35f)
			: MixColors(Dark(accent), accent, 0.5f);
		// (three pixels on the small, square buttons, so that their picture has room)
		const int capWidth = frame.Width() < 34 ? 3 : 5;
		const float top = frame.top + 2, bottom = frame.bottom - 2;
		if (bottom > top + 4) {
			const uint8 alpha = disabled ? 175 : 255;
			const int rows = (int)(bottom - top) + 1;
			for (int side = 0; side < 2; side++) {
				const float sign = side == 0 ? 1.0f : -1.0f;
				const float edge = side == 0 ? frame.left : frame.right;
				for (int r = 0; r < rows; r++) {
					// lighter at the top, darker at the bottom
					const float t = rows > 1 ? (float)r / (rows - 1) : 0.0f;
					const rgb_color row = t < 0.5f ? MixColors(Mix(body, 255, 0.22f), body, t * 2)
						: MixColors(body, Mix(body, 0, 0.18f), (t - 0.5f) * 2);
					const float y = top + r;
					// the outer corners follow the button's
					const bool corner = r == 0 || r == rows - 1;
					view->SetHighColor(rim.red, rim.green, rim.blue, alpha);
					if (!corner)
						view->FillRect(BRect(edge, y, edge, y));
					view->SetHighColor(row.red, row.green, row.blue, alpha);
					const float x0 = edge + sign * 1, x1 = edge + sign * (capWidth - 1);
					view->FillRect(BRect(std::min(x0, x1), y, std::max(x0, x1), y));
					view->SetHighColor(255, 255, 255, 70);
					view->FillRect(BRect(edge + sign * capWidth, y, edge + sign * capWidth, y));
				}
			}
		}
		view->PopState();
	}

	// Menu bars are the accent pill (a menu bar laid out in a column is not, and neither is the Deskbar's)
	static bool PillMenuBar(BView* view)
	{
		if (dynamic_cast<BMenuBar*>(view) == NULL || PlainButton(view))
			return false;
		const BRect bounds = view->Bounds();
		return bounds.Height() <= 40 && bounds.Width() > bounds.Height();
	}

	// the Deskbar's own bars and buttons keep the stock look
	static bool PlainButton(BView* view)
	{
		// the Deskbar is told by its program, not its window: its leaf bar is drawn in views that have none
		static int inDeskbar = -1;
		if (inDeskbar < 0) {
			app_info info;
			inDeskbar = be_app != NULL && be_app->GetAppInfo(&info) == B_OK
				&& strcmp(info.signature, "application/x-vnd.Be-TSKB") == 0 ? 1 : 0;
		}
		BWindow* window = view->Window();
		// (a window's name is its title with "w>" in front)
		if (window != NULL && window->Name() != NULL
			&& (strcmp(window->Name(), "Deskbar") == 0 || strcmp(window->Name(), "w>Deskbar") == 0)) {
			return true;
		}
		// the leaf bar and its titles are drawn in views that are not in a window; the Deskbar's other windows
		// (its preferences) are drawn as everywhere
		return inDeskbar == 1 && window == NULL;
	}

	virtual	void DrawButtonBackground(BView* view, BRect& rect, const BRect& updateRect, const rgb_color& base,
		uint32 flags = 0, uint32 borders = B_ALL_BORDERS, orientation orientation = B_HORIZONTAL)
	{
		if (PlainButton(view)) {
			HaikuControlLook::DrawButtonBackground(view, rect, updateRect, base, flags, borders, orientation);
			return;
		}

		// Tracker's column titles ask for B_FLAT with a top and a bottom border only: one accent bar, a pill like
		// a scroll bar's thumb (a title being pressed only darkens its part of it)
		// a column list's header: the whole strip is one pill, and each cell draws its part of it
		if (borders == (B_TOP_BORDER | B_BOTTOM_BORDER) && (flags & B_FLAT) == 0 && IsColumnListHeader(view)
			&& rect.Height() >= 10 && rect.Height() <= 36 && ShouldDraw(view, rect, updateRect)) {
			const rgb_color tone = BarToneForText();
			view->PushState();
			view->ClipToRect(rect);
			view->SetDrawingMode(B_OP_COPY);
			view->SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
			view->FillRect(rect);
			view->PopState();
			BRect strip(view->Bounds().left, rect.top + 1, view->Bounds().right, rect.bottom - 1);
			DrawAccentBlock(view, strip, strip.Height() / 2 + 1, false, false, &tone, &rect);
			return;
		}

		const bool foreignHeader = (flags & B_FLAT) == 0 && IsTrackerHeader(view);
		if (borders == (B_TOP_BORDER | B_BOTTOM_BORDER) && ((flags & B_FLAT) != 0 || foreignHeader)
			&& rect.Height() >= 10 && rect.Height() <= 36 && ShouldDraw(view, rect, updateRect)) {
			const rgb_color tone = BarToneForText();
			// outside Tracker the pressed title is told from the bar by being narrower than the view, and the
			// pieces the program redraws are not the whole bar: the whole bar is asked for, once
			const bool pressed = foreignHeader ? rect.Width() < view->Bounds().Width() - 2
				: (flags & B_ACTIVATED) != 0;
			if (foreignHeader && !pressed && !updateRect.Contains(view->Bounds()))
				view->Invalidate();
			if (!pressed) {
				// the header isn't cleared before it is drawn, and the bar's rounded ends are partly see-through:
				// without the panel colour under it, every redraw while the window is resized left more of them
				view->PushState();
				view->ClipToRect(rect);
				view->SetDrawingMode(B_OP_COPY);
				view->SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
				view->FillRect(rect);
				view->PopState();
				BRect bar = rect;
				bar.InsetBy(0, 1);
				DrawAccentBlock(view, bar, bar.Height() / 2 + 1, false, false, foreignHeader ? &tone : NULL);
			} else {
				view->PushState();
				view->ClipToRect(rect);
				view->SetDrawingMode(B_OP_ALPHA);
				view->SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_COMPOSITE);
				view->SetHighColor(0, 0, 0, 38);
				view->FillRect(rect.InsetByCopy(0, 1));
				view->PopState();
			}
			return;
		}

		const rgb_color color = ButtonColor(base, flags);
		if (borders == B_ALL_BORDERS && (flags & B_FLAT) == 0 && rect.Height() >= 16 && rect.Width() >= 24) {
			float radius = std::min(8.0f, std::max(5.0f, floorf(rect.Height() / 3.5f)));
			const BRect frame = rect;
			// a small, square button (the Tracker window's back, forward and up) is one solid piece of the accent
			if (SmallRoundButton(view, rect, flags, borders)) {
				if (ShouldDraw(view, rect, updateRect))
					DrawSolidButton(view, frame, flags);
				rect.InsetBy(2, 2);
				return;
			}
			HaikuControlLook::DrawButtonBackground(view, rect, updateRect, radius, color, flags, borders,
				orientation);
			ButtonHighlights(view, frame, radius, flags);
			return;
		}
		HaikuControlLook::DrawButtonBackground(view, rect, updateRect, color, flags, borders, orientation);
	}

	virtual	void DrawButtonBackground(BView* view, BRect& rect, const BRect& updateRect, float radius,
		const rgb_color& base, uint32 flags = 0, uint32 borders = B_ALL_BORDERS,
		orientation orientation = B_HORIZONTAL)
	{
		HaikuControlLook::DrawButtonBackground(view, rect, updateRect, radius,
			PlainButton(view) ? base : ButtonColor(base, flags), flags, borders, orientation);
	}

	virtual	void DrawButtonBackground(BView* view, BRect& rect, const BRect& updateRect, float leftTopRadius,
		float rightTopRadius, float leftBottomRadius, float rightBottomRadius, const rgb_color& base,
		uint32 flags = 0, uint32 borders = B_ALL_BORDERS, orientation orientation = B_HORIZONTAL)
	{
		HaikuControlLook::DrawButtonBackground(view, rect, updateRect, leftTopRadius, rightTopRadius,
			leftBottomRadius, rightBottomRadius, PlainButton(view) ? base : ButtonColor(base, flags), flags,
			borders, orientation);
	}

	// On a dark theme the stock frame of a default button is a stark light ring (two pixels, with one pixel of the
	// background either side of it); the default button there is drawn like the others, in the room that was left
	// inside the ring, so it keeps the size it has. It is still the one Enter presses.
	static uint32 WithoutDefaultRing(const rgb_color& base, uint32 flags, BRect& rect)
	{
		const bool darkTheme = !LightPanel();
		if (!darkTheme || (flags & B_DEFAULT_BUTTON) == 0 || (flags & B_FLAT) != 0)
			return flags;
		rect.InsetBy(3, 3);
		return flags & ~(uint32)B_DEFAULT_BUTTON;
	}

	virtual	void DrawButtonFrame(BView* view, BRect& rect, const BRect& updateRect, const rgb_color& base,
		const rgb_color& background, uint32 flags = 0, uint32 borders = B_ALL_BORDERS)
	{
		if (SmallRoundButton(view, rect, flags, borders))
			return;
		flags = WithoutDefaultRing(base, flags, rect);
		const BRect outer = rect;
		HaikuControlLook::DrawButtonFrame(view, rect, updateRect, base, FrameSurround(view, background), flags,
			borders);
		EraseFrameEdge(view, outer, background, flags);
	}

	virtual	void DrawButtonFrame(BView* view, BRect& rect, const BRect& updateRect, float radius,
		const rgb_color& base, const rgb_color& background, uint32 flags = 0, uint32 borders = B_ALL_BORDERS)
	{
		if (SmallRoundButton(view, rect, flags, borders))
			return;
		flags = WithoutDefaultRing(base, flags, rect);
		const BRect outer = rect;
		HaikuControlLook::DrawButtonFrame(view, rect, updateRect, radius, base, FrameSurround(view, background), flags,
			borders);
		EraseFrameEdge(view, outer, background, flags);
	}

	virtual	void DrawButtonFrame(BView* view, BRect& rect, const BRect& updateRect, float leftTopRadius,
		float rightTopRadius, float leftBottomRadius, float rightBottomRadius, const rgb_color& base,
		const rgb_color& background, uint32 flags = 0, uint32 borders = B_ALL_BORDERS)
	{
		if (leftTopRadius == rightTopRadius && SmallRoundButton(view, rect, flags, borders))
			return;
		flags = WithoutDefaultRing(base, flags, rect);
		const BRect outer = rect;
		HaikuControlLook::DrawButtonFrame(view, rect, updateRect, leftTopRadius, rightTopRadius,
			leftBottomRadius, rightBottomRadius, base, FrameSurround(view, background), flags, borders);
		EraseFrameEdge(view, outer, background, flags);
	}

	// the frame's outermost line is a tint of the colour around the button: a faint light line round it. It is drawn
	// in that colour instead
	void EraseFrameEdge(BView* view, const BRect& outer, const rgb_color& background, uint32 flags)
	{
		if (PlainButton(view) || (flags & (B_FLAT | B_DEFAULT_BUTTON)) != 0 || !outer.IsValid())
			return;
		view->PushState();
		view->SetDrawingMode(B_OP_COPY);
		view->SetHighColor(SurroundColor(view, background));
		view->StrokeRect(outer);
		// and the line just inside it, all the way round (the button's own body starts inside both)
		if (outer.Width() > 8 && outer.Height() > 8)
			view->StrokeRect(outer.InsetByCopy(1, 1));
		view->PopState();
	}

	// the frame's outer edge is blended with the colour around the button: that of the window, not of the control
	static rgb_color FrameSurround(BView* view, const rgb_color& background)
	{
		return PlainButton(view) ? background : SurroundColor(view, background);
	}

	// the label of a default button on a light panel sits on a dark body: white
	virtual	void DrawLabel(BView* view, const char* label, const BBitmap* icon, BRect rect,
		const BRect& updateRect, const rgb_color& base, uint32 flags, const BAlignment& alignment,
		const rgb_color* textColor)
	{
		rgb_color white = make_color(255, 255, 255);
		const bool lightPanel = LightPanel();
		if ((flags & B_DEFAULT_BUTTON) != 0 && (flags & (B_DISABLED | B_FLAT)) == 0 && lightPanel
			&& !PlainButton(view)) {
			textColor = &white;
		}
		HaikuControlLook::DrawLabel(view, label, icon, rect, updateRect, base, flags, alignment, textColor);
	}

	// Drop-down fields are the accent pill (DrawFieldPill). The frame the field draws around its menu bar is left
	// out: the pill is its own outline, and the field's margin stays the panel colour.
	virtual	void DrawMenuFieldFrame(BView* view, BRect& rect, const BRect& updateRect, const rgb_color& base,
		const rgb_color& background, uint32 flags = 0, uint32 borders = B_ALL_BORDERS)
	{
		if (PlainButton(view))
			HaikuControlLook::DrawMenuFieldFrame(view, rect, updateRect, base, background, flags, borders);
	}

	virtual	void DrawMenuFieldFrame(BView* view, BRect& rect, const BRect& updateRect, float radius,
		const rgb_color& base, const rgb_color& background, uint32 flags = 0, uint32 borders = B_ALL_BORDERS)
	{
		if (PlainButton(view)) {
			HaikuControlLook::DrawMenuFieldFrame(view, rect, updateRect, radius, base, background, flags,
				borders);
		}
	}

	virtual	void DrawMenuFieldFrame(BView* view, BRect& rect, const BRect& updateRect, float leftTopRadius,
		float rightTopRadius, float leftBottomRadius, float rightBottomRadius, const rgb_color& base,
		const rgb_color& background, uint32 flags = 0, uint32 borders = B_ALL_BORDERS)
	{
		if (PlainButton(view)) {
			HaikuControlLook::DrawMenuFieldFrame(view, rect, updateRect, leftTopRadius, rightTopRadius,
				leftBottomRadius, rightBottomRadius, base, background, flags, borders);
		}
	}

	virtual	void DrawMenuFieldBackground(BView* view, BRect& rect, const BRect& updateRect,
		const rgb_color& base, bool popupIndicator, uint32 flags = 0)
	{
		if (PlainButton(view) || !ShouldDraw(view, rect, updateRect)) {
			HaikuControlLook::DrawMenuFieldBackground(view, rect, updateRect, base, popupIndicator, flags);
			return;
		}
		DrawFieldPill(view, rect, base, flags, popupIndicator);
	}

	virtual	void DrawMenuFieldBackground(BView* view, BRect& rect, const BRect& updateRect, float radius,
		const rgb_color& base, bool popupIndicator, uint32 flags = 0)
	{
		if (PlainButton(view) || !ShouldDraw(view, rect, updateRect)) {
			HaikuControlLook::DrawMenuFieldBackground(view, rect, updateRect, radius, base, popupIndicator,
				flags);
			return;
		}
		DrawFieldPill(view, rect, base, flags, popupIndicator);
	}

	virtual	void DrawMenuFieldBackground(BView* view, BRect& rect, const BRect& updateRect, float leftTopRadius,
		float rightTopRadius, float leftBottomRadius, float rightBottomRadius, const rgb_color& base,
		bool popupIndicator, uint32 flags = 0)
	{
		if (PlainButton(view) || !ShouldDraw(view, rect, updateRect)) {
			HaikuControlLook::DrawMenuFieldBackground(view, rect, updateRect, leftTopRadius, rightTopRadius,
				leftBottomRadius, rightBottomRadius, base, popupIndicator, flags);
			return;
		}
		DrawFieldPill(view, rect, base, flags, popupIndicator);
	}

	// Tracker's item count asks for B_FLAT: the same accent bar as the column titles, drawn over the panel colour
	// so that its see-through ends leave nothing behind when the window is resized
	virtual	void DrawMenuBarBackground(BView* view, BRect& rect, const BRect& updateRect, const rgb_color& base,
		uint32 flags = 0, uint32 borders = B_ALL_BORDERS)
	{
		// the folder icon at the end of a Tracker window's menu bar: a round backdrop like the toolbar's buttons
		if (strstr(typeid(*view).name(), "DraggableContainerIcon") != NULL) {
			if (ShouldDraw(view, rect, updateRect))
				DrawSolidButton(view, view->Bounds(), 0);
			return;
		}

		if (PillMenuBar(view) && rect.Height() >= 10 && ShouldDraw(view, rect, updateRect)) {
			// the bar's rounded ends need the whole bar drawn: it is drawn whole whatever part is asked for (the
			// window system clips it), and asks for a full update when it is resized
			if ((view->Flags() & B_FULL_UPDATE_ON_RESIZE) == 0)
				view->SetFlags(view->Flags() | B_FULL_UPDATE_ON_RESIZE);
			const rgb_color tone = ToneForText(ui_color(B_MENU_ITEM_TEXT_COLOR));
			view->PushState();
			view->SetDrawingMode(B_OP_COPY);
			view->SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
			view->FillRect(view->Bounds());
			view->PopState();
			BRect bar = rect;
			bar.InsetBy(0, 1);
			DrawAccentBlock(view, bar, bar.Height() / 2 + 1, false, false, &tone);
			return;
		}

		const bool foreignCount = (flags & B_FLAT) == 0 && IsTrackerCount(view);
		if (((flags & B_FLAT) == 0 && !foreignCount) || rect.Height() < 10
			|| !ShouldDraw(view, rect, updateRect)) {
			HaikuControlLook::DrawMenuBarBackground(view, rect, updateRect, base, flags, borders);
			return;
		}
		// the bar's rounded ends need the whole bar drawn: outside Tracker it is asked for once
		if (foreignCount && !updateRect.Contains(view->Bounds()))
			view->Invalidate();
		view->PushState();
		view->ClipToRect(rect);
		view->SetDrawingMode(B_OP_COPY);
		view->SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
		view->FillRect(rect);
		view->PopState();
		BRect bar = rect;
		bar.InsetBy(0, 1);
		const rgb_color tone = BarToneForText();
		DrawAccentBlock(view, bar, bar.Height() / 2 + 1, false, false, foreignCount ? &tone : NULL);
	}

	// the slider's thumb is a rounded block in the accent; it keeps the area Haiku gives it
	virtual	void DrawSliderThumb(BView* view, BRect& rect, const BRect& updateRect, const rgb_color& base,
		uint32 flags, orientation orientation)
	{
		if (!ShouldDraw(view, rect, updateRect)) {
			HaikuControlLook::DrawSliderThumb(view, rect, updateRect, base, flags, orientation);
			return;
		}
		BRect block = rect;
		if (orientation == B_HORIZONTAL)
			block.InsetBy(0, 1);
		else
			block.InsetBy(1, 0);
		DrawAccentBlock(view, block, 4.0f, orientation == B_VERTICAL, (flags & B_DISABLED) != 0);
	}

	// the selected tab gets a two pixel line of the accent along its outer edge
	// the other tabs of a pill tab strip have no body of their own
	virtual	void DrawInactiveTab(BView* view, BRect& rect, const BRect& updateRect, const rgb_color& base,
		uint32 flags = 0, uint32 borders = B_ALL_BORDERS, uint32 side = B_TOP_BORDER, int32 index = 0,
		int32 selected = -1, int32 first = 0, int32 last = 0)
	{
		BRect outer(floorf(rect.left), floorf(rect.top), floorf(rect.right), floorf(rect.bottom));
		if ((side == B_TOP_BORDER || side == B_BOTTOM_BORDER) && outer.Height() >= 14) {
			if (ShouldDraw(view, outer, updateRect))
				ClearTab(view, outer, base, side);
			rect = outer;
			rect.InsetBy(2, 2);
			return;
		}
		HaikuControlLook::DrawInactiveTab(view, rect, updateRect, base, flags, borders, side, index, selected,
			first, last);
	}

	// clears the tab and draws the thin rule between the tab strip and the page
	static void ClearTab(BView* view, const BRect& outer, const rgb_color& base, uint32 side)
	{
		const bool dark = (base.red * 299 + base.green * 587 + base.blue * 114) / 1000 < 128;
		view->PushState();
		view->ClipToRect(outer);
		view->SetDrawingMode(B_OP_COPY);
		view->SetHighColor(base);
		view->FillRect(outer);
		view->SetHighColor(dark ? Mix(base, 255, 0.2f) : Mix(base, 0, 0.35f));
		const float y = side == B_BOTTOM_BORDER ? outer.top : outer.bottom;
		view->StrokeLine(BPoint(outer.left, y), BPoint(outer.right, y));
		view->PopState();
	}

	virtual	void DrawActiveTab(BView* view, BRect& rect, const BRect& updateRect, const rgb_color& base,
		uint32 flags = 0, uint32 borders = B_ALL_BORDERS, uint32 side = B_TOP_BORDER, int32 index = 0,
		int32 selected = -1, int32 first = 0, int32 last = 0)
	{
		BRect outer(floorf(rect.left), floorf(rect.top), floorf(rect.right), floorf(rect.bottom));
		// tabs along the top or bottom: the selected one is a pill of the accent, the others are plain
		if ((side == B_TOP_BORDER || side == B_BOTTOM_BORDER) && outer.Height() >= 14) {
			if (ShouldDraw(view, outer, updateRect)) {
				ClearTab(view, outer, base, side);
				BRect pill = outer;
				pill.InsetBy(1, 3);
				const rgb_color tone = ToneForText(ui_color(B_PANEL_TEXT_COLOR));
				DrawAccentBlock(view, pill, pill.Height() / 2 + 1, false, (flags & B_DISABLED) != 0, &tone);
			}
			rect = outer;
			rect.InsetBy(2, 2);
			return;
		}
		HaikuControlLook::DrawActiveTab(view, rect, updateRect, base, flags, borders, side, index, selected,
			first, last);
		if ((flags & B_DISABLED) != 0 || !ShouldDraw(view, outer, updateRect))
			return;

		BRect line;
		const float corner = 4;
		switch (side) {
			case B_BOTTOM_BORDER:
				line.Set(outer.left + corner, outer.bottom - 2, outer.right - corner, outer.bottom - 1);
				break;
			case B_LEFT_BORDER:
				line.Set(outer.left + 1, outer.top + corner, outer.left + 2, outer.bottom - corner);
				break;
			case B_RIGHT_BORDER:
				line.Set(outer.right - 2, outer.top + corner, outer.right - 1, outer.bottom - corner);
				break;
			default:
				line.Set(outer.left + corner, outer.top + 1, outer.right - corner, outer.top + 2);
				break;
		}
		if (!line.IsValid())
			return;
		view->PushState();
		view->ClipToRect(outer);
		view->SetDrawingMode(B_OP_COPY);
		view->SetHighColor(Accent());
		view->FillRect(line);
		view->PopState();
	}

	virtual	void DrawMenuItemBackground(BView* view, BRect& rect, const BRect& updateRect,
		const rgb_color& base, uint32 flags = 0, uint32 borders = B_ALL_BORDERS)
	{
		if ((flags & B_ACTIVATED) == 0) {
			HaikuControlLook::DrawMenuItemBackground(view, rect, updateRect, base, flags, borders);
			return;
		}

		// a title of a pill menu bar: a darker pill inside the bar's, with light text
		if (PillMenuBar(view)) {
			if (ShouldDraw(view, rect, updateRect)) {
				const rgb_color deeper = MixColors(Dark(Accent()), Accent(), 0.25f);
				BRect inner = rect.InsetByCopy(1, 2);
				DrawAccentBlock(view, inner, inner.Height() / 2 + 1, false, false, &deeper);
			}
			view->SetLowColor(Dark(Accent()));
			view->SetHighColor(255, 255, 255);
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


static bool
IsPillMenuBar(BView* view)
{
	return SnakeControlLook::PillMenuBar(view);
}

}	// namespace BPrivate


extern "C" BControlLook*
instantiate_control_look(image_id id)
{
	return new (std::nothrow) BPrivate::SnakeControlLook();
}
