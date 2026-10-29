/*
 * A page of a window that scrolls when the window is too short for it (a small screen): the page is never made smaller than
 * what it needs, and the scroll bar covers the rest. No scroll bar is shown while the whole page fits.
 */
#ifndef SNAKE_SCROLL_PANE_H
#define SNAKE_SCROLL_PANE_H


#include <ScrollBar.h>
#include <ScrollView.h>
#include <Window.h>
#include <View.h>

#include <algorithm>


namespace BPrivate {

// Holds the page and scrolls it when the window is too short for it (a small screen): the page is never made smaller than
// what it needs, and the scroll bar covers the rest.
class SnakeScrollPane : public BView {
public:
	SnakeScrollPane(BView* content)
		:
		BView("page", B_FRAME_EVENTS),
		fContent(content),
		fRelayoutPending(false)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		AddChild(content);
	}

	virtual BSize MinSize()
	{
		// (a short window is all right: the page scrolls)
		return BSize(fContent->MinSize().width, 120);
	}

	virtual BSize PreferredSize() { return fContent->PreferredSize(); }
	virtual BSize MaxSize() { return BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED); }

	virtual void AttachedToWindow()
	{
		BView::AttachedToWindow();
		_Layout();
	}

	virtual void FrameResized(float, float)
	{
		_Layout();
	}

	virtual void DoLayout()
	{
		BView::DoLayout();
		_Layout();
	}

	// the page changed what it needs (a control was shown or hidden): its size and the scroll bar are set again, once the
	// page has worked out its new minimum
	virtual void InvalidateLayout(bool descendants = false)
	{
		BView::InvalidateLayout(descendants);
		if (Window() != NULL && !fRelayoutPending) {
			fRelayoutPending = true;
			Window()->PostMessage(kMsgRelayout, this);
		}
	}

	virtual void MessageReceived(BMessage* message)
	{
		if (message->what == kMsgRelayout) {
			fRelayoutPending = false;
			_Layout();
			return;
		}
		BView::MessageReceived(message);
	}

private:
	void _Layout()
	{
		const float viewWidth = Bounds().Width() + 1;
		const float viewHeight = Bounds().Height() + 1;
		BSize min = fContent->MinSize();
		float width = std::max(min.width, viewWidth);
		float height = std::max(min.height, viewHeight);
		fContent->MoveTo(0, 0);
		fContent->ResizeTo(width - 1, height - 1);

		BScrollBar* bar = ScrollBar(B_VERTICAL);
		if (bar != NULL) {
			float range = std::max(0.0f, height - viewHeight);
			bar->SetRange(0, range);
			bar->SetProportion(range > 0 ? viewHeight / height : 1.0f);
			bar->SetSteps(24, std::max(24.0f, viewHeight - 24));
			// (no scroll bar when the whole page is shown)
			if (range <= 0 && !bar->IsHidden(this))
				bar->Hide();
			else if (range > 0 && bar->IsHidden(this))
				bar->Show();
		}
	}

	static const uint32 kMsgRelayout = 'ScRl';

	BView*	fContent;
	bool	fRelayoutPending;
};



// the page, in a view that scrolls it
static inline BView*
MakeScrollable(BView* page)
{
	return new BScrollView("scrollable page", new SnakeScrollPane(page), 0, false, true, B_NO_BORDER);
}

}	// namespace BPrivate


#endif	// SNAKE_SCROLL_PANE_H
