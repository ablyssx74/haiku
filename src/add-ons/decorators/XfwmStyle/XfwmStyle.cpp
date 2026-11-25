/*
 * XfwmStyle: an xfwm4 theme as a decorator style (RenkuOS/Source#626, the style interface in DecorStyle.h).
 *
 * The same drawing and layout as the XfwmDecorator add-on (which is made on app_server's private Decorator classes and has to
 * be built for the app_server it runs in), but made only of the plain structures of DecorStyle.h, so that it does not have to
 * be built again for each build of app_server. The theme itself (XfwmTheme: the XPM and PNG pictures and the themerc) is the
 * same code.
 *
 * The theme is the one named in the file's name: XfwmStyle-b6 is the b6 theme (one file of the same code for each theme,
 * as with the add-on).
 */

#include "DecorStyle.h"
#include "ServerBitmap.h"
#include "XfwmTheme.h"

#include <FindDirectory.h>
#include <Path.h>
#include <String.h>
#include <Window.h>

#include <image.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <algorithm>
#include <new>
#include <vector>


// #pragma mark - the theme and the font


static XfwmTheme* sTheme = NULL;
static decor_style sStyle;

// the title font: the system's bold font, at its own size
static const decor_font kTitleFont = { DECOR_FONT_BOLD, 0, 0, 0 };


static decor_rect
Rect(float left, float top, float right, float bottom)
{
	decor_rect rect = { left, top, right, bottom };
	return rect;
}


static decor_rect
NoRect()
{
	return Rect(0, 0, -1, -1);
}


static bool
IsValid(const decor_rect& rect)
{
	return rect.right >= rect.left && rect.bottom >= rect.top;
}


static bool
Contains(const decor_rect& rect, decor_point point)
{
	return IsValid(rect) && point.x >= rect.left && point.x <= rect.right && point.y >= rect.top
		&& point.y <= rect.bottom;
}


// What kind of frame the window has, from the look of its front tab.
enum {
	kFrameNone,		// no border
	kFrameBordered,	// a one pixel line
	kFrameModal,	// a border all round, no title bar
	kFrameTitled	// the theme's title bar and borders
};


static const decor_tab*
TopTab(const decor_window* window)
{
	if (window->tab_count <= 0 || window->tabs == NULL)
		return NULL;
	const int32 index = window->top_tab >= 0 && window->top_tab < window->tab_count ? window->top_tab : 0;
	return window->tabs[index];
}


static int32
FrameKind(const decor_tab* top)
{
	if (top == NULL)
		return kFrameNone;
	switch (top->look) {
		case B_TITLED_WINDOW_LOOK:
		case B_DOCUMENT_WINDOW_LOOK:
		case B_FLOATING_WINDOW_LOOK:
			return kFrameTitled;
		case B_MODAL_WINDOW_LOOK:
			return kFrameModal;
		case B_NO_BORDER_WINDOW_LOOK:
			return kFrameNone;
		default:
			// the looks this style has no frame for (a title on the side, ...): a plain line
			return kFrameBordered;
	}
}


static bool
Active(const decor_tab* tab)
{
	return tab != NULL && (tab->focused || tab->button_focus);
}


// A dark desktop gets dark borders: the theme's symbolic colours come from the desktop's, and the pictures are loaded again
// when those changed.
static void
UpdatePalette(decor_canvas* canvas)
{
	if (sTheme == NULL)
		return;
	const rgb_color panel = canvas->ui_color(canvas, B_PANEL_BACKGROUND_COLOR);
	const bool dark = (panel.red * 299 + panel.green * 587 + panel.blue * 114) / 1000 < 128;
	if (XfwmTheme::SetDarkPalette(dark, canvas->ui_color(canvas, B_WINDOW_TAB_COLOR),
			canvas->ui_color(canvas, B_WINDOW_INACTIVE_TAB_COLOR), panel)) {
		sTheme->Reload();
	}
}


// The hover pictures are on unless the file ~/config/settings/xfwm_decorators_no_hover exists (the xfwm-hover command makes
// and removes it); looked at every two seconds, not at every draw.
static bool
HoverOn()
{
	static bigtime_t lastLook = 0;
	static bool on = true;
	const bigtime_t now = system_time();
	if (now - lastLook > 2000000) {
		lastLook = now;
		BPath path;
		on = true;
		if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK && path.Append("xfwm_decorators_no_hover") == B_OK) {
			struct stat st;
			on = stat(path.Path(), &st) != 0;
		}
	}
	return on;
}


static bool
IsHovered(const decor_tab* tab, int32 button)
{
	// decor_tab::hovered is from version 5: an older app_server's decor_tab is too small to have it
	return HoverOn() && tab->size >= offsetof(decor_tab, hovered) + sizeof(tab->hovered) && tab->hovered[button];
}


// #pragma mark - measuring


static float
TextWidth(decor_canvas* canvas, const char* string)
{
	if (string == NULL || string[0] == '\0')
		return 0;
	return canvas->string_width(canvas, &kTitleFont, string, (int32)strlen(string));
}


// The buttons a window has, by the letters of the theme's button layout (C, M and H).
static bool
HasButton(const decor_tab* tab, char letter)
{
	switch (letter) {
		case 'C':
			return (tab->flags & B_NOT_CLOSABLE) == 0;
		case 'M':
			return (tab->flags & B_NOT_ZOOMABLE) == 0;
		case 'H':
			return (tab->flags & B_NOT_MINIMIZABLE) == 0;
		default:
			return false;
	}
}


// #pragma mark - the bar


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
	int32	frameEnd;		// one past the right end of the right border picture
	int32	topRightX;		// where the picture of the bar's right end starts
	int32	restStart;		// where the plain bar after the last tab starts
	std::vector<TabSlot> slots;
};


static int32
StackOverlap(const decor_window* window)
{
	return sTheme->StackedTabs() && window->tab_count > 1 ? sTheme->StackOverlap() : 0;
}


// A tab after the first has its left end close to the tab before it, so its left button gets a little room from that
static int32
LeftGap(int32 index)
{
	return index > 0 && sTheme->ButtonsInTab() ? 3 : 0;
}


// How far from the tab's left end its title starts, when it is not the first tab: past the left buttons, and at least where the
// pictures of the end leave room.
static int32
TabTextStart(const decor_tab* tab, int32 index)
{
	const int32 before = sTheme->Title(0, true).Width() + sTheme->Title(1, true).Width();
	const int32 gap = LeftGap(index);
	int32 start = before + gap;

	const BString layout = sTheme->ButtonLayout();
	int32 divider = layout.FindFirst('|');
	if (!sTheme->ButtonsInTab() || divider < 0 || tab == NULL)
		return start;
	int32 count = 0;
	for (int32 i = 0; i < divider; i++) {
		if (HasButton(tab, layout[i]))
			count++;
	}
	if (count == 0)
		return start;
	const int32 buttonsEnd = std::max((int32)0, sTheme->ButtonOffset()) + gap
		+ count * (sTheme->ButtonWidth() + sTheme->ButtonSpacing()) - sTheme->ButtonSpacing();
	return std::max(start, buttonsEnd + 2);
}


// A theme with its buttons in the tab has the right ones at the tab's right end, over the picture of the end; each tab is made
// that much wider, less a few pixels, so that the title does not run up to them.
static int32
RoomForTabButtons(const decor_tab* tab)
{
	if (!sTheme->ButtonsInTab() || tab == NULL)
		return 0;

	const BString layout = sTheme->ButtonLayout();
	int32 divider = layout.FindFirst('|');
	if (divider < 0)
		divider = layout.Length();
	int32 rightCount = 0;
	for (int32 i = divider + 1; i < layout.Length(); i++) {
		if (HasButton(tab, layout[i]))
			rightCount++;
	}
	if (rightCount == 0)
		return 0;
	return std::max((int32)0, rightCount * (sTheme->ButtonWidth() + sTheme->ButtonSpacing()) - 2);
}


static void
ButtonExtents(const decor_window* window, const decor_tab* top, int32& leftEnd, int32& rightStart)
{
	leftEnd = (int32)window->frame.left;
	rightStart = (int32)window->frame.right + 1;
	if (top == NULL || sTheme->ButtonsInTab())
		return;

	const BString layout = sTheme->ButtonLayout();
	int32 divider = layout.FindFirst('|');
	if (divider < 0)
		divider = layout.Length();

	int32 leftCount = 0, rightCount = 0;
	for (int32 i = 0; i < layout.Length(); i++) {
		if (!HasButton(top, layout[i]))
			continue;
		if (i < divider)
			leftCount++;
		else if (i > divider)
			rightCount++;
	}

	const int32 width = sTheme->ButtonWidth();
	const int32 spacing = sTheme->ButtonSpacing();
	const int32 offset = sTheme->ButtonOffset();
	if (leftCount > 0)
		leftEnd = (int32)window->frame.left + offset + leftCount * (width + spacing) - spacing;
	if (rightCount > 0)
		rightStart = (int32)window->frame.right - offset - (rightCount - 1) * (width + spacing) - width + 1;
}


// Where the tabs go. xfwm4 lays the frame out by the full size of the border pictures (a left picture 14 wide is a 14 wide
// border, transparent part included), so the pieces are anchored to that outer frame.
static void
ComputeBar(const decor_window* window, decor_canvas* canvas, BarLayout& bar)
{
	const decor_tab* top = TopTab(window);
	const bool active = Active(top);
	const int32 bw = sTheme->BorderWidth();
	const XfwmImage& topLeft = sTheme->TopLeft(active);
	const XfwmImage& topRight = sTheme->TopRight(active);

	bar.x0 = (int32)window->frame.left - sTheme->Left(active).Width();
	bar.y = (int32)window->frame.top - sTheme->TitleHeight();
	bar.right = (int32)window->frame.right + bw;
	bar.frameEnd = (int32)window->frame.right + 1 + sTheme->Right(active).Width();
	bar.topRightX = bar.frameEnd - topRight.Width();
	bar.slots.clear();

	const int32 count = window->tab_count;
	if (count == 0) {
		bar.restStart = bar.x0;
		return;
	}

	const int32 capWidth = topLeft.Width();
	const int32 before = sTheme->Title(0, active).Width() + sTheme->Title(1, active).Width();
	const int32 after = sTheme->Title(3, active).Width();
	const int32 available = std::max((int32)0, bar.topRightX - bar.x0);

	std::vector<int32> natural(count), fixed(count);
	int32 sum = 0;
	for (int32 i = 0; i < count; i++) {
		const decor_tab* tab = window->tabs[i];
		fixed[i] = (i == 0 ? capWidth : 0) + before + after + RoomForTabButtons(tab)
			+ (i == 0 ? 0 : TabTextStart(tab, i) - before);
		natural[i] = fixed[i] + (int32)ceilf(TextWidth(canvas, tab->title));
		sum += natural[i];
	}

	// the tabs of a stack lie over each other's ends a little
	const int32 overlap = StackOverlap(window);
	const int32 totalOverlap = overlap * (count - 1);

	// a theme with a full width title has one tab across the whole bar: any room left is shared out
	std::vector<int32> extra(count, 0);
	if (sTheme->FullWidthTitle() && sum - totalOverlap < available) {
		int32 spare = available - (sum - totalOverlap);
		for (int32 i = 0; i < count; i++)
			extra[i] = spare / count + (i == count - 1 ? spare % count : 0);
	}

	int32 x = bar.x0;
	for (int32 i = 0; i < count; i++) {
		int32 width = natural[i] + extra[i];
		if (sum - totalOverlap > available && sum > 0) {
			// too many or too long: every tab gets a share of the room, but keeps its edges
			width = std::max(fixed[i], (int32)((int64)(available + totalOverlap) * natural[i] / sum));
		}
		if (x + width > bar.x0 + available)
			width = std::max(fixed[i], bar.x0 + available - x);

		TabSlot slot;
		slot.x = x;
		slot.width = width;
		slot.fillLeft = x + (i == 0 ? capWidth : 0) + before;
		slot.textLeft = i == 0 ? slot.fillLeft : x + TabTextStart(window->tabs[i], i);
		slot.textWidth = std::max((int32)0, width - fixed[i]);
		// (the tab's own color runs on under the room kept for the buttons, up to the picture of its end)
		slot.fillWidth = std::max((int32)0, width - ((i == 0 ? capWidth : 0) + before + after));
		bar.slots.push_back(slot);
		x += width - (i < count - 1 ? overlap : 0);
	}
	bar.restStart = bar.slots.back().x + bar.slots.back().width;

	// the title text goes between the buttons, left side and right side alike
	int32 leftEnd, rightStart;
	ButtonExtents(window, top, leftEnd, rightStart);
	for (size_t i = 0; i < bar.slots.size(); i++) {
		TabSlot& slot = bar.slots[i];
		int32 textLeft = slot.textLeft, textRight = slot.textLeft + slot.textWidth;
		if (slot.x < leftEnd && textLeft < leftEnd)
			textLeft = leftEnd;
		if (slot.x + slot.width > rightStart && textRight > rightStart)
			textRight = rightStart;
		slot.textLeft = textLeft;
		slot.textWidth = std::max((int32)0, textRight - textLeft);
	}
}


// The picture for the end of a tab: the tabs of a stack that are not the last one end without the bar's band, and the last
// one, when it is not the front one in an active window, has the band of the bar that goes on from it.
static const XfwmImage&
EndOf(const decor_window* window, int32 index, int32 count, bool active, bool stacked)
{
	if (stacked && index < count - 1)
		return sTheme->TabEnd(active);
	if (stacked && !active && Active(TopTab(window)))
		return sTheme->TabEndOnActiveBar();
	return sTheme->Title(3, active);
}


// The buttons of the front tab: at the ends of the bar, or in the tab.
static void
LayoutButtons(const decor_window* window, decor_tab* tab, int32 index, const BarLayout& bar)
{
	const bool active = Active(tab);
	const BString layout = sTheme->ButtonLayout();
	int32 divider = layout.FindFirst('|');
	if (divider < 0)
		divider = layout.Length();

	// the letters of a side, in order; a button the window doesn't have takes no room
	struct Placement {
		int32			button;
		decor_rect*		rect;
	};
	Placement left[4], right[4];
	int32 leftCount = 0, rightCount = 0;
	for (int32 i = 0; i < layout.Length(); i++) {
		int32 button = -1;
		switch (layout[i]) {
			case 'C':
				button = DECOR_BUTTON_CLOSE;
				break;
			case 'M':
				button = DECOR_BUTTON_ZOOM;
				break;
			case 'H':
				button = DECOR_BUTTON_MINIMIZE;
				break;
			default:
				break;
		}
		if (button < 0 || !HasButton(tab, layout[i]))
			continue;
		Placement placement = { button, &tab->buttons[button] };
		if (i < divider && leftCount < 4)
			left[leftCount++] = placement;
		else if (i > divider && rightCount < 4)
			right[rightCount++] = placement;
	}

	const int32 width = sTheme->ButtonWidth();
	const int32 spacing = sTheme->ButtonSpacing();
	const int32 offset = sTheme->ButtonOffset();
	const int32 state = active ? kStateActive : kStateInactive;

	if (sTheme->ButtonsInTab()) {
		// in the tab: the left ones after its cap (when it is the first), the right ones against its right end
		if (index < 0 || index >= (int32)bar.slots.size())
			return;
		const TabSlot& slot = bar.slots[index];
		int32 rx = slot.x + slot.width - sTheme->ButtonOffsetRight();
		for (int32 i = rightCount - 1; i >= 0; i--) {
			BRect opaque = sTheme->Button(right[i].button, state).OpaqueBounds();
			*right[i].rect = Rect(rx - width, bar.y + opaque.top, rx - 1, bar.y + opaque.bottom);
			rx -= width + spacing;
		}
		int32 lx = index == 0 ? slot.x + sTheme->TopLeft(active).Width() + offset
			: slot.x + std::max((int32)0, offset) + LeftGap(index);
		for (int32 i = 0; i < leftCount; i++) {
			BRect opaque = sTheme->Button(left[i].button, state).OpaqueBounds();
			*left[i].rect = Rect(lx, bar.y + opaque.top, lx + width - 1, bar.y + opaque.bottom);
			lx += width + spacing;
		}
		return;
	}

	// right side: from the inner edge of the right border (the frame's right edge), left side likewise
	int32 x = (int32)window->frame.right - offset;
	for (int32 i = rightCount - 1; i >= 0; i--) {
		BRect opaque = sTheme->Button(right[i].button, state).OpaqueBounds();
		*right[i].rect = Rect(x - width + 1, bar.y + opaque.top, x, bar.y + opaque.bottom);
		x -= width + spacing;
	}

	x = (int32)window->frame.left + offset;
	for (int32 i = 0; i < leftCount; i++) {
		BRect opaque = sTheme->Button(left[i].button, state).OpaqueBounds();
		*left[i].rect = Rect(x, bar.y + opaque.top, x + width - 1, bar.y + opaque.bottom);
		x += width + spacing;
	}
}


// #pragma mark - areas of the pictures


static BRect
LeftArea(const decor_window* window, bool active)
{
	const XfwmImage& left = sTheme->Left(active);
	return BRect(window->frame.left - left.Width(), window->frame.top, window->frame.left - 1, window->frame.bottom);
}


static BRect
RightArea(const decor_window* window, bool active)
{
	const XfwmImage& right = sTheme->Right(active);
	float x = window->frame.right + 1 - right.OpaqueBounds().left;
	return BRect(x, window->frame.top, x + right.Width() - 1, window->frame.bottom);
}


// The strip between the two corner pictures, as in xfwm4: the corners have the shape of their own (a rounded one is see-through
// where the strip would show), and the border is not wider than the sides, which a strip from the border width could be.
static BRect
BottomArea(const decor_window* window, bool active)
{
	const XfwmImage& bottom = sTheme->Bottom(active);
	const XfwmImage& bottomLeft = sTheme->BottomLeft(active);
	const XfwmImage& bottomRight = sTheme->BottomRight(active);
	float y = window->frame.bottom + 1 - bottom.OpaqueBounds().top;
	float left = window->frame.left - sTheme->Left(active).Width() + bottomLeft.Width();
	float right = window->frame.right + sTheme->Right(active).Width() - bottomRight.Width();
	return BRect(left, y, right, y + bottom.Height() - 1);
}


static BRect
TopFrameArea(const decor_window* window, bool active)
{
	const XfwmImage& topFrame = sTheme->TopFrame(active);
	float y = window->frame.top - topFrame.Height();
	float left = window->frame.left - sTheme->Left(active).Width() + sTheme->TopLeftCorner(active).Width();
	float right = window->frame.right + sTheme->Right(active).Width() - sTheme->TopRightCorner(active).Width();
	return BRect(left, y, right, y + topFrame.Height() - 1);
}


// #pragma mark - layout


static status_t
xfwm_create_window(decor_window* window, decor_canvas* canvas)
{
	UpdatePalette(canvas);
	return B_OK;
}


static void
xfwm_delete_window(decor_window* window)
{
}


static void
xfwm_update_font(decor_window* window, decor_canvas* canvas)
{
	canvas->set_font(canvas, &kTitleFont);
}


static void
xfwm_update_colors(decor_window* window, decor_canvas* canvas)
{
	UpdatePalette(canvas);
}


static void
xfwm_layout(decor_window* window, decor_canvas* canvas)
{
	const decor_tab* top = TopTab(window);
	const int32 kind = FrameKind(top);
	const decor_rect frame = window->frame;

	window->border_width = 0;
	window->left_border = window->top_border = window->right_border = window->bottom_border = NoRect();
	window->title_bar = NoRect();
	window->resize = NoRect();
	window->tabs_slide = false;
	for (int32 i = 0; i < window->tab_count; i++) {
		decor_tab* tab = window->tabs[i];
		tab->frame = NoRect();
		for (int32 b = 0; b < DECOR_BUTTON_CAPACITY; b++)
			tab->buttons[b] = NoRect();
		tab->text_offset = 0;
		tab->title_room = 0;
		tab->truncate = DECOR_TRUNCATE_END;
		tab->min_size = tab->max_size = 0;
	}

	if (kind == kFrameNone)
		return;

	if (kind == kFrameBordered) {
		window->border_width = 1;
		window->left_border = Rect(frame.left - 1, frame.top, frame.left - 1, frame.bottom);
		window->right_border = Rect(frame.right + 1, frame.top, frame.right + 1, frame.bottom);
		window->top_border = Rect(frame.left - 1, frame.top - 1, frame.right + 1, frame.top - 1);
		window->bottom_border = Rect(frame.left - 1, frame.bottom + 1, frame.right + 1, frame.bottom + 1);
		return;
	}

	const int32 bw = sTheme->BorderWidth();
	window->border_width = bw;
	window->left_border = Rect(frame.left - bw, frame.top, frame.left - 1, frame.bottom);
	window->right_border = Rect(frame.right + 1, frame.top, frame.right + bw, frame.bottom);
	window->bottom_border = Rect(frame.left - bw, frame.bottom + 1, frame.right + bw, frame.bottom + bw);

	if (kind == kFrameModal) {
		// no title bar: a border all the way round, the top one made from the bottom border turned over
		window->top_border = Rect(frame.left - bw, frame.top - bw, frame.right + bw, frame.top - 1);
		return;
	}

	// the title bar is the top border
	if (top->look == B_DOCUMENT_WINDOW_LOOK) {
		const float knob = 18;
		window->resize = Rect(window->bottom_border.right - knob, window->bottom_border.bottom - knob,
			window->bottom_border.right, window->bottom_border.bottom);
	}

	BarLayout bar;
	ComputeBar(window, canvas, bar);

	const int32 height = std::max(sTheme->TitleHeight(), sTheme->TopLeft(true).Height());
	window->title_bar = Rect(bar.x0, bar.y, bar.right + 1, bar.y + height - 1);

	const int32 overlap = StackOverlap(window);
	for (int32 i = 0; i < window->tab_count; i++) {
		decor_tab* tab = window->tabs[i];
		const TabSlot& slot = bar.slots[i];

		// (a tab is what shows of it: the next one lies over its end)
		const int32 right = slot.x + slot.width - 1 - (i < window->tab_count - 1 ? overlap : 0);
		tab->frame = Rect(slot.x, bar.y, right, bar.y + sTheme->TitleHeight() - 1);
		tab->text_offset = std::max((int32)0, slot.textLeft - slot.x);
		tab->title_room = (float)slot.textWidth;
		tab->truncate = DECOR_TRUNCATE_END;
		tab->min_size = tab->max_size = (float)(tab->frame.right - tab->frame.left + 1);
	}

	// the buttons are at the right end of the bar and belong to the front tab
	const int32 topIndex = window->top_tab >= 0 && window->top_tab < window->tab_count ? window->top_tab : 0;
	if (window->tab_count > 0)
		LayoutButtons(window, window->tabs[topIndex], topIndex, bar);
}


// #pragma mark - drawing


static void
Blit(decor_canvas* canvas, const XfwmImage& image, float x, float y)
{
	if (!image.IsValid())
		return;
	const ServerBitmap* bitmap = image.Bitmap();
	canvas->draw_bitmap(canvas, bitmap->Bits(), image.Width(), image.Height(), bitmap->BytesPerRow(),
		Rect(0, 0, image.Width() - 1, image.Height() - 1),
		Rect(x, y, x + image.Width() - 1, y + image.Height() - 1));
}


static void
BlitTiled(decor_canvas* canvas, const XfwmImage& image, BRect area, bool horizontal)
{
	if (!image.IsValid() || !area.IsValid())
		return;

	const ServerBitmap* bitmap = image.Bitmap();
	if (horizontal) {
		for (float x = area.left; x <= area.right; x += image.Width()) {
			const float width = std::min((float)image.Width(), area.right - x + 1);
			canvas->draw_bitmap(canvas, bitmap->Bits(), image.Width(), image.Height(), bitmap->BytesPerRow(),
				Rect(0, 0, width - 1, image.Height() - 1),
				Rect(x, area.top, x + width - 1, area.top + image.Height() - 1));
		}
	} else {
		for (float y = area.top; y <= area.bottom; y += image.Height()) {
			const float height = std::min((float)image.Height(), area.bottom - y + 1);
			canvas->draw_bitmap(canvas, bitmap->Bits(), image.Width(), image.Height(), bitmap->BytesPerRow(),
				Rect(0, 0, image.Width() - 1, height - 1),
				Rect(area.left, y, area.left + image.Width() - 1, y + height - 1));
		}
	}
}


static void
AlphaMode(decor_canvas* canvas)
{
	canvas->set_drawing_mode(canvas, B_OP_ALPHA);
	canvas->set_blending_mode(canvas, B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
}


// The plain bar after the last tab and the bar's right end.
static void
DrawBarEnd(decor_canvas* canvas, const BarLayout& bar, bool active)
{
	AlphaMode(canvas);
	const XfwmImage& rest = sTheme->Title(4, active);
	if (bar.restStart < bar.topRightX)
		BlitTiled(canvas, rest, BRect(bar.restStart, bar.y, bar.topRightX - 1, bar.y + rest.Height() - 1), true);
	Blit(canvas, sTheme->TopRight(active), bar.topRightX, bar.y);
}


static void
xfwm_draw_frame(decor_window* window, decor_canvas* canvas, decor_rect invalid)
{
	const decor_tab* top = TopTab(window);
	const int32 kind = FrameKind(top);
	const decor_rect frame = window->frame;

	if (kind == kFrameNone)
		return;
	if (kind == kFrameBordered) {
		// menus and the like: a one pixel line in the theme's outline colour
		canvas->stroke_rect(canvas, Rect(frame.left - 1, frame.top - 1, frame.right + 1, frame.bottom + 1),
			sTheme->OutlineColor());
		return;
	}

	const bool active = Active(top);
	AlphaMode(canvas);

	BlitTiled(canvas, sTheme->Left(active), LeftArea(window, active), false);
	BlitTiled(canvas, sTheme->Right(active), RightArea(window, active), false);
	BlitTiled(canvas, sTheme->Bottom(active), BottomArea(window, active), true);

	if (top->look == B_DOCUMENT_WINDOW_LOOK) {
		// the resize knob inside the frame's bottom right corner
		const float knob = 18 - window->border_width;
		canvas->fill_rect(canvas, Rect(frame.right - knob, frame.bottom - knob, frame.right, frame.bottom),
			sTheme->ResizeKnobColor());
		AlphaMode(canvas);
	}

	const XfwmImage& bottomLeft = sTheme->BottomLeft(active);
	const XfwmImage& bottomRight = sTheme->BottomRight(active);
	BarLayout bar;
	ComputeBar(window, canvas, bar);
	const int32 bottomEnd = (int32)frame.bottom + 1 + sTheme->Bottom(active).Height();
	Blit(canvas, bottomLeft, bar.x0, bottomEnd - bottomLeft.Height());
	Blit(canvas, bottomRight, bar.frameEnd - bottomRight.Width(), bottomEnd - bottomRight.Height());

	if (kind == kFrameModal) {
		// the top border and its corners
		const XfwmImage& topFrame = sTheme->TopFrame(active);
		const int32 topY = (int32)frame.top - topFrame.Height();
		BlitTiled(canvas, topFrame, TopFrameArea(window, active), true);
		Blit(canvas, sTheme->TopLeftCorner(active), bar.x0, topY);
		Blit(canvas, sTheme->TopRightCorner(active), bar.frameEnd - sTheme->TopRightCorner(active).Width(), topY);
	}

	if (kind == kFrameTitled)
		DrawBarEnd(canvas, bar, active);

	canvas->set_drawing_mode(canvas, B_OP_COPY);
}


static void
DrawTitle(decor_window* window, decor_canvas* canvas, int32 index, const BarLayout& bar)
{
	const decor_tab* tab = window->tabs[index];
	if (index >= (int32)bar.slots.size() || tab->shown_title == NULL)
		return;
	const bool active = Active(tab);

	float ascent = 0, descent = 0, leading = 0;
	canvas->font_height(canvas, &kTitleFont, &ascent, &descent, &leading);

	canvas->set_font(canvas, &kTitleFont);
	canvas->set_drawing_mode(canvas, B_OP_OVER);

	const float textHeight = ascent + descent;
	// where the title sits in the room it has, as the theme's title_alignment says
	const float room = bar.slots[index].textWidth;
	const float used = canvas->string_width(canvas, &kTitleFont, tab->shown_title, tab->shown_title_length);
	float shift = 0;
	if (sTheme->TitleAlignment() == 1)
		shift = (room - used) / 2;
	else if (sTheme->TitleAlignment() == 2)
		shift = room - used - sTheme->TitleHorizontalOffset();
	else
		shift = sTheme->TitleHorizontalOffset();
	if (shift > room - used)
		shift = room - used;
	if (shift < 0)
		shift = 0;

	decor_point where = { (float)bar.slots[index].textLeft + floorf(shift),
		floorf(bar.y + (sTheme->TitleHeight() - textHeight) / 2 + ascent + sTheme->TitleOffset(active) + 0.5f) };
	const int32 shadow = sTheme->ShadowMode(active);
	if (shadow != XfwmTheme::kShadowNone) {
		// the theme's shadow: a copy of the title behind it, one pixel down and to the right, or on every side
		const rgb_color shadowColor = sTheme->ShadowColor(active);
		for (int32 dy = -1; dy <= 1; dy++) {
			for (int32 dx = -1; dx <= 1; dx++) {
				if ((dx == 0 && dy == 0) || (shadow == XfwmTheme::kShadowUnder && (dx != 1 || dy != 1)))
					continue;
				decor_point at = { where.x + dx, where.y + dy };
				canvas->draw_string(canvas, tab->shown_title, tab->shown_title_length, at, shadowColor);
			}
		}
	}
	canvas->draw_string(canvas, tab->shown_title, tab->shown_title_length, where, sTheme->TextColor(active));

	canvas->set_drawing_mode(canvas, B_OP_COPY);
}


static void
DrawOneButton(decor_canvas* canvas, const decor_tab* tab, int32 button)
{
	const decor_rect rect = tab->buttons[button];
	if (!IsValid(rect) || button >= kButtonCount)
		return;

	const bool active = Active(tab);
	int32 state = active ? kStateActive : kStateInactive;
	if (tab->pressed[button])
		state = kStatePressed;
	else if (IsHovered(tab, button))
		state = active ? kStatePrelight : kStatePrelightInactive;
	const XfwmImage& image = sTheme->Button(button, state);
	if (!image.IsValid())
		return;

	AlphaMode(canvas);
	Blit(canvas, image, rect.left, rect.top - image.OpaqueBounds().top);
	canvas->set_drawing_mode(canvas, B_OP_COPY);
}


static void
xfwm_draw_tab(decor_window* window, decor_canvas* canvas, int32 index, decor_rect invalid)
{
	if (FrameKind(TopTab(window)) != kFrameTitled)
		return;

	BarLayout bar;
	ComputeBar(window, canvas, bar);
	if (index < 0 || index >= (int32)bar.slots.size())
		return;

	const decor_tab* tab = window->tabs[index];
	const TabSlot& slot = bar.slots[index];
	const bool active = Active(tab);

	AlphaMode(canvas);

	// the tabs of a stack have a rounded corner at each end, and a pixel between them
	const bool stacked = StackOverlap(window) > 0;
	int32 x = slot.x;
	if (index == 0) {
		Blit(canvas, sTheme->TopLeft(active), x, bar.y);
		x += sTheme->TopLeft(active).Width();
	}
	if (stacked && index > 0) {
		Blit(canvas, sTheme->TabLeft(active), x, bar.y);
	} else {
		Blit(canvas, sTheme->Title(0, active), x, bar.y);
		x += sTheme->Title(0, active).Width();
		Blit(canvas, sTheme->Title(1, active), x, bar.y);
	}

	const XfwmImage& middle = sTheme->Title(2, active);
	if (slot.fillWidth > 0) {
		BlitTiled(canvas, middle, BRect(slot.fillLeft, bar.y, slot.fillLeft + slot.fillWidth - 1,
			bar.y + middle.Height() - 1), true);
	}
	const XfwmImage& end = EndOf(window, index, (int32)bar.slots.size(), active, stacked);
	Blit(canvas, end, slot.x + slot.width - end.Width(), bar.y);

	canvas->set_drawing_mode(canvas, B_OP_COPY);

	DrawTitle(window, canvas, index, bar);
	if (tab == TopTab(window)) {
		DrawBarEnd(canvas, bar, Active(tab));
		for (int32 button = 0; button < kButtonCount; button++)
			DrawOneButton(canvas, tab, button);
		canvas->set_drawing_mode(canvas, B_OP_COPY);
	}
}


static void
xfwm_draw_button(decor_window* window, decor_canvas* canvas, int32 index, int32 button, decor_rect invalid)
{
	if (index < 0 || index >= window->tab_count)
		return;
	DrawOneButton(canvas, window->tabs[index], button);
}


// #pragma mark - regions


static int32
xfwm_region_at(decor_window* window, decor_point where, int32* tab)
{
	if (FrameKind(TopTab(window)) != kFrameTitled)
		return DECOR_REGION_DEFAULT;

	const int32 topIndex = window->top_tab >= 0 && window->top_tab < window->tab_count ? window->top_tab : 0;
	if (window->tab_count > 0) {
		const decor_tab* top = window->tabs[topIndex];
		static const int32 regions[] = { DECOR_REGION_CLOSE_BUTTON, DECOR_REGION_ZOOM_BUTTON,
			DECOR_REGION_MINIMIZE_BUTTON };
		for (int32 button = 0; button < DECOR_BUTTON_COUNT; button++) {
			if (Contains(top->buttons[button], where)) {
				*tab = topIndex;
				return regions[button];
			}
		}
	}
	for (int32 i = 0; i < window->tab_count; i++) {
		if (Contains(window->tabs[i]->frame, where)) {
			*tab = i;
			return DECOR_REGION_TAB;
		}
	}

	// the part of the cap that sticks out to the left of the window, and the plain bar beyond the tabs: both drag the window
	// (the front tab's)
	if (Contains(window->title_bar, where) && where.y < window->frame.top) {
		*tab = topIndex;
		return DECOR_REGION_TAB;
	}
	if (Contains(window->title_bar, where) && where.x < window->left_border.left) {
		*tab = 0;
		return DECOR_REGION_LEFT_BORDER;
	}
	return DECOR_REGION_DEFAULT;
}


// The opaque pixels of a picture tiled along an area (the last tile cut off there, as it is drawn): a footprint that reached
// past the end would own pixels nothing paints.
static void
IncludeTiled(BRegion& region, const XfwmImage& image, BRect area, bool horizontal)
{
	if (!image.IsValid() || !area.IsValid())
		return;

	BRegion pieces;
	if (horizontal) {
		for (float x = area.left; x <= area.right; x += image.Width())
			image.IncludeIn(pieces, (int32)x, (int32)area.top);
	} else {
		for (float y = area.top; y <= area.bottom; y += image.Height())
			image.IncludeIn(pieces, (int32)area.left, (int32)y);
	}
	BRegion clip(area);
	pieces.IntersectWith(&clip);
	region.Include(&pieces);
}


static void
IncludeRow(BRegion& region, const XfwmImage& image, int32 from, int32 end, int32 y)
{
	if (!image.IsValid() || end <= from)
		return;

	BRegion tiles;
	for (int32 x = from; x < end; x += image.Width())
		image.IncludeIn(tiles, x, y);
	BRegion clip(BRect(from, y, end - 1, y + image.Height() - 1));
	tiles.IntersectWith(&clip);
	region.Include(&tiles);
}


static void
IncludeTab(const decor_window* window, BRegion& region, int32 index, const BarLayout& bar)
{
	if (index < 0 || index >= (int32)bar.slots.size())
		return;

	const TabSlot& slot = bar.slots[index];
	const bool active = Active(window->tabs[index]);
	const bool stacked = StackOverlap(window) > 0;
	int32 x = slot.x;
	if (index == 0) {
		sTheme->TopLeft(active).IncludeIn(region, x, bar.y);
		x += sTheme->TopLeft(active).Width();
	}
	if (stacked && index > 0) {
		sTheme->TabLeft(active).IncludeIn(region, x, bar.y);
	} else {
		sTheme->Title(0, active).IncludeIn(region, x, bar.y);
		x += sTheme->Title(0, active).Width();
		sTheme->Title(1, active).IncludeIn(region, x, bar.y);
	}

	IncludeRow(region, sTheme->Title(2, active), slot.fillLeft, slot.fillLeft + slot.fillWidth, bar.y);
	const XfwmImage& end = EndOf(window, index, (int32)bar.slots.size(), active, stacked);
	end.IncludeIn(region, slot.x + slot.width - end.Width(), bar.y);
}


static void
xfwm_footprint(decor_window* window, decor_region* out)
{
	const decor_tab* top = TopTab(window);
	const int32 kind = FrameKind(top);
	const decor_rect frame = window->frame;

	BRegion region;
	if (kind == kFrameBordered) {
		const decor_rect* borders[] = { &window->left_border, &window->top_border, &window->right_border,
			&window->bottom_border };
		for (int32 i = 0; i < 4; i++) {
			if (IsValid(*borders[i]))
				region.Include(BRect(borders[i]->left, borders[i]->top, borders[i]->right, borders[i]->bottom));
		}
	} else if (kind == kFrameModal) {
		const bool active = Active(top);
		IncludeTiled(region, sTheme->Left(active), LeftArea(window, active), false);
		IncludeTiled(region, sTheme->Right(active), RightArea(window, active), false);
		IncludeTiled(region, sTheme->Bottom(active), BottomArea(window, active), true);
		IncludeTiled(region, sTheme->TopFrame(active), TopFrameArea(window, active), true);

		const int32 x0 = (int32)frame.left - sTheme->Left(active).Width();
		const int32 frameEnd = (int32)frame.right + 1 + sTheme->Right(active).Width();
		const int32 topY = (int32)frame.top - sTheme->TopFrame(active).Height();
		const int32 bottomEnd = (int32)frame.bottom + 1 + sTheme->Bottom(active).Height();
		sTheme->TopLeftCorner(active).IncludeIn(region, x0, topY);
		sTheme->TopRightCorner(active).IncludeIn(region, frameEnd - sTheme->TopRightCorner(active).Width(), topY);
		sTheme->BottomLeft(active).IncludeIn(region, x0, bottomEnd - sTheme->BottomLeft(active).Height());
		sTheme->BottomRight(active).IncludeIn(region, frameEnd - sTheme->BottomRight(active).Width(),
			bottomEnd - sTheme->BottomRight(active).Height());
	} else if (kind == kFrameTitled) {
		// (the text widths are not needed for the footprint's shape of a tab, but the slots are: measured with the
		// canvas-less estimate below)
		const bool active = Active(top);
		IncludeTiled(region, sTheme->Left(active), LeftArea(window, active), false);
		IncludeTiled(region, sTheme->Right(active), RightArea(window, active), false);
		IncludeTiled(region, sTheme->Bottom(active), BottomArea(window, active), true);

		// the slots come from the layout that was made: the tab rectangles
		BarLayout bar;
		bar.x0 = (int32)frame.left - sTheme->Left(active).Width();
		bar.y = (int32)frame.top - sTheme->TitleHeight();
		bar.frameEnd = (int32)frame.right + 1 + sTheme->Right(active).Width();
		bar.topRightX = bar.frameEnd - sTheme->TopRight(active).Width();
		const int32 overlap = StackOverlap(window);
		const int32 before = sTheme->Title(0, active).Width() + sTheme->Title(1, active).Width();
		const int32 after = sTheme->Title(3, active).Width();
		const int32 capWidth = sTheme->TopLeft(active).Width();
		for (int32 i = 0; i < window->tab_count; i++) {
			const decor_tab* tab = window->tabs[i];
			TabSlot slot;
			slot.x = (int32)tab->frame.left;
			slot.width = (int32)(tab->frame.right - tab->frame.left + 1) + (i < window->tab_count - 1 ? overlap : 0);
			slot.fillLeft = slot.x + (i == 0 ? capWidth : 0) + before;
			slot.fillWidth = std::max((int32)0, slot.width - ((i == 0 ? capWidth : 0) + before + after));
			slot.textLeft = slot.fillLeft;
			slot.textWidth = 0;
			bar.slots.push_back(slot);
		}
		bar.restStart = bar.slots.empty() ? bar.x0 : bar.slots.back().x + bar.slots.back().width;

		for (int32 i = 0; i < window->tab_count; i++)
			IncludeTab(window, region, i, bar);

		// the plain bar after the tabs, and its right end
		IncludeRow(region, sTheme->Title(4, active), bar.restStart, bar.topRightX, bar.y);
		sTheme->TopRight(active).IncludeIn(region, bar.topRightX, bar.y);

		const XfwmImage& bottomLeft = sTheme->BottomLeft(active);
		const XfwmImage& bottomRight = sTheme->BottomRight(active);
		const int32 bottomEnd = (int32)frame.bottom + 1 + sTheme->Bottom(active).Height();
		bottomLeft.IncludeIn(region, bar.x0, bottomEnd - bottomLeft.Height());
		bottomRight.IncludeIn(region, bar.frameEnd - bottomRight.Width(), bottomEnd - bottomRight.Height());

		if (top->look == B_DOCUMENT_WINDOW_LOOK) {
			const float knob = 18 - window->border_width;
			region.Include(BRect(frame.right - knob, frame.bottom - knob, frame.right, frame.bottom));
		}
	}

	for (int32 i = 0; i < region.CountRects(); i++) {
		const BRect rect = region.RectAt(i);
		out->include(out, Rect(rect.left, rect.top, rect.right, rect.bottom));
	}
}


// #pragma mark - the entry point


// The name of this add-on's file, found from the image that this function is in.
static bool
OwnFileName(BString& name)
{
	image_info info;
	int32 cookie = 0;
	const char* address = (const char*)&OwnFileName;
	while (get_next_image_info(B_CURRENT_TEAM, &cookie, &info) == B_OK) {
		if (address >= (const char*)info.text && address < (const char*)info.text + info.text_size) {
			const char* slash = strrchr(info.name, '/');
			name = slash != NULL ? slash + 1 : info.name;
			return true;
		}
	}
	return false;
}


extern "C" _EXPORT const decor_style*
instantiate_decor_style(uint32 apiVersion)
{
	if (apiVersion < 3)
		return NULL;

	if (sTheme == NULL) {
		BString file;
		if (!OwnFileName(file))
			return NULL;
		// XfwmStyle-b6 is the b6 theme
		BString theme(file);
		const int32 dash = theme.FindFirst('-');
		if (dash < 0)
			return NULL;
		theme.Remove(0, dash + 1);

		XfwmTheme* loaded = new(std::nothrow) XfwmTheme();
		if (loaded == NULL)
			return NULL;
		if (!loaded->Load(theme.String())) {
			delete loaded;
			return NULL;
		}
		sTheme = loaded;
	}

	memset(&sStyle, 0, sizeof(sStyle));
	sStyle.size = sizeof(sStyle);
	sStyle.api_version = apiVersion < DECOR_STYLE_API_VERSION ? apiVersion : DECOR_STYLE_API_VERSION;
	sStyle.name = "xfwm4 style";
	sStyle.create_window = xfwm_create_window;
	sStyle.delete_window = xfwm_delete_window;
	sStyle.update_font = xfwm_update_font;
	sStyle.layout = xfwm_layout;
	sStyle.draw_frame = xfwm_draw_frame;
	sStyle.draw_tab = xfwm_draw_tab;
	sStyle.draw_button = xfwm_draw_button;
	sStyle.region_at = xfwm_region_at;
	sStyle.footprint = xfwm_footprint;
	sStyle.update_colors = xfwm_update_colors;
	return &sStyle;
}
