/*
 * The Custom HVIF Icons tab: see SnakeHeaderIconView.h.
 */


#include "SnakeHeaderIconView.h"

#include <algorithm>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <Alert.h>
#include <AppFileInfo.h>
#include <Bitmap.h>
#include <Button.h>
#include <Catalog.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <IconUtils.h>
#include <LayoutBuilder.h>
#include <Locale.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Messenger.h>
#include <Path.h>
#include <Node.h>
#include <PopUpMenu.h>
#include <Application.h>
#include <Roster.h>
#include <SeparatorView.h>
#include <Slider.h>
#include <String.h>
#include <StringView.h>
#include <View.h>

#include "SnakeHeader.h"
#include "SnakeSelector.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SnakeHeaderIconView"


namespace BPrivate {

static const uint32 kMsgChoose = 'hiCs';
static const uint32 kMsgDefault = 'hiDf';
static const uint32 kMsgHeight = 'hiHt';
static const uint32 kMsgAlign = 'hiAl';
static const uint32 kMsgSize = 'hiSz';
static const uint32 kMsgVAlign = 'hiVa';
static const uint32 kMsgTrackerChoose = 'hiTc';
static const uint32 kMsgTrackerDefault = 'hiTd';
static const uint32 kMsgTrackerPicked = 'hiTk';
static const off_t kMostIconBytes = 256 * 1024;
static const char* const kDeskbarSignature = "application/x-vnd.Be-TSKB";
static const char* const kDockSignature = "application/x-vnd.hdesktop";
// (the SnakeTracker snake: the icon of the server's file, which is also what the Deskbar draws when nothing was chosen)
static const char* const kDefaultIconFile = "/boot/system/servers/snaketracker_server";
static const float kPreviewWidth = 150;
static const float kPreviewBaseHeight = 28;
static const float kPreviewBaseIcon = 24;


// the bytes of an HVIF file (the caller frees them); NULL when it is not a vector icon
static uint8*
ReadIcon(const char* path, size_t* _size)
{
	BFile file(path, B_READ_ONLY);
	off_t size;
	if (file.InitCheck() != B_OK || file.GetSize(&size) != B_OK || size < 4 || size > kMostIconBytes)
		return NULL;
	uint8* data = (uint8*)malloc((size_t)size);
	if (data == NULL)
		return NULL;
	if (file.Read(data, (size_t)size) != size || memcmp(data, "ncif", 4) != 0) {
		free(data);
		return NULL;
	}
	*_size = (size_t)size;
	return data;
}


// the icon the Deskbar shows now, at size pixels: the user's file, else the snake (NULL if neither can be drawn)
static BBitmap*
CurrentIcon(int32 size)
{
	BBitmap* bitmap = new BBitmap(BRect(0, 0, size - 1, size - 1), B_RGBA32);
	if (bitmap->InitCheck() != B_OK) {
		delete bitmap;
		return NULL;
	}
	BPath path;
	if (HeaderIconPath(&path)) {
		size_t length;
		uint8* data = ReadIcon(path.Path(), &length);
		if (data != NULL) {
			const status_t status = BIconUtils::GetVectorIcon(data, length, bitmap);
			free(data);
			if (status == B_OK)
				return bitmap;
		}
	}
	BFile file(kDefaultIconFile, B_READ_ONLY);
	BAppFileInfo info(&file);
	if (info.InitCheck() == B_OK && info.GetIcon(bitmap, (icon_size)size) == B_OK)
		return bitmap;
	delete bitmap;
	return NULL;
}


// Tracker's own icon, at size pixels: the file the user chose, else the one of this Tracker's file (NULL if neither can be drawn)
static BBitmap*
TrackerIcon(int32 size)
{
	BBitmap* bitmap = new BBitmap(BRect(0, 0, size - 1, size - 1), B_RGBA32);
	if (bitmap->InitCheck() != B_OK) {
		delete bitmap;
		return NULL;
	}
	BPath path;
	if (TrackerIconPath(&path)) {
		size_t length;
		uint8* data = ReadIcon(path.Path(), &length);
		if (data != NULL) {
			const status_t status = BIconUtils::GetVectorIcon(data, length, bitmap);
			free(data);
			if (status == B_OK)
				return bitmap;
		}
	}
	app_info info;
	if (be_app != NULL && be_app->GetAppInfo(&info) == B_OK) {
		BFile file(&info.ref, B_READ_ONLY);
		BAppFileInfo appInfo(&file);
		if (appInfo.InitCheck() == B_OK && appInfo.GetIcon(bitmap, (icon_size)size) == B_OK)
			return bitmap;
	}
	delete bitmap;
	return NULL;
}


// the copy of a chosen HVIF file at target (made only when the file is a vector icon: an alert says so when it is not)
static bool
CopyIcon(const entry_ref& ref, const BPath& target)
{
	BEntry entry(&ref, true);
	BPath source;
	size_t length = 0;
	uint8* data = NULL;
	if (entry.GetPath(&source) == B_OK)
		data = ReadIcon(source.Path(), &length);
	BBitmap test(BRect(0, 0, 31, 31), B_RGBA32);
	if (data == NULL || BIconUtils::GetVectorIcon(data, length, &test) != B_OK) {
		free(data);
		BAlert* alert = new BAlert(B_TRANSLATE("Custom icon"), B_TRANSLATE("That file is not a vector (HVIF) icon."),
			B_TRANSLATE("OK"), NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
		alert->Go(NULL);
		return false;
	}
	BFile file(target.Path(), B_CREATE_FILE | B_ERASE_FILE | B_WRITE_ONLY);
	const bool written = file.InitCheck() == B_OK && file.Write(data, length) == (ssize_t)length;
	free(data);
	if (!written)
		return false;
	const BString name(ref.name);
	file.WriteAttr(kHeaderSourceAttribute, B_STRING_TYPE, 0, name.String(), name.Length() + 1);
	return true;
}


// only folders and files that look like they can be an icon are offered
class IconFileFilter : public BRefFilter {
public:
	virtual bool Filter(const entry_ref* ref, BNode* node, struct stat_beos*, const char*)
	{
		if (node->IsDirectory() || node->IsSymLink())
			return true;
		const size_t length = strlen(ref->name);
		return length > 5 && strcasecmp(ref->name + length - 5, ".hvif") == 0;
	}
};


// A picture of the header: its block in the SnakeTracker color, as tall as it is set to be, with the icon in it where it is set to
// sit.
class HeaderPreview : public BView {
public:
	HeaderPreview()
		:
		BView("preview", B_WILL_DRAW),
		fBitmap(NULL),
		fExtra(0),
		fAlign(kHeaderAlignCenter),
		fVAlign(kHeaderAlignMiddle)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		const BSize size(kPreviewWidth + 16, kPreviewBaseHeight + kHeaderMostExtra + 16);
		SetExplicitMinSize(size);
		SetExplicitMaxSize(size);
		SetExplicitPreferredSize(size);
	}

	~HeaderPreview() { delete fBitmap; }

	void Set(int32 extra, int32 align, int32 valign, int32 percent)
	{
		fExtra = extra;
		fAlign = align;
		fVAlign = valign;
		const float height = kPreviewBaseHeight + extra;
		// (the icon fills the header, or the part of it that was set)
		const int32 size = std::max((int32)8, (int32)((std::min(kPreviewWidth, height) - 2) * percent / 100));
		delete fBitmap;
		fBitmap = CurrentIcon(size);
		Invalidate();
	}

	virtual void Draw(BRect)
	{
		const float height = kPreviewBaseHeight + fExtra;
		BRect block(8, 8, 8 + kPreviewWidth - 1, 8 + height - 1);
		rgb_color fill = ui_color(B_CONTROL_HIGHLIGHT_COLOR), hot = fill;
		SnakeSelector::PillColors(&fill, &hot, NULL, NULL, NULL);
		SetHighColor(hot);
		FillRoundRect(block, 8, 8);
		SetHighColor(tint_color(hot, B_DARKEN_2_TINT));
		StrokeRoundRect(block, 8, 8);
		if (fBitmap == NULL)
			return;
		const float width = fBitmap->Bounds().Width() + 1;
		float x;
		if (fAlign == kHeaderAlignLeft)
			x = block.left + 6;
		else if (fAlign == kHeaderAlignRight)
			x = block.right - 6 - width + 1;
		else
			x = block.left + floorf((kPreviewWidth - width) / 2);
		float y;
		if (fVAlign == kHeaderAlignTop)
			y = block.top + std::min(floorf((height - width) / 2), 2.0f);
		else if (fVAlign == kHeaderAlignBottom)
			y = block.bottom - width + 1 - std::min(floorf((height - width) / 2), 2.0f);
		else
			y = block.top + floorf((height - width) / 2);
		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		DrawBitmap(fBitmap, BPoint(x, y));
	}

private:
	BBitmap*	fBitmap;
	int32		fExtra;
	int32		fAlign;
	int32		fVAlign;
};


// Tracker's icon, as it is now (the user's file, or the icon of Tracker's own file).
class TrackerIconPreview : public BView {
public:
	TrackerIconPreview()
		:
		BView("tracker icon", B_WILL_DRAW),
		fBitmap(NULL)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		const BSize size(56, 56);
		SetExplicitMinSize(size);
		SetExplicitMaxSize(size);
		SetExplicitPreferredSize(size);
	}

	~TrackerIconPreview() { delete fBitmap; }

	void Reload()
	{
		delete fBitmap;
		fBitmap = TrackerIcon(48);
		Invalidate();
	}

	virtual void AttachedToWindow() { Reload(); }

	virtual void Draw(BRect)
	{
		if (fBitmap == NULL)
			return;
		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		DrawBitmap(fBitmap, BPoint(4, 4));
	}

private:
	BBitmap*	fBitmap;
};


SnakeHeaderIconView::SnakeHeaderIconView()
	:
	BGroupView("custom icons", B_VERTICAL),
	fPanel(NULL),
	fTrackerPanel(NULL)
{
	fPreview = new HeaderPreview();
	fSource = new BStringView("source", "");
	fSource->SetAlignment(B_ALIGN_CENTER);

	BButton* choose = new BButton("choose", B_TRANSLATE("Choose file" B_UTF8_ELLIPSIS), new BMessage(kMsgChoose));
	BButton* reset = new BButton("default", B_TRANSLATE("Use default"), new BMessage(kMsgDefault));

	fHeightLabel = new BStringView("height label", "");
	fHeight = new BSlider("height", "", new BMessage(kMsgHeight), 0, kHeaderMostExtra, B_HORIZONTAL);
	fHeight->SetModificationMessage(new BMessage(kMsgHeight));
	fHeight->SetHashMarks(B_HASH_MARKS_BOTTOM);
	fHeight->SetHashMarkCount(kHeaderMostExtra / 8 + 1);
	fHeight->SetLimitLabels("0", "64");

	fAlignMenu = new BPopUpMenu("align");
	fAlignMenu->SetLabelFromMarked(true);
	static const char* const kAlignLabels[3] = { "Left", "Center", "Right" };
	for (int32 i = 0; i < 3; i++) {
		BMessage* message = new BMessage(kMsgAlign);
		message->AddInt32("value", i);
		fAlignMenu->AddItem(new BMenuItem(B_TRANSLATE_NOCOLLECT(kAlignLabels[i]), message));
	}
	BMenuField* align = new BMenuField(B_TRANSLATE("Icon horizontal position:"), fAlignMenu);

	fVAlignMenu = new BPopUpMenu("valign");
	fVAlignMenu->SetLabelFromMarked(true);
	static const char* const kVAlignLabels[3] = { "Top", "Center", "Bottom" };
	for (int32 i = 0; i < 3; i++) {
		BMessage* message = new BMessage(kMsgVAlign);
		message->AddInt32("value", i);
		fVAlignMenu->AddItem(new BMenuItem(B_TRANSLATE_NOCOLLECT(kVAlignLabels[i]), message));
	}
	BMenuField* valign = new BMenuField(B_TRANSLATE("Icon vertical position:"), fVAlignMenu);

	fSizeLabel = new BStringView("size label", "");
	fSize = new BSlider("size", "", new BMessage(kMsgSize), kHeaderLeastSize, 100, B_HORIZONTAL);
	fSize->SetModificationMessage(new BMessage(kMsgSize));
	fSize->SetLimitLabels("30%", "100%");

	int32 extra, alignment, vertical, percent;
	ReadHeaderSettings(&extra, &alignment, &vertical, &percent);
	fHeight->SetValue(extra);
	fSize->SetValue(percent);
	fAlignMenu->ItemAt(alignment)->SetMarked(true);
	fVAlignMenu->ItemAt(vertical)->SetMarked(true);

	// Tracker's icon
	fTrackerPreview = new TrackerIconPreview();
	fTrackerSource = new BStringView("tracker source", "");
	BButton* trackerChoose = new BButton("tracker choose", B_TRANSLATE("Choose file" B_UTF8_ELLIPSIS),
		new BMessage(kMsgTrackerChoose));
	BButton* trackerReset = new BButton("tracker default", B_TRANSLATE("Use default"), new BMessage(kMsgTrackerDefault));

	BStringView* headerTitle = new BStringView("header title", B_TRANSLATE("Deskbar header icon"));
	headerTitle->SetFont(be_bold_font);
	BStringView* trackerTitle = new BStringView("tracker title", B_TRANSLATE("Tracker icon"));
	trackerTitle->SetFont(be_bold_font);

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.Add(headerTitle)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fPreview)
			.AddGlue()
			.End()
		.Add(fSource)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(choose)
			.Add(reset)
			.AddGlue()
			.End()
		.Add(fHeightLabel)
		.Add(fHeight)
		.Add(fSizeLabel)
		.Add(fSize)
		.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
			.Add(align->CreateLabelLayoutItem(), 0, 0).Add(align->CreateMenuBarLayoutItem(), 1, 0)
			.Add(valign->CreateLabelLayoutItem(), 0, 1).Add(valign->CreateMenuBarLayoutItem(), 1, 1)
			.End()
		.Add(new BStringView("note", B_TRANSLATE("The icon grows with the header. Choose an HVIF (vector icon) file.")))
		.AddStrut(B_USE_BIG_SPACING)
		.Add(new BSeparatorView(B_HORIZONTAL))
		.Add(trackerTitle)
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.Add(fTrackerPreview)
			.AddGroup(B_VERTICAL, B_USE_SMALL_SPACING)
				.Add(fTrackerSource)
				.AddGroup(B_HORIZONTAL)
					.Add(trackerChoose)
					.Add(trackerReset)
					.AddGlue()
					.End()
				.End()
			.AddGlue()
			.End()
		.Add(new BStringView("tracker note", B_TRANSLATE("Used for Tracker in the Deskbar and, if it runs, in hDesktop's dock.")))
		.AddGlue()
		.SetInsets(B_USE_WINDOW_INSETS);

	_Update();
}


SnakeHeaderIconView::~SnakeHeaderIconView()
{
	delete fPanel;
	delete fTrackerPanel;
}


void
SnakeHeaderIconView::_Update()
{
	BString text(B_TRANSLATE("The SnakeTracker snake (the built-in icon)"));
	BPath path;
	struct stat st;
	if (HeaderIconPath(&path) && stat(path.Path(), &st) == 0) {
		BNode node(path.Path());
		char name[B_FILE_NAME_LENGTH];
		if (node.InitCheck() == B_OK && node.ReadAttr(kHeaderSourceAttribute, B_STRING_TYPE, 0, name, sizeof(name) - 1) > 0)
			text = name;
		else
			text = kHeaderIconFile;
	}
	fSource->SetText(text.String());

	BString label;
	label << B_TRANSLATE("Header height: taller by ") << fHeight->Value() << B_TRANSLATE(" pixels");
	fHeightLabel->SetText(label.String());

	BMenuItem* marked = fAlignMenu->FindMarked();
	int32 align = marked != NULL ? fAlignMenu->IndexOf(marked) : kHeaderAlignCenter;
	BMenuItem* verticalMarked = fVAlignMenu->FindMarked();
	int32 valign = verticalMarked != NULL ? fVAlignMenu->IndexOf(verticalMarked) : kHeaderAlignMiddle;
	BString sizeLabel;
	sizeLabel << B_TRANSLATE("Icon size: ") << fSize->Value() << B_TRANSLATE("% of the header");
	fSizeLabel->SetText(sizeLabel.String());
	fPreview->Set(fHeight->Value(), align, valign, fSize->Value());

	BString trackerText(B_TRANSLATE("The icon of Tracker (the built-in icon)"));
	BPath trackerPath;
	if (TrackerIconPath(&trackerPath) && stat(trackerPath.Path(), &st) == 0) {
		BNode node(trackerPath.Path());
		char name[B_FILE_NAME_LENGTH];
		if (node.InitCheck() == B_OK && node.ReadAttr(kHeaderSourceAttribute, B_STRING_TYPE, 0, name, sizeof(name) - 1) > 0)
			trackerText = name;
		else
			trackerText = kTrackerIconFile;
	}
	fTrackerSource->SetText(trackerText.String());
	fTrackerPreview->Reload();
}


void
SnakeHeaderIconView::_TellDeskbar()
{
	BMessenger deskbar(kDeskbarSignature);
	if (deskbar.IsValid())
		deskbar.SendMessage(kMsgHeaderChanged);
}


void
SnakeHeaderIconView::_TellDock()
{
	BMessenger dock(kDockSignature);
	if (dock.IsValid())
		dock.SendMessage(kMsgTrackerIconChanged);
}


void
SnakeHeaderIconView::_SaveSettings()
{
	BMenuItem* marked = fAlignMenu->FindMarked();
	BMenuItem* verticalMarked = fVAlignMenu->FindMarked();
	WriteHeaderSettings(fHeight->Value(), marked != NULL ? fAlignMenu->IndexOf(marked) : kHeaderAlignCenter,
		verticalMarked != NULL ? fVAlignMenu->IndexOf(verticalMarked) : kHeaderAlignMiddle, fSize->Value());
	_TellDeskbar();
}


void
SnakeHeaderIconView::_Choose(const entry_ref& ref)
{
	BPath target;
	if (!HeaderIconPath(&target, true) || !CopyIcon(ref, target))
		return;
	_Update();
	_TellDeskbar();
}


void
SnakeHeaderIconView::_ChooseTracker(const entry_ref& ref)
{
	BPath target;
	if (!TrackerIconPath(&target) || !CopyIcon(ref, target))
		return;
	_Update();
	_TellDeskbar();
	_TellDock();
}


void
SnakeHeaderIconView::_UseDefaultTracker()
{
	BPath path;
	if (TrackerIconPath(&path))
		BEntry(path.Path()).Remove();
	_Update();
	_TellDeskbar();
	_TellDock();
}


void
SnakeHeaderIconView::_UseDefault()
{
	BPath path;
	if (HeaderIconPath(&path))
		BEntry(path.Path()).Remove();
	_Update();
	_TellDeskbar();
}


void
SnakeHeaderIconView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgChoose:
			if (fPanel == NULL) {
				fPanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(this), NULL, B_FILE_NODE, false, NULL,
					new IconFileFilter(), true);
				fPanel->Window()->SetTitle(B_TRANSLATE("Choose a vector icon"));
			}
			fPanel->Show();
			break;

		case B_REFS_RECEIVED:
		{
			entry_ref ref;
			if (message->FindRef("refs", &ref) == B_OK)
				_Choose(ref);
			break;
		}

		case kMsgDefault:
			_UseDefault();
			break;

		case kMsgTrackerChoose:
			if (fTrackerPanel == NULL) {
				BMessage picked(kMsgTrackerPicked);
				fTrackerPanel = new BFilePanel(B_OPEN_PANEL, new BMessenger(this), NULL, B_FILE_NODE, false, &picked,
					new IconFileFilter(), true);
				fTrackerPanel->Window()->SetTitle(B_TRANSLATE("Choose a vector icon for Tracker"));
			}
			fTrackerPanel->Show();
			break;

		case kMsgTrackerPicked:
		{
			entry_ref ref;
			if (message->FindRef("refs", &ref) == B_OK)
				_ChooseTracker(ref);
			break;
		}

		case kMsgTrackerDefault:
			_UseDefaultTracker();
			break;

		case kMsgHeight:
		case kMsgSize:
		case kMsgAlign:
		case kMsgVAlign:
			_Update();
			_SaveSettings();
			break;

		default:
			BGroupView::MessageReceived(message);
			break;
	}
}

}	// namespace BPrivate
