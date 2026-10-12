/*
 * A tool tip in the pill look of the Deskbar's panel: the pill's colour behind dark text. Haiku's own tool tip is drawn by the
 * library, in the tool tip colours, and cannot be told otherwise; a tool tip can be given a view of its own.
 */
#ifndef SNAKE_TOOL_TIP_H
#define SNAKE_TOOL_TIP_H


#include <String.h>
#include <ToolTip.h>
#include <View.h>

#include "SnakeSelector.h"


class SnakeTipView : public BView {
public:
	SnakeTipView(const char* text)
		:
		BView("snake tool tip", B_WILL_DRAW),
		fText(text)
	{
		SetFont(be_plain_font);
		SetViewColor(B_TRANSPARENT_COLOR);
		BSize size(ceilf(be_plain_font->StringWidth(text)) + 12, 0);
		font_height height;
		be_plain_font->GetHeight(&height);
		size.height = ceilf(height.ascent + height.descent) + 6;
		SetExplicitMinSize(size);
		SetExplicitPreferredSize(size);
		SetExplicitMaxSize(size);
	}

	virtual void AttachedToWindow()
	{
		// the tool tip window puts its own colour round a view: it takes the pill's
		rgb_color fill, text;
		if (SnakeSelector::PillColors(&fill, NULL, NULL, &text, NULL) && Parent() != NULL) {
			Parent()->SetViewColor(fill);
			Parent()->SetLowColor(fill);
		}
	}

	virtual void Draw(BRect)
	{
		rgb_color fill = ui_color(B_TOOL_TIP_BACKGROUND_COLOR), text = ui_color(B_TOOL_TIP_TEXT_COLOR);
		SnakeSelector::PillColors(&fill, NULL, NULL, &text, NULL);
		BRect bounds(Bounds());
		SetHighColor(fill);
		FillRect(bounds);
		SetHighColor(text);
		font_height height;
		GetFontHeight(&height);
		DrawString(fText.String(), BPoint(6, 3 + ceilf(height.ascent)));
	}

private:
	BString	fText;
};


class SnakeToolTip : public BToolTip {
public:
	SnakeToolTip(const char* text)
		:
		BToolTip(),
		fView(new SnakeTipView(text))
	{
	}

	virtual ~SnakeToolTip() { delete fView; }

	virtual BView* View() const { return fView; }

private:
	BView*	fView;
};


#endif	// SNAKE_TOOL_TIP_H
