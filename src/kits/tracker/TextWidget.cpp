/*
Open Tracker License

Terms and Conditions

Copyright (c) 1991-2000, Be Incorporated. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice applies to all licensees
and shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF TITLE, MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
BE INCORPORATED BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF, OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Be Incorporated shall not be
used in advertising or otherwise to promote the sale, use or other dealings in
this Software without prior written authorization from Be Incorporated.

Tracker(TM), Be(R), BeOS(R), and BeIA(TM) are trademarks or registered trademarks
of Be Incorporated in the United States and other countries. Other brand product
names are registered trademarks or trademarks of their respective holders.
All rights reserved.
*/


#include "TextWidget.h"

#include <math.h>
#include <algorithm>
#include <string.h>
#include <stdlib.h>

#include <Alert.h>
#include <Catalog.h>
#include <Clipboard.h>
#include <Debug.h>
#include <Directory.h>
#include <MessageFilter.h>
#include <Region.h>
#include <ScrollView.h>
#include <TextView.h>
#include <Volume.h>
#include <Window.h>

#include "Attributes.h"
#include "ContainerWindow.h"
#include "Commands.h"
#include "FSUtils.h"
#include "PoseView.h"
#include "SnakeSelector.h"
#include "Utilities.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "TextWidget"


const float kWidthMargin = 20;
const float kPillPadding = 4;


// The box a file name is edited in: the label's pill is drawn behind it by the widget, so the box has no border of its
// own, and it asks for the pose view to redraw that pill whenever it changes size or place.
class EditBorderView : public BScrollView {
public:
	EditBorderView(BView* target)
		:
		BScrollView("BorderView", target, 0, 0, false, false, B_NO_BORDER)
	{
		SetFlags(Flags() | B_FRAME_EVENTS);
	}

	virtual void AttachedToWindow()
	{
		BScrollView::AttachedToWindow();
		fLastFrame = Frame();
	}

	virtual void FrameResized(float width, float height)
	{
		BScrollView::FrameResized(width, height);
		_Redraw();
	}

	virtual void FrameMoved(BPoint where)
	{
		BScrollView::FrameMoved(where);
		_Redraw();
	}

private:
	// both where the box was and where it is now: the pill behind it moves with it (it shrinks when text is
	// deleted), and what it leaves behind has to be drawn again
	void _Redraw()
	{
		if (Parent() == NULL)
			return;
		const BRect now = Frame();
		BRect area = fLastFrame.IsValid() ? (fLastFrame | now) : now;
		fLastFrame = now;
		Parent()->Invalidate(area.InsetByCopy(-(kPillPadding + 4), -3));
	}

	BRect	fLastFrame;
};


// The text view of the edit box. Haiku shows the selection by inverting it, which turns the pill's light colour into a
// dark block: the inversion is undone and the selection is drawn as a tint of the accent instead.
class PillTextView : public BTextView {
public:
	PillTextView(BRect frame, const char* name, BRect textRect, const BFont* font, const rgb_color* color)
		:
		BTextView(frame, name, textRect, font, color, B_FOLLOW_ALL, B_WILL_DRAW)
	{
	}

	virtual void Draw(BRect updateRect)
	{
		// the pill behind the box moves and changes size with it (the frame hooks are not always called): when the
		// box is somewhere else than it was drawn last, what was there is drawn again
		BView* box = Parent();
		BView* poseView = box != NULL ? box->Parent() : NULL;
		if (box != NULL && poseView != NULL) {
			const BRect now = box->Frame();
			if (now != fLastBox) {
				const BRect area = fLastBox.IsValid() ? (fLastBox | now) : now;
				fLastBox = now;
				poseView->Invalidate(area.InsetByCopy(-(kPillPadding + 4), -3));
			}
		}

		BTextView::Draw(updateRect);

		int32 start, end;
		GetSelection(&start, &end);
		if (start == end || !IsFocus() || Window() == NULL || !Window()->IsActive())
			return;

		BRegion region;
		GetTextRegion(start, end, &region);
		PushState();
		SetDrawingMode(B_OP_INVERT);
		FillRegion(&region, B_SOLID_HIGH);
		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_OVERLAY);
		rgb_color tint = SnakeSelector::Accent();
		tint.alpha = 110;
		SetHighColor(tint);
		FillRegion(&region);
		PopState();
	}

	// the selection is drawn in place by the base class when it changes: draw the whole thing again
	virtual void Select(int32 startOffset, int32 endOffset)
	{
		BTextView::Select(startOffset, endOffset);
		Invalidate();
	}

	virtual void MakeFocus(bool focus = true)
	{
		BTextView::MakeFocus(focus);
		Invalidate();
	}

	virtual void MouseUp(BPoint where)
	{
		BTextView::MouseUp(where);
		Invalidate();
	}

private:
	BRect	fLastBox;
};


//	#pragma mark - BTextWidget


BTextWidget::BTextWidget(Model* model, BColumn* column, BPoseView* view)
	:
	fText(WidgetAttributeText::NewWidgetText(model, column, view)),
	fAttrHash(column->AttrHash()),
	fAlignment(column->Alignment()),
	fEditable(column->Editable()),
	fVisible(true),
	fActive(false),
	fSymLink(model->IsSymLink()),
	fMaxWidth(0),
	fLastClickedTime(0)
{
}


BTextWidget::~BTextWidget()
{
	if (fLastClickedTime != 0)
		fParams.poseView->SetTextWidgetToCheck(NULL, this);

	delete fText;
}


int
BTextWidget::Compare(const BTextWidget& with, BPoseView* view) const
{
	return fText->Compare(*with.fText, view);
}


const char*
BTextWidget::Text(const BPoseView* view) const
{
	StringAttributeText* textAttribute = dynamic_cast<StringAttributeText*>(fText);
	if (textAttribute == NULL)
		return NULL;

	return textAttribute->ValueAsText(view);
}


float
BTextWidget::TextWidth(const BPoseView* pose) const
{
	return fText->Width(pose);
}


float
BTextWidget::PreferredWidth(const BPoseView* pose) const
{
	return fText->PreferredWidth(pose);
}


BRect
BTextWidget::ColumnRect(BPoint poseLoc, const BColumn* column,
	const BPoseView* view)
{
	if (view->ViewMode() != kListMode) {
		// ColumnRect only makes sense in list view, return
		// CalcRect otherwise
		return CalcRect(poseLoc, column, view);
	}

	BRect rect;
	rect.left = column->Offset() + poseLoc.x;
	rect.right = rect.left + column->Width();
	rect.bottom = poseLoc.y + roundf((view->ListElemHeight() + view->FontHeight()) / 2.f);
	rect.top = rect.bottom - view->FontHeight();

	return rect;
}


BRect
BTextWidget::CalcRectCommon(BPoint poseLoc, const BColumn* column,
	const BPoseView* view, float textWidth)
{
	BRect rect;
	float viewWidth;

	poseLoc.x = roundf(poseLoc.x);
	poseLoc.y = roundf(poseLoc.y);

	if (view->ViewMode() == kListMode) {
		viewWidth = roundf(std::min(column->Width(), textWidth));

		poseLoc.x += column->Offset();

		switch (fAlignment) {
			case B_ALIGN_LEFT:
				rect.left = poseLoc.x;
				rect.right = rect.left + viewWidth - 1;
				break;

			case B_ALIGN_CENTER:
				rect.left = poseLoc.x + roundf((column->Width() - viewWidth) / 2.f);
				if (rect.left < 0)
					rect.left = 0;

				rect.right = rect.left + viewWidth - 1;
				break;

			case B_ALIGN_RIGHT:
				rect.right = poseLoc.x + column->Width();
				rect.left = rect.right - viewWidth + 1;
				if (rect.left < 0)
					rect.left = 0;
				break;

			default:
				TRESPASS();
				break;
		}

		rect.bottom = poseLoc.y + roundf((view->ListElemHeight() + view->FontHeight()) / 2.f);
		rect.top = rect.bottom - view->FontHeight() + 1;
	} else {
		float iconSize = (float)view->IconSizeInt();
		if (view->ViewMode() == kIconMode) {
			// icon mode
			viewWidth = roundf(std::min(view->StringWidth("M") * 30, textWidth));

			rect.left = poseLoc.x + roundf((iconSize - viewWidth) / 2.f);
			rect.bottom = poseLoc.y + ceilf(view->IconPoseHeight());
		} else {
			// mini icon mode
			viewWidth = roundf(textWidth);

			rect.left = poseLoc.x + iconSize + kMiniIconSeparator;
			rect.bottom = poseLoc.y + roundf((iconSize + view->FontHeight()) / 2.f);
		}

		rect.top = rect.bottom - view->FontHeight() + 1;
		rect.right = rect.left + viewWidth - 1;
	}

	return rect;
}


BRect
BTextWidget::CalcRect(BPoint poseLoc, const BColumn* column, const BPoseView* view)
{
	return CalcRectCommon(poseLoc, column, view, fText->Width(view));
}


BRect
BTextWidget::CalcOldRect(BPoint poseLoc, const BColumn* column, const BPoseView* view)
{
	return CalcRectCommon(poseLoc, column, view, fText->CurrentWidth());
}


BRect
BTextWidget::CalcClickRect(BPoint poseLoc, const BColumn* column, const BPoseView* view)
{
	BRect rect = CalcRect(poseLoc, column, view);
	if (rect.Width() < kWidthMargin) {
		// if recting rect too narrow, make it a bit wider
		// for comfortable clicking
		if (column != NULL && column->Width() < kWidthMargin)
			rect.right = rect.left + column->Width();
		else
			rect.right = rect.left + kWidthMargin;
	}

	return rect;
}


void
BTextWidget::CheckExpiration()
{
	if (fLastClickedTime > 0 && IsEditable() && fParams.pose->IsSelected()) {
		bigtime_t doubleClickSpeed;
		get_click_speed(&doubleClickSpeed);

		bigtime_t delta = system_time() - fLastClickedTime;

		if (delta > doubleClickSpeed) {
			// at least 'doubleClickSpeed' microseconds elapsed with no click
			fLastClickedTime = 0;
			BColumn* column = fParams.poseView->ColumnFor(fAttrHash);
			StartEdit(fParams.poseView, fParams.pose, column);
		}
	} else {
		CancelWait();
	}
}


void
BTextWidget::CancelWait()
{
	fLastClickedTime = 0;
	fParams.poseView->SetTextWidgetToCheck(NULL);
}


void
BTextWidget::DoMouseUp(BPoseView* view, BPose* pose)
{
	// Register the time of that click.  The PoseView, through its Pulse()
	// will allow us to StartEdit() if no other click have been registered since
	// then.

	// TODO: re-enable modifiers, one should be enough
	view->SetTextWidgetToCheck(NULL);
	if (IsEditable() && pose->IsSelected()) {
		bigtime_t doubleClickSpeed;
		get_click_speed(&doubleClickSpeed);

		if (fLastClickedTime == 0) {
			fLastClickedTime = system_time();
			if (fLastClickedTime - doubleClickSpeed < pose->SelectionTime())
				fLastClickedTime = 0;
		} else
			fLastClickedTime = 0;

		if (fLastClickedTime == 0)
			return;

		view->SetTextWidgetToCheck(this);

		fParams.pose = pose;
		fParams.poseView = view;
	} else
		fLastClickedTime = 0;
}


static filter_result
TextViewKeyDownFilter(BMessage* message, BHandler**, BMessageFilter* filter)
{
	uchar key;
	if (message->FindInt8("byte", (int8*)&key) != B_OK)
		return B_DISPATCH_MESSAGE;

	ThrowOnAssert(filter != NULL);

	BContainerWindow* window = dynamic_cast<BContainerWindow*>(
		filter->Looper());
	ThrowOnAssert(window != NULL);

	BPoseView* view = window->PoseView();
	ThrowOnAssert(view != NULL);

	if (key == B_RETURN || key == B_ESCAPE) {
		view->CommitActivePose(key == B_RETURN);
		return B_SKIP_MESSAGE;
	}

	if (key == B_TAB) {
		if (view->ActivePose()) {
			if (message->FindInt32("modifiers") & B_SHIFT_KEY)
				view->ActivePose()->EditPreviousWidget(view);
			else
				view->ActivePose()->EditNextWidget(view);
		}

		return B_SKIP_MESSAGE;
	}

	// the BTextView doesn't respect window borders when resizing itself;
	// we try to work-around this "bug" here.

	// find the text editing view
	BView* scrollView = view->FindView("BorderView");
	if (scrollView != NULL) {
		BTextView* textView = dynamic_cast<BTextView*>(
			scrollView->FindView("WidgetTextView"));
		if (textView != NULL) {
			ASSERT(view->ActiveTextWidget() != NULL);
			float maxWidth = view->ActiveTextWidget()->MaxWidth();
			bool tooWide = textView->TextRect().Width() > maxWidth;
			textView->MakeResizable(!tooWide, tooWide ? NULL : scrollView);
		}
	}

	return B_DISPATCH_MESSAGE;
}


static filter_result
TextViewPasteFilter(BMessage* message, BHandler**, BMessageFilter* filter)
{
	ThrowOnAssert(filter != NULL);

	BContainerWindow* window = dynamic_cast<BContainerWindow*>(
		filter->Looper());
	ThrowOnAssert(window != NULL);

	BPoseView* view = window->PoseView();
	ThrowOnAssert(view != NULL);

	// the BTextView doesn't respect window borders when resizing itself;
	// we try to work-around this "bug" here.

	// find the text editing view
	BView* scrollView = view->FindView("BorderView");
	if (scrollView != NULL) {
		BTextView* textView = dynamic_cast<BTextView*>(
			scrollView->FindView("WidgetTextView"));
		if (textView != NULL) {
			float textWidth = textView->TextRect().Width();

			// subtract out selected text region width
			int32 start, finish;
			textView->GetSelection(&start, &finish);
			if (start != finish) {
				BRegion selectedRegion;
				textView->GetTextRegion(start, finish, &selectedRegion);
				textWidth -= selectedRegion.Frame().Width();
			}

			// add pasted text width
			if (be_clipboard->Lock()) {
				BMessage* clip = be_clipboard->Data();
				if (clip != NULL) {
					const char* text = NULL;
					ssize_t length = 0;

					if (clip->FindData("text/plain", B_MIME_TYPE,
							(const void**)&text, &length) == B_OK) {
						textWidth += textView->StringWidth(text);
					}
				}

				be_clipboard->Unlock();
			}

			// check if pasted text is too wide
			ASSERT(view->ActiveTextWidget() != NULL);
			float maxWidth = view->ActiveTextWidget()->MaxWidth();
			bool tooWide = textWidth > maxWidth;

			if (tooWide) {
				// resize text view to max width

				// move scroll view if not left aligned
				float oldWidth = textView->Bounds().Width();
				float newWidth = maxWidth;
				float right = oldWidth - newWidth;

				if (textView->Alignment() == B_ALIGN_CENTER)
					scrollView->MoveBy(roundf(right / 2), 0);
				else if (textView->Alignment() == B_ALIGN_RIGHT)
					scrollView->MoveBy(right, 0);

				// resize scroll view
				float grow = newWidth - oldWidth;
				scrollView->ResizeBy(grow, 0);
			}

			textView->MakeResizable(!tooWide, tooWide ? NULL : scrollView);
		}
	}

	return B_DISPATCH_MESSAGE;
}


void
BTextWidget::StartEdit(BPoseView* view, BPose* pose, BColumn* column)
{
	ASSERT(view != NULL);
	ASSERT(view->Window() != NULL);
	ASSERT(pose != NULL);

	view->SetTextWidgetToCheck(NULL, this);
	if (!IsEditable() || IsActive())
		return;

	view->SetActiveTextWidget(this);

	// The initial text color has to be set differently on Desktop
	rgb_color initialTextColor;
	if (view->IsDesktopView())
		initialTextColor = InvertColor(view->HighColor());
	else
		initialTextColor = view->HighColor();

	// The pose may have been dragged to a new location.
	BPoint poseLoc;
	if (view->ViewMode() == kListMode)
		poseLoc = BPoint(0, view->IndexOfPose(pose) * view->ListElemHeight());
	else
		poseLoc = pose->Location(view);

	BRect rect(CalcRect(poseLoc, column, view));
	rect.OffsetTo(roundf(rect.left), roundf(rect.top));

	// the colours of the selection pill the box sits in, and of the text on it
	const rgb_color backdrop = view->IsDesktopView() ? InvertColor(view->HighColor()) : view->LowColor();
	rgb_color pillColor, pillTextColor;
	SnakeSelector::SelectionColors(backdrop, true, &pillColor, &pillTextColor);
	initialTextColor = pillTextColor;
		// (the text is drawn in the colour it was created with, not the view's high colour)

	BTextView* textView = new PillTextView(rect.InsetByCopy(-2, -1), "WidgetTextView",
		rect.OffsetToCopy(2, 1), be_plain_font, &initialTextColor);

	textView->SetWordWrap(false);
	textView->SetInsets(2, 1, 2, 1);
	DisallowMetaKeys(textView);
	fText->SetupEditing(textView);

	textView->SetViewColor(pillColor);
	textView->SetLowColor(pillColor);
	textView->SetHighColor(pillTextColor);

	if (view->SelectedVolumeIsReadOnly()) {
		textView->MakeEditable(false);
		textView->MakeSelectable(true);
	}

	textView->AddFilter(new BMessageFilter(B_KEY_DOWN, TextViewKeyDownFilter));
	if (!view->SelectedVolumeIsReadOnly())
		textView->AddFilter(new BMessageFilter(B_PASTE, TextViewPasteFilter));

	// truncated text bounds
	BRect bounds(rect);

	// get full text length
	rect.right = rect.left + textView->LineWidth() - 1;
	rect.bottom = rect.top + textView->LineHeight() - 1;

	if (view->ViewMode() == kListMode) {
		// limit max width to column width in list mode
		BColumn* column = view->ColumnFor(fAttrHash);
		ASSERT(column != NULL);
		fMaxWidth = column->Width();
	} else {
		// limit max width to 30em in icon and mini icon mode
		fMaxWidth = textView->StringWidth("M") * 30;

		// center under the icon if text is longer than truncated label width
		if (view->ViewMode() == kIconMode && rect.Width() > bounds.Width()) {
			float newWidth = std::min(fMaxWidth, rect.Width());
			rect.OffsetBy(roundf((bounds.Width() - newWidth) / 2), 0);
			textView->MoveTo(rect.left - 2, rect.top - 1);
		}
	}

	// resize textView
	textView->ResizeTo(std::min(fMaxWidth, rect.Width()) + 4, rect.Height() + 2);
	textView->SetTextRect(rect.OffsetToCopy(2, 1));

	// set alignment before adding textView so it doesn't redraw
	switch (view->ViewMode()) {
		case kIconMode:
			textView->SetAlignment(B_ALIGN_CENTER);
			break;

		case kMiniIconMode:
			textView->SetAlignment(B_ALIGN_LEFT);
			break;

		case kListMode:
			textView->SetAlignment(fAlignment);
			break;
	}

	BScrollView* scrollView = new EditBorderView(textView);
	view->AddChild(scrollView);

	bool tooWide = textView->TextRect().Width() > fMaxWidth;
	textView->MakeResizable(!tooWide, tooWide ? NULL : scrollView);

	view->SetActivePose(pose);
		// tell view about pose
	SetActive(true);
		// for widget

	textView->SelectAll();
	textView->ScrollToSelection();
		// scroll to beginning so that text is visible
	textView->MakeFocus();

	// the widget draws the pill under the edit box while it is edited (see Draw())
	view->Invalidate(rect.InsetByCopy(-(kPillPadding + 4), -3));

	// force immediate redraw so TextView appears instantly
	view->Window()->UpdateIfNeeded();
}


void
BTextWidget::StopEdit(bool saveChanges, BPoint poseLoc, BPoseView* view,
	BPose* pose, int32 poseIndex)
{
	view->SetActiveTextWidget(NULL);

	// find the text editing view
	BView* scrollView = view->FindView("BorderView");
	ASSERT(scrollView != NULL);
	if (scrollView == NULL)
		return;

	BTextView* textView = dynamic_cast<BTextView*>(scrollView->FindView("WidgetTextView"));
	ASSERT(textView != NULL);
	if (textView == NULL)
		return;

	BColumn* column = view->ColumnFor(fAttrHash);
	ASSERT(column != NULL);
	if (column == NULL)
		return;

	if (saveChanges && fText->CommitEditedText(textView)) {
		// we have an actual change, re-sort
		view->CheckPoseSortOrder(pose, poseIndex);
	}

	// make text widget visible again
	SetVisible(true);
	view->Invalidate(ColumnRect(poseLoc, column, view));
	view->Invalidate(scrollView->Frame().InsetByCopy(-(kPillPadding + 4), -3));

	// force immediate redraw so TEView disappears
	scrollView->RemoveSelf();
	delete scrollView;

	ASSERT(view->Window() != NULL);
	view->Window()->UpdateIfNeeded();
	view->MakeFocus();

	SetActive(false);
}


void
BTextWidget::CheckAndUpdate(BPoint loc, const BColumn* column, BPoseView* view, bool visible)
{
	BRect oldRect;
	if (view->ViewMode() != kListMode)
		oldRect = CalcOldRect(loc, column, view);

	if (fText->CheckAttributeChanged() && fText->CheckViewChanged(view) && visible) {
		BRect invalidRect(ColumnRect(loc, column, view));
		if (view->ViewMode() != kListMode)
			invalidRect = invalidRect | oldRect;

		view->Invalidate(invalidRect);
	}
}


void
BTextWidget::SelectAll(BPoseView* view)
{
	BTextView* text = dynamic_cast<BTextView*>(view->FindView("WidgetTextView"));
	if (text != NULL)
		text->SelectAll();
}


void
BTextWidget::Draw(BRect eraseRect, BRect textRect, BPoseView* view, BView* drawView,
	bool selected, uint32 clipboardMode, BPoint offset)
{
	ASSERT(view != NULL);
	ASSERT(view->Window() != NULL);
	ASSERT(drawView != NULL);

	textRect.OffsetBy(offset);

	if (fActive) {
		// being edited: only the pill under the edit box is drawn, the box has the text
		BView* box = view->FindView("BorderView");
		if (box != NULL) {
			BRect pillRect = box->Frame().InsetByCopy(-kPillPadding, 0);
			if (view->IsDesktopView()) {
				const rgb_color backdrop = InvertColor(view->HighColor());
				SnakeSelector::DrawSelectionPill(drawView, pillRect, true, false, &backdrop);
			} else
				SnakeSelector::DrawSelectionPill(drawView, pillRect, true);
		}
		return;
	}

	// a selected label is a pill, which reaches a little past the text (the pose's rect leaves room for it)
	const bool pill = selected
		&& (view->Window()->IsActive() || view->IsDrawingSelectionRect() || view->ShowSelectionWhenInactive());
	// (the label's text colour is not to carry over to the other columns of its row)
	const rgb_color savedHigh = drawView->HighColor();
	BRect pillRect(textRect);
	if (pill)
		pillRect.InsetBy(-kPillPadding, -1);

	BRegion textRegion(pill ? pillRect : textRect);
	drawView->ConstrainClippingRegion(&textRegion);

	// We are only concerned with setting the correct text color.

	// For active views the selection is drawn as inverse text
	// (background color for the text, solid black for the background).
	// For inactive windows the text is drawn normally, then the
	// selection rect is alpha-blended on top. This all happens in
	// BPose::Draw before and after calling this function.

	bool direct = drawView == view;
	bool dragging = false;
	if (!direct && view->Window() != NULL && view->Window()->CurrentMessage() != NULL)
		dragging = view->Window()->CurrentMessage()->what == kMsgMouseDragged;
	bool drawOutlines = view->WidgetTextOutline() && !selected && (direct || dragging);
	bool drawCut = clipboardMode == kMoveSelectionTo;

	if (pill) {
		const bool active = view->Window()->IsActive() || view->IsDrawingSelectionRect();
		if (view->IsDesktopView()) {
			// the Desktop's labels are drawn straight onto the wallpaper, in the text colour; the wallpaper is
			// taken to be as dark as the text is light
			const rgb_color backdrop = InvertColor(view->HighColor());
			drawView->SetHighColor(SnakeSelector::DrawSelectionPill(drawView, pillRect, active, false, &backdrop));
		} else
			drawView->SetHighColor(SnakeSelector::DrawSelectionPill(drawView, pillRect, active));
	} else if (selected) {
		if (dragging) {
			drawView->SetDrawingMode(B_OP_ALPHA);
			drawView->SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_COMPOSITE);
		} else {
			// erase selection rect background
			drawView->SetDrawingMode(B_OP_COPY);
		}

		drawView->FillRect(textRect, B_SOLID_LOW);

		// High color is set to inverted low, then the whole thing is
		// inverted again so that the background color "shines through".
		drawView->SetHighColor(InvertColorSmart(drawView->LowColor()));
	} else {
		if (view->IsDesktopView())
			drawView->SetHighColor(view->HighColor());
		else
			drawView->SetHighUIColor(view->HighUIColor());
	}

	if (drawOutlines || dragging || (direct && drawCut)) {
		drawView->SetDrawingMode(B_OP_ALPHA);
		drawView->SetBlendingMode(B_CONSTANT_ALPHA, B_ALPHA_COMPOSITE);
	} else {
		drawView->SetDrawingMode(B_OP_OVER);
	}

	// high color and drawing mode are set to draw text

	BPoint location(textRect.left, roundf(textRect.top) + view->FontInfo().ascent + 1);
		// let's determine the baseline position

	const char* fittingText = fText->FittingText(view);

	// Draw text outline if enabled unless selected or column resizing.
	if (drawOutlines) {
		// draw a halo around the text by using the "false bold"
		// feature for text rendering. Either black or white is used for
		// the glow (whatever acts as contrast) with a some alpha value,
		BFont font;
		drawView->GetFont(&font);

		rgb_color textColor = drawView->HighColor();
		if (textColor.IsDark()) {
			// dark text on light outline
			rgb_color glowColor = ui_color(B_SHINE_COLOR);

			font.SetFalseBoldWidth(2.0);
			drawView->SetFont(&font, B_FONT_FALSE_BOLD_WIDTH);
			glowColor.alpha = 30;
			drawView->SetHighColor(glowColor);

			drawView->DrawString(fittingText, location);

			font.SetFalseBoldWidth(1.0);
			drawView->SetFont(&font, B_FONT_FALSE_BOLD_WIDTH);
			glowColor.alpha = 65;
			drawView->SetHighColor(glowColor);

			drawView->DrawString(fittingText, location);

			font.SetFalseBoldWidth(0.0);
			drawView->SetFont(&font, B_FONT_FALSE_BOLD_WIDTH);
		} else {
			// light text on dark outline
			rgb_color outlineColor = kBlack;

			font.SetFalseBoldWidth(1.0);
			drawView->SetFont(&font, B_FONT_FALSE_BOLD_WIDTH);
			outlineColor.alpha = 30;
			drawView->SetHighColor(outlineColor);

			drawView->DrawString(fittingText, location);

			font.SetFalseBoldWidth(0.0);
			drawView->SetFont(&font, B_FONT_FALSE_BOLD_WIDTH);

			outlineColor.alpha = 200;
			drawView->SetHighColor(outlineColor);

			drawView->DrawString(fittingText, location + BPoint(1, 1));
		}

		drawView->SetHighColor(textColor);
	}

	drawView->DrawString(fittingText, location);

	if (fSymLink && (fAttrHash == view->FirstColumn()->AttrHash())) {
		// TODO:
		// this should be exported to the WidgetAttribute class, probably
		// by having a per widget kind style
		BRect lineRect(textRect);
		lineRect.OffsetTo(location.x, std::min(location.y + 2, textRect.bottom));
			// underline goes 2px under the baseline
		lineRect.InsetBy(roundf(textRect.Width() - fText->Width(view)), 0);
			// only underline text part
		drawView->StrokeLine(lineRect.LeftTop(), lineRect.RightTop(), B_MIXED_COLORS);
	}

	drawView->ConstrainClippingRegion(NULL);
	if (pill)
		drawView->SetHighColor(savedHigh);
}
