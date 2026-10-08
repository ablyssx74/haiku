#include "XfwmDecorator.h"

#include <algorithm>
#include <new>
#include <string.h>

#include <WindowPrivate.h>

#include <Region.h>

#include "Desktop.h"
#include "DesktopSettings.h"
#include "DrawingEngine.h"
#include "DrawState.h"
#include "ServerBitmap.h"


// #pragma mark - XfwmDecorAddOn


XfwmDecorAddOn::XfwmDecorAddOn(image_id id, const char* name)
	:
	DecorAddOn(id, name)
{
	// "Xfwm-<theme>" is the file name; <theme> is the theme folder
	BString themeName(name);
	if (themeName.IFindFirst("xfwm-") == 0)
		themeName.Remove(0, 5);
	fTheme.Load(themeName.String());

}


status_t
XfwmDecorAddOn::InitCheck() const
{
	return fTheme.IsValid() ? B_OK : B_ERROR;
}


Decorator*
XfwmDecorAddOn::_AllocateDecorator(DesktopSettings& settings, BRect rect, Desktop* desktop)
{
	return new(std::nothrow) XfwmDecorator(settings, rect, desktop, &fTheme);
}


// #pragma mark - XfwmDecorator


XfwmDecorator::XfwmDecorator(DesktopSettings& settings, BRect frame, Desktop* desktop, const XfwmTheme* theme)
	:
	SATDecorator(settings, frame, desktop),
	fTheme(theme)
{
}


XfwmDecorator::~XfwmDecorator()
{
}


bool
XfwmDecorator::_HasTab() const
{
	if (fTopTab == NULL)
		return false;
	return fTopTab->look == B_TITLED_WINDOW_LOOK || fTopTab->look == B_DOCUMENT_WINDOW_LOOK
		|| fTopTab->look == B_FLOATING_WINDOW_LOOK;
}


bool
XfwmDecorator::_IsModal() const
{
	return fTopTab != NULL && fTopTab->look == B_MODAL_WINDOW_LOOK;
}


bool
XfwmDecorator::_IsBordered() const
{
	return fTopTab != NULL && fTopTab->look == B_BORDERED_WINDOW_LOOK;
}


int32
XfwmDecorator::_IndexOf(Decorator::Tab* tab) const
{
	for (int32 i = 0; i < fTabList.CountItems(); i++) {
		if (fTabList.ItemAt(i) == tab)
			return i;
	}
	return -1;
}


// The tab can't be slid (and so a stack's tabs can't be dragged into a new order): the title bar is part
// of the theme's artwork. When a slide ends, the layout is redone, as TabDecorator does.
bool
XfwmDecorator::_SetTabLocation(Decorator::Tab* tab, float location, bool isShifting, BRegion* updateRegion)
{
	if (_HasTab() && CountTabs() > 1 && !isShifting) {
		_DoTabLayout();
		if (updateRegion != NULL)
			updateRegion->Include(fTitleBarRect);
		return true;
	}
	return false;
}


bool
XfwmDecorator::_MoveTab(int32 from, int32 to, bool isMoving, BRegion* updateRegion)
{
	return false;
}


void
XfwmDecorator::_SetFocus(Decorator::Tab* tab)
{
	tab->buttonFocus = IsFocus(tab)
		|| ((tab->look == B_FLOATING_WINDOW_LOOK) && (tab->flags & B_AVOID_FOCUS) != 0);

	if (_HasTab() && tab != NULL) {
		// the active and inactive pictures differ, and only the focused tab has buttons
		_DoTabLayout();
		_InvalidateFootprint();
	}
}


// #pragma mark - layout


void
XfwmDecorator::_DoLayout()
{
	if (_IsModal()) {
		// no title bar: a border all the way round, the top one made from the bottom border turned over
		const bool active = _Active(fTopTab);
		const int32 bw = fTheme->BorderWidth();
		fBorderWidth = bw;
		fResizeKnobSize = 18;
		fBorderResizeLength = 22;

		fLeftBorder.Set(fFrame.left - bw, fFrame.top, fFrame.left - 1, fFrame.bottom);
		fRightBorder.Set(fFrame.right + 1, fFrame.top, fFrame.right + bw, fFrame.bottom);
		fTopBorder.Set(fFrame.left - bw, fFrame.top - bw, fFrame.right + bw, fFrame.top - 1);
		fBottomBorder.Set(fFrame.left - bw, fFrame.bottom + 1, fFrame.right + bw, fFrame.bottom + bw);
		fResizeRect.Set(0, 0, -1, -1);

		for (int32 i = 0; i < fTabList.CountItems(); i++)
			fTabList.ItemAt(i)->tabRect.Set(0, 0, -1, -1);
		fTitleBarRect.Set(0, 0, -1, -1);
		fTabsRegion.MakeEmpty();

		const int32 x0 = (int32)fFrame.left - fTheme->Left(active).Width();
		const int32 frameEnd = (int32)fFrame.right + 1 + fTheme->Right(active).Width();
		const int32 top = (int32)fFrame.top - fTheme->TopFrame(active).Height();
		fBorderRect.Set(x0, top, frameEnd - 1, fFrame.bottom + fTheme->Bottom(active).Height());
		return;
	}

	if (!_HasTab()) {
		SATDecorator::_DoLayout();
		return;
	}

	const int32 bw = fTheme->BorderWidth();
	fBorderWidth = bw;
	fResizeKnobSize = 18;
	fBorderResizeLength = 22;

	fLeftBorder.Set(fFrame.left - bw, fFrame.top, fFrame.left - 1, fFrame.bottom);
	fRightBorder.Set(fFrame.right + 1, fFrame.top, fFrame.right + bw, fFrame.bottom);
	fBottomBorder.Set(fFrame.left - bw, fFrame.bottom + 1, fFrame.right + bw, fFrame.bottom + bw);
	fTopBorder.Set(0, 0, -1, -1);
		// the title bar is the top border

	if (fTopTab->look == B_DOCUMENT_WINDOW_LOOK) {
		fResizeRect.Set(fBottomBorder.right - fResizeKnobSize, fBottomBorder.bottom - fResizeKnobSize,
			fBottomBorder.right, fBottomBorder.bottom);
	} else
		fResizeRect.Set(0, 0, -1, -1);

	_DoTabLayout();

	fBorderRect = BRect(fFrame.left - bw - fTheme->LeftMargin(), fFrame.top - fTheme->TitleHeight(),
		fFrame.right + bw + 1, fBottomBorder.bottom + 1);
}


// Where the tabs go. xfwm4 lays the frame out by the full size of the border pictures (a left picture 14 wide
// is a 14 wide border, transparent part included), so the pieces are anchored to that outer frame.
void
XfwmDecorator::_ComputeBar(BarLayout& bar) const
{
	const bool active = _Active(fTopTab);
	const int32 bw = fTheme->BorderWidth();
	const XfwmImage& topLeft = fTheme->TopLeft(active);
	const XfwmImage& topRight = fTheme->TopRight(active);

	bar.x0 = (int32)fFrame.left - fTheme->Left(active).Width();
	bar.y = (int32)fFrame.top - fTheme->TitleHeight();
	bar.right = (int32)fFrame.right + bw;
	bar.frameEnd = (int32)fFrame.right + 1 + fTheme->Right(active).Width();
	bar.topRightX = bar.frameEnd - topRight.Width();
	bar.slots.clear();

	const int32 count = fTabList.CountItems();
	if (count == 0) {
		bar.restStart = bar.x0;
		return;
	}

	const int32 capWidth = topLeft.Width();
	const int32 before = fTheme->Title(0, active).Width() + fTheme->Title(1, active).Width();
	const int32 after = fTheme->Title(3, active).Width();
	const int32 available = std::max((int32)0, bar.topRightX - bar.x0);

	std::vector<int32> natural(count), fixed(count);
	int32 sum = 0;
	for (int32 i = 0; i < count; i++) {
		Decorator::Tab* tab = fTabList.ItemAt(i);
		fixed[i] = (i == 0 ? capWidth : 0) + before + after;
		float wanted = tab->title.Length() > 0
			? fDrawState.Font().StringWidth(tab->title.String(), tab->title.Length()) : 0.0f;
		natural[i] = fixed[i] + (int32)ceilf(wanted);
		sum += natural[i];
	}

	// a theme with a full width title has one tab across the whole bar: any room left is shared out
	std::vector<int32> extra(count, 0);
	if (fTheme->FullWidthTitle() && sum < available) {
		int32 spare = available - sum;
		for (int32 i = 0; i < count; i++)
			extra[i] = spare / count + (i == count - 1 ? spare % count : 0);
	}

	int32 x = bar.x0;
	for (int32 i = 0; i < count; i++) {
		int32 width = natural[i] + extra[i];
		if (sum > available && sum > 0) {
			// too many or too long: every tab gets a share of the room, but keeps its edges
			width = std::max(fixed[i], (int32)((int64)available * natural[i] / sum));
		}
		if (x + width > bar.x0 + available)
			width = std::max(fixed[i], bar.x0 + available - x);

		TabSlot slot;
		slot.x = x;
		slot.width = width;
		slot.textLeft = x + (i == 0 ? capWidth : 0) + before;
		slot.textWidth = std::max((int32)0, width - fixed[i]);
		slot.fillLeft = slot.textLeft;
		slot.fillWidth = slot.textWidth;
		bar.slots.push_back(slot);
		x += width;
	}
	bar.restStart = x;

	// the title text goes between the buttons, left side and right side alike
	int32 leftEnd, rightStart;
	_ButtonExtents(leftEnd, rightStart);
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


void
XfwmDecorator::_ButtonExtents(int32& leftEnd, int32& rightStart) const
{
	leftEnd = (int32)fFrame.left;
	rightStart = (int32)fFrame.right + 1;
	if (fTopTab == NULL)
		return;

	const BString layout = fTheme->ButtonLayout();
	int32 divider = layout.FindFirst('|');
	if (divider < 0)
		divider = layout.Length();

	int32 leftCount = 0, rightCount = 0;
	for (int32 i = 0; i < layout.Length(); i++) {
		bool present = false;
		switch (layout[i]) {
			case 'C':
				present = (fTopTab->flags & B_NOT_CLOSABLE) == 0;
				break;
			case 'M':
				present = (fTopTab->flags & B_NOT_ZOOMABLE) == 0;
				break;
			case 'H':
				present = (fTopTab->flags & B_NOT_MINIMIZABLE) == 0;
				break;
			default:
				break;
		}
		if (!present)
			continue;
		if (i < divider)
			leftCount++;
		else if (i > divider)
			rightCount++;
	}

	const int32 width = fTheme->ButtonWidth();
	const int32 spacing = fTheme->ButtonSpacing();
	const int32 offset = fTheme->ButtonOffset();
	if (leftCount > 0)
		leftEnd = (int32)fFrame.left + offset + leftCount * (width + spacing) - spacing;
	if (rightCount > 0)
		rightStart = (int32)fFrame.right - offset - (rightCount - 1) * (width + spacing) - width + 1;
}


void
XfwmDecorator::_DoTabLayout()
{
	BarLayout bar;
	_ComputeBar(bar);

	const int32 bw = fTheme->BorderWidth();
	const int32 height = std::max(fTheme->TitleHeight(), fTheme->TopLeft(true).Height());
	fTitleBarRect.Set(bar.x0, bar.y, bar.right + 1, bar.y + height - 1);

	for (int32 i = 0; i < fTabList.CountItems(); i++) {
		Decorator::Tab* tab = fTabList.ItemAt(i);
		const TabSlot& slot = bar.slots[i];

		tab->tabRect.Set(slot.x, bar.y, slot.x + slot.width - 1, bar.y + fTheme->TitleHeight() - 1);
		tab->textOffset = 0;
		tab->tabOffset = (uint32)std::max((int32)0, slot.x - (int32)fLeftBorder.left);
		tab->minTabSize = 0;
		tab->maxTabSize = tab->tabRect.Width();

		// the title, cut to the room it has
		tab->truncatedTitle = tab->title;
		fDrawState.Font().TruncateString(&tab->truncatedTitle, B_TRUNCATE_END, slot.textWidth);
		tab->truncatedTitleLength = tab->truncatedTitle.Length();

		tab->closeRect.Set(0, 0, -1, -1);
		tab->zoomRect.Set(0, 0, -1, -1);
		tab->minimizeRect.Set(0, 0, -1, -1);
	}

	// the buttons are at the right end of the bar and belong to the front tab
	if (fTopTab != NULL)
		_LayoutButtons(fTopTab, bar);

	(void)bw;
	fTabsRegion.MakeEmpty();
	BRegion bit;
	_GetFootprint(&bit);
	fTabsRegion = bit;
	fUnreported.Include(&fShown);
	fShown = bit;
}


void
XfwmDecorator::_LayoutButtons(Decorator::Tab* tab, const BarLayout& bar)
{
	const bool active = _Active(tab);
	const BString layout = fTheme->ButtonLayout();
	int32 divider = layout.FindFirst('|');
	if (divider < 0)
		divider = layout.Length();

	// the letters of a side, in order; a button the window doesn't have takes no room
	struct Placement {
		int32	button;
		BRect*	rect;
	};
	Placement left[4], right[4];
	int32 leftCount = 0, rightCount = 0;
	for (int32 i = 0; i < layout.Length(); i++) {
		int32 button = -1;
		BRect* rect = NULL;
		switch (layout[i]) {
			case 'C':
				if ((tab->flags & B_NOT_CLOSABLE) == 0) {
					button = kButtonClose;
					rect = &tab->closeRect;
				}
				break;
			case 'M':
				if ((tab->flags & B_NOT_ZOOMABLE) == 0) {
					button = kButtonMaximize;
					rect = &tab->zoomRect;
				}
				break;
			case 'H':
				if ((tab->flags & B_NOT_MINIMIZABLE) == 0) {
					button = kButtonHide;
					rect = &tab->minimizeRect;
				}
				break;
			default:
				break;
		}
		if (button < 0)
			continue;
		Placement placement = {button, rect};
		if (i < divider && leftCount < 4)
			left[leftCount++] = placement;
		else if (i > divider && rightCount < 4)
			right[rightCount++] = placement;
	}

	const int32 width = fTheme->ButtonWidth();
	const int32 spacing = fTheme->ButtonSpacing();
	const int32 offset = fTheme->ButtonOffset();

	// right side: from the inner edge of the right border (the frame's right edge), left side likewise
	int32 x = (int32)fFrame.right - offset;
	for (int32 i = rightCount - 1; i >= 0; i--) {
		const XfwmImage& image = fTheme->Button(right[i].button, active ? kStateActive : kStateInactive);
		BRect opaque = image.OpaqueBounds();
		right[i].rect->Set(x - width + 1, bar.y + opaque.top, x, bar.y + opaque.bottom);
		x -= width + spacing;
	}

	x = (int32)fFrame.left + offset;
	for (int32 i = 0; i < leftCount; i++) {
		const XfwmImage& image = fTheme->Button(left[i].button, active ? kStateActive : kStateInactive);
		BRect opaque = image.OpaqueBounds();
		left[i].rect->Set(x, bar.y + opaque.top, x + width - 1, bar.y + opaque.bottom);
		x += width + spacing;
	}
}


void
XfwmDecorator::_ResizeBy(BPoint offset, BRegion* dirty)
{
	if (!_HasTab() && !_IsModal()) {
		SATDecorator::_ResizeBy(offset, dirty);
		return;
	}

	const BRect oldBar = fTitleBarRect;
	BRegion before(fUnreported);
	before.Include(&fShown);
	if (_IsModal() && dirty != NULL)
		_GetFootprint(&before);

	fFrame.right += offset.x;
	fFrame.bottom += offset.y;
	_DoLayout();
	_InvalidateFootprint();

	fUnreported.MakeEmpty();
	if (dirty != NULL) {
		BRegion after;
		_GetFootprint(&after);
		dirty->Include(&before);
		dirty->Include(&after);
		dirty->Include(oldBar.InsetByCopy(-2, -2) | fTitleBarRect.InsetByCopy(-2, -2));
	}
}


void
XfwmDecorator::_SetTitle(Decorator::Tab* tab, const char* string, BRegion* updateRegion)
{
	if (!_HasTab()) {
		SATDecorator::_SetTitle(tab, string, updateRegion);
		return;
	}

	// the title is already the new one, so the old area is the one remembered from the last layout
	const BRect oldBar = fTitleBarRect;
	BRegion before(fUnreported);
	before.Include(&fShown);

	_DoLayout();
	_DoOutlineLayout();
	_InvalidateFootprint();

	fUnreported.MakeEmpty();
	if (updateRegion != NULL) {
		BRegion after;
		_GetFootprint(&after);
		updateRegion->Include(&before);
		updateRegion->Include(&after);
		updateRegion->Include(oldBar.InsetByCopy(-2, -2) | fTitleBarRect.InsetByCopy(-2, -2));
	}
}


void
XfwmDecorator::_IncludeTab(BRegion& region, Decorator::Tab* tab, const BarLayout& bar) const
{
	const int32 index = _IndexOf(tab);
	if (index < 0 || index >= (int32)bar.slots.size())
		return;

	const TabSlot& slot = bar.slots[index];
	const bool active = _Active(tab);
	int32 x = slot.x;
	if (index == 0) {
		fTheme->TopLeft(active).IncludeIn(region, x, bar.y);
		x += fTheme->TopLeft(active).Width();
	}
	fTheme->Title(0, active).IncludeIn(region, x, bar.y);
	x += fTheme->Title(0, active).Width();
	fTheme->Title(1, active).IncludeIn(region, x, bar.y);

	const XfwmImage& middle = fTheme->Title(2, active);
	if (middle.IsValid()) {
		for (int32 tx = slot.fillLeft; tx < slot.fillLeft + slot.fillWidth; tx += middle.Width())
			middle.IncludeIn(region, tx, bar.y);
	}
	fTheme->Title(3, active).IncludeIn(region, slot.x + slot.width - fTheme->Title(3, active).Width(), bar.y);
}


void
XfwmDecorator::_GetFootprint(BRegion* region)
{
	if (region == NULL)
		return;
	if (_IsModal()) {
		const bool active = _Active(fTopTab);
		_IncludeTiled(*region, fTheme->Left(active), _LeftArea(active), false);
		_IncludeTiled(*region, fTheme->Right(active), _RightArea(active), false);
		_IncludeTiled(*region, fTheme->Bottom(active), _BottomArea(active), true);
		_IncludeTiled(*region, fTheme->TopFrame(active), _TopFrameArea(active), true);

		const int32 x0 = (int32)fFrame.left - fTheme->Left(active).Width();
		const int32 frameEnd = (int32)fFrame.right + 1 + fTheme->Right(active).Width();
		const int32 top = (int32)fFrame.top - fTheme->TopFrame(active).Height();
		const int32 bottomEnd = (int32)fFrame.bottom + 1 + fTheme->Bottom(active).Height();
		fTheme->TopLeftCorner(active).IncludeIn(*region, x0, top);
		fTheme->TopRightCorner(active).IncludeIn(*region, frameEnd - fTheme->TopRightCorner(active).Width(), top);
		fTheme->BottomLeft(active).IncludeIn(*region, x0, bottomEnd - fTheme->BottomLeft(active).Height());
		fTheme->BottomRight(active).IncludeIn(*region, frameEnd - fTheme->BottomRight(active).Width(),
			bottomEnd - fTheme->BottomRight(active).Height());
		return;
	}
	if (!_HasTab()) {
		SATDecorator::_GetFootprint(region);
		return;
	}

	const bool active = _Active(fTopTab);
	_IncludeTiled(*region, fTheme->Left(active), _LeftArea(active), false);
	_IncludeTiled(*region, fTheme->Right(active), _RightArea(active), false);
	_IncludeTiled(*region, fTheme->Bottom(active), _BottomArea(active), true);

	BarLayout bar;
	_ComputeBar(bar);

	for (int32 i = 0; i < fTabList.CountItems(); i++)
		_IncludeTab(*region, fTabList.ItemAt(i), bar);

	// the plain bar after the tabs, and its right end
	const XfwmImage& rest = fTheme->Title(4, active);
	if (rest.IsValid()) {
		for (int32 tx = bar.restStart; tx < bar.topRightX; tx += rest.Width())
			rest.IncludeIn(*region, tx, bar.y);
	}
	fTheme->TopRight(active).IncludeIn(*region, bar.topRightX, bar.y);

	const XfwmImage& bottomLeft = fTheme->BottomLeft(active);
	const XfwmImage& bottomRight = fTheme->BottomRight(active);
	const int32 bottomEnd = (int32)fFrame.bottom + 1 + fTheme->Bottom(active).Height();
	bottomLeft.IncludeIn(*region, bar.x0, bottomEnd - bottomLeft.Height());
	bottomRight.IncludeIn(*region, bar.frameEnd - bottomRight.Width(), bottomEnd - bottomRight.Height());

	if (fTopTab->look == B_DOCUMENT_WINDOW_LOOK) {
		float knob = fResizeKnobSize - fBorderWidth;
		region->Include(BRect(fFrame.right - knob, fFrame.bottom - knob, fFrame.right, fFrame.bottom));
	}
}


Decorator::Region
XfwmDecorator::RegionAt(BPoint where, int32& tab) const
{
	if (_HasTab()) {
		// the base class only knows the close and zoom buttons
		for (int32 i = 0; i < fTabList.CountItems(); i++) {
			Decorator::Tab* candidate = fTabList.ItemAt(i);
			if ((candidate->flags & B_NOT_MINIMIZABLE) == 0 && candidate->minimizeRect.IsValid()
				&& candidate->minimizeRect.Contains(where)) {
				tab = i;
				return REGION_MINIMIZE_BUTTON;
			}
		}
	}

	Region region = SATDecorator::RegionAt(where, tab);
	if (region != REGION_NONE || !_HasTab())
		return region;

	// the part of the cap that sticks out to the left of the window, and the plain bar beyond the tabs:
	// both drag the window (the front tab's)
	if (fTitleBarRect.Contains(where) && where.y < fFrame.top) {
		tab = _IndexOf(fTopTab);
		if (tab < 0)
			tab = 0;
		return REGION_TAB;
	}
	if (fTitleBarRect.Contains(where) && where.x < fLeftBorder.left) {
		tab = 0;
		return REGION_LEFT_BORDER;
	}
	return REGION_NONE;
}


// #pragma mark - drawing


void
XfwmDecorator::_Blit(const XfwmImage& image, BPoint at)
{
	if (!image.IsValid())
		return;
	BRect source(0, 0, image.Width() - 1, image.Height() - 1);
	fDrawingEngine->DrawBitmap(image.Bitmap(), source, source.OffsetToCopy(at));
}


BRect
XfwmDecorator::_LeftArea(bool active) const
{
	const XfwmImage& left = fTheme->Left(active);
	return BRect(fFrame.left - left.Width(), fFrame.top, fFrame.left - 1, fFrame.bottom);
}


BRect
XfwmDecorator::_RightArea(bool active) const
{
	const XfwmImage& right = fTheme->Right(active);
	float x = fFrame.right + 1 - right.OpaqueBounds().left;
	return BRect(x, fFrame.top, x + right.Width() - 1, fFrame.bottom);
}


BRect
XfwmDecorator::_BottomArea(bool active) const
{
	const XfwmImage& bottom = fTheme->Bottom(active);
	const int32 bw = fTheme->BorderWidth();
	float y = fFrame.bottom + 1 - bottom.OpaqueBounds().top;
	return BRect(fFrame.left - bw, y, fFrame.right + bw, y + bottom.Height() - 1);
}


BRect
XfwmDecorator::_TopFrameArea(bool active) const
{
	const XfwmImage& topFrame = fTheme->TopFrame(active);
	const int32 bw = fTheme->BorderWidth();
	float y = fFrame.top - topFrame.Height();
	return BRect(fFrame.left - bw, y, fFrame.right + bw, y + topFrame.Height() - 1);
}


void
XfwmDecorator::_IncludeTiled(BRegion& region, const XfwmImage& image, BRect area, bool horizontal) const
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


void
XfwmDecorator::_BlitTiled(const XfwmImage& image, BRect area, bool horizontal)
{
	if (!image.IsValid() || !area.IsValid())
		return;

	if (horizontal) {
		for (float x = area.left; x <= area.right; x += image.Width()) {
			float width = std::min((float)image.Width(), area.right - x + 1);
			fDrawingEngine->DrawBitmap(image.Bitmap(), BRect(0, 0, width - 1, image.Height() - 1),
				BRect(x, area.top, x + width - 1, area.top + image.Height() - 1));
		}
	} else {
		for (float y = area.top; y <= area.bottom; y += image.Height()) {
			float height = std::min((float)image.Height(), area.bottom - y + 1);
			fDrawingEngine->DrawBitmap(image.Bitmap(), BRect(0, 0, image.Width() - 1, height - 1),
				BRect(area.left, y, area.left + image.Width() - 1, y + height - 1));
		}
	}
}


// The plain bar after the last tab and the bar's right end.
void
XfwmDecorator::_DrawBarEnd(const BarLayout& bar)
{
	const bool active = _Active(fTopTab);
	drawing_mode oldMode;
	fDrawingEngine->SetDrawingMode(B_OP_ALPHA, oldMode);
	fDrawingEngine->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);

	const XfwmImage& rest = fTheme->Title(4, active);
	if (bar.restStart < bar.topRightX)
		_BlitTiled(rest, BRect(bar.restStart, bar.y, bar.topRightX - 1, bar.y + rest.Height() - 1), true);
	_Blit(fTheme->TopRight(active), BPoint(bar.topRightX, bar.y));

	fDrawingEngine->SetDrawingMode(oldMode);
}


void
XfwmDecorator::_DrawFrame(BRect invalid)
{
	if (_IsBordered()) {
		// menus and the like: a one pixel line in the theme's outline colour
		fDrawingEngine->StrokeRect(BRect(fFrame.left - 1, fFrame.top - 1, fFrame.right + 1, fFrame.bottom + 1),
			fTheme->OutlineColor());
		return;
	}

	if (!_HasTab() && !_IsModal()) {
		SATDecorator::_DrawFrame(invalid);
		return;
	}

	const bool active = _Active(fTopTab);
	const int32 bw = fTheme->BorderWidth();

	drawing_mode oldMode;
	fDrawingEngine->SetDrawingMode(B_OP_ALPHA, oldMode);
	fDrawingEngine->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);

	const XfwmImage& left = fTheme->Left(active);
	const XfwmImage& right = fTheme->Right(active);
	const XfwmImage& bottom = fTheme->Bottom(active);

	_BlitTiled(left, _LeftArea(active), false);
	_BlitTiled(right, _RightArea(active), false);
	_BlitTiled(bottom, _BottomArea(active), true);

	if (fTopTab->look == B_DOCUMENT_WINDOW_LOOK) {
		// the resize knob inside the frame's bottom right corner
		float knob = fResizeKnobSize - fBorderWidth;
		fDrawingEngine->FillRect(BRect(fFrame.right - knob, fFrame.bottom - knob, fFrame.right, fFrame.bottom),
			fTheme->FillColor());
	}

	const XfwmImage& bottomLeft = fTheme->BottomLeft(active);
	const XfwmImage& bottomRight = fTheme->BottomRight(active);
	BarLayout bar;
	_ComputeBar(bar);
	const int32 bottomEnd = (int32)fFrame.bottom + 1 + bottom.Height();
	_Blit(bottomLeft, BPoint(bar.x0, bottomEnd - bottomLeft.Height()));
	_Blit(bottomRight, BPoint(bar.frameEnd - bottomRight.Width(), bottomEnd - bottomRight.Height()));

	if (_IsModal()) {
		// the top border and its corners
		const XfwmImage& topFrame = fTheme->TopFrame(active);
		const int32 top = (int32)fFrame.top - topFrame.Height();
		_BlitTiled(topFrame, _TopFrameArea(active), true);
		_Blit(fTheme->TopLeftCorner(active), BPoint(bar.x0, top));
		_Blit(fTheme->TopRightCorner(active), BPoint(bar.frameEnd - fTheme->TopRightCorner(active).Width(), top));
	}

	fDrawingEngine->SetDrawingMode(oldMode);

	if (_HasTab())
		_DrawBarEnd(bar);
}


void
XfwmDecorator::_DrawTab(Decorator::Tab* tab, BRect invalid)
{
	if (!_HasTab()) {
		SATDecorator::_DrawTab(tab, invalid);
		return;
	}

	const int32 index = _IndexOf(tab);
	BarLayout bar;
	_ComputeBar(bar);
	if (index < 0 || index >= (int32)bar.slots.size())
		return;

	const TabSlot& slot = bar.slots[index];
	const bool active = _Active(tab);

	drawing_mode oldMode;
	fDrawingEngine->SetDrawingMode(B_OP_ALPHA, oldMode);
	fDrawingEngine->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);

	int32 x = slot.x;
	if (index == 0) {
		_Blit(fTheme->TopLeft(active), BPoint(x, bar.y));
		x += fTheme->TopLeft(active).Width();
	}
	_Blit(fTheme->Title(0, active), BPoint(x, bar.y));
	x += fTheme->Title(0, active).Width();
	_Blit(fTheme->Title(1, active), BPoint(x, bar.y));

	const XfwmImage& middle = fTheme->Title(2, active);
	if (slot.fillWidth > 0) {
		_BlitTiled(middle, BRect(slot.fillLeft, bar.y, slot.fillLeft + slot.fillWidth - 1,
			bar.y + middle.Height() - 1), true);
	}
	_Blit(fTheme->Title(3, active), BPoint(slot.x + slot.width - fTheme->Title(3, active).Width(), bar.y));

	fDrawingEngine->SetDrawingMode(oldMode);

	_DrawTitle(tab, invalid);
	if (tab == fTopTab) {
		_DrawBarEnd(bar);
		_DrawButtons(tab, invalid);
	}
}


void
XfwmDecorator::_DrawTitle(Decorator::Tab* tab, BRect)
{
	if (!_HasTab())
		return;

	const int32 index = _IndexOf(tab);
	BarLayout bar;
	_ComputeBar(bar);
	if (index < 0 || index >= (int32)bar.slots.size())
		return;
	const bool active = _Active(tab);

	font_height fontHeight;
	fDrawState.Font().GetHeight(fontHeight);

	fDrawingEngine->SetDrawingMode(B_OP_OVER);
	fDrawingEngine->SetHighColor(fTheme->TextColor(active));
	fDrawingEngine->SetFont(fDrawState.Font());

	float textHeight = fontHeight.ascent + fontHeight.descent;
	// where the title sits in the room it has, as the theme's title_alignment says
	float room = bar.slots[index].textWidth;
	float used = fDrawState.Font().StringWidth(tab->truncatedTitle.String(), tab->truncatedTitleLength);
	float shift = 0;
	if (fTheme->TitleAlignment() == 1)
		shift = (room - used) / 2;
	else if (fTheme->TitleAlignment() == 2)
		shift = room - used;
	if (shift < 0)
		shift = 0;

	BPoint where(bar.slots[index].textLeft + floorf(shift),
		floorf(bar.y + (fTheme->TitleHeight() - textHeight) / 2 + fontHeight.ascent
			+ fTheme->TitleOffset(active) + 0.5f));
	fDrawingEngine->DrawString(tab->truncatedTitle.String(), tab->truncatedTitleLength, where);

	fDrawingEngine->SetDrawingMode(B_OP_COPY);
}


void
XfwmDecorator::_DrawButtons(Decorator::Tab* tab, const BRect& invalid)
{
	if ((tab->flags & B_NOT_CLOSABLE) == 0 && invalid.Intersects(tab->closeRect))
		_DrawClose(tab, false, tab->closeRect);
	if ((tab->flags & B_NOT_ZOOMABLE) == 0 && invalid.Intersects(tab->zoomRect))
		_DrawZoom(tab, false, tab->zoomRect);
	if ((tab->flags & B_NOT_MINIMIZABLE) == 0 && invalid.Intersects(tab->minimizeRect))
		_DrawMinimize(tab, false, tab->minimizeRect);
}


void
XfwmDecorator::_DrawButton(Decorator::Tab* tab, int32 button, bool pressed, BRect rect, bool direct)
{
	if (!rect.IsValid())
		return;

	const XfwmImage& image
		= fTheme->Button(button, pressed ? kStatePressed : (_Active(tab) ? kStateActive : kStateInactive));
	if (!image.IsValid())
		return;

	bool copyToFront = fDrawingEngine->CopyToFrontEnabled();
	fDrawingEngine->SetCopyToFrontEnabled(direct);
	drawing_mode oldMode;
	fDrawingEngine->SetDrawingMode(B_OP_ALPHA, oldMode);
	fDrawingEngine->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
	_Blit(image, BPoint(rect.left, rect.top - image.OpaqueBounds().top));
	fDrawingEngine->SetDrawingMode(oldMode);
	fDrawingEngine->SetCopyToFrontEnabled(copyToFront);
}


void
XfwmDecorator::_DrawClose(Decorator::Tab* tab, bool direct, BRect rect)
{
	if (!_HasTab()) {
		SATDecorator::_DrawClose(tab, direct, rect);
		return;
	}
	_DrawButton(tab, kButtonClose, tab->closePressed, rect, direct);
}


void
XfwmDecorator::_DrawZoom(Decorator::Tab* tab, bool direct, BRect rect)
{
	if (!_HasTab()) {
		SATDecorator::_DrawZoom(tab, direct, rect);
		return;
	}
	_DrawButton(tab, kButtonMaximize, tab->zoomPressed, rect, direct);
}


void
XfwmDecorator::_DrawMinimize(Decorator::Tab* tab, bool direct, BRect rect)
{
	if (!_HasTab()) {
		SATDecorator::_DrawMinimize(tab, direct, rect);
		return;
	}
	_DrawButton(tab, kButtonHide, tab->minimizePressed, rect, direct);
}


// #pragma mark - the add-on's entry point


extern "C" DecorAddOn*
instantiate_decor_addon(image_id id, const char* name)
{
	return new(std::nothrow) XfwmDecorAddOn(id, name);
}
