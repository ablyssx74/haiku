#include "HeaderIconWindow.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <Button.h>
#include <Catalog.h>
#include <ControlLook.h>
#include <Entry.h>
#include <File.h>
#include <FilePanel.h>
#include <IconUtils.h>
#include <LayoutBuilder.h>
#include <Messenger.h>
#include <Locale.h>
#include <Node.h>
#include <Path.h>
#include <StringView.h>
#include <String.h>
#include <View.h>

#include "DeskbarUtils.h"
#include "ResourceSet.h"
#include "icons.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "HeaderIconWindow"


static const char* kIconFileName = "header_icon.hvif";
static const char* kSourceAttribute = "deskbar:source";
static const off_t kMostIconBytes = 256 * 1024;
static const uint32 kChoose = 'HiCs';
static const uint32 kDefault = 'HiDf';
static const uint32 kPreviewSize = 64;


static status_t
IconPath(BPath* path, bool create = false)
{
	status_t status = GetDeskbarSettingsDirectory(*path, create);
	if (status != B_OK)
		return status;
	return path->Append(kIconFileName);
}


// the bytes of a custom icon file (the caller frees them); NULL when there is none or it is not a vector icon
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


int64
HeaderIconStamp()
{
	BPath path;
	struct stat st;
	if (IconPath(&path) != B_OK || stat(path.Path(), &st) != 0)
		return 0;
	return (int64)st.st_mtime * 1000003 + (int64)st.st_size + 1;
}


BBitmap*
HeaderIconBitmap(int32 size)
{
	BBitmap* bitmap = new BBitmap(BRect(0, 0, size - 1, size - 1), B_RGBA32);
	if (bitmap->InitCheck() != B_OK) {
		delete bitmap;
		return NULL;
	}
	BPath path;
	if (IconPath(&path) == B_OK) {
		size_t length;
		uint8* data = ReadIcon(path.Path(), &length);
		if (data != NULL) {
			const status_t status = BIconUtils::GetVectorIcon(data, length, bitmap);
			free(data);
			if (status == B_OK)
				return bitmap;
		}
	}
	size_t dataSize;
	const void* data = AppResSet()->FindResource(B_VECTOR_ICON_TYPE, R_LeafLogoBitmap, &dataSize);
	if (data != NULL && BIconUtils::GetVectorIcon((const uint8*)data, dataSize, bitmap) == B_OK)
		return bitmap;
	delete bitmap;
	return NULL;
}


// #pragma mark -


class IconPreview : public BView {
public:
	IconPreview()
		:
		BView("preview", B_WILL_DRAW),
		fBitmap(NULL)
	{
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
		SetExplicitMinSize(BSize(kPreviewSize + 24, kPreviewSize + 24));
		SetExplicitMaxSize(BSize(kPreviewSize + 24, kPreviewSize + 24));
		SetExplicitPreferredSize(BSize(kPreviewSize + 24, kPreviewSize + 24));
	}

	~IconPreview() { delete fBitmap; }

	void Reload()
	{
		delete fBitmap;
		fBitmap = HeaderIconBitmap(kPreviewSize);
		Invalidate();
	}

	virtual void AttachedToWindow() { Reload(); }

	virtual void Draw(BRect)
	{
		BRect frame = Bounds();
		SetHighUIColor(B_PANEL_BACKGROUND_COLOR, 1.06f);
		FillRoundRect(frame, 10, 10);
		SetHighUIColor(B_PANEL_BACKGROUND_COLOR, 0.8f);
		StrokeRoundRect(frame.InsetByCopy(0.5f, 0.5f), 10, 10);
		if (fBitmap == NULL)
			return;
		SetDrawingMode(B_OP_ALPHA);
		SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
		DrawBitmap(fBitmap, BPoint(floorf((frame.Width() - kPreviewSize) / 2),
			floorf((frame.Height() - kPreviewSize) / 2)));
	}

private:
	BBitmap*	fBitmap;
};


// only folders and files that look like they can be an icon are offered
class IconFileFilter : public BRefFilter {
public:
	virtual bool Filter(const entry_ref* ref, BNode* node, struct stat_beos* st, const char* fileType)
	{
		if (node->IsDirectory() || node->IsSymLink())
			return true;
		const size_t length = strlen(ref->name);
		return length > 5 && strcasecmp(ref->name + length - 5, ".hvif") == 0;
	}
};


HeaderIconWindow* HeaderIconWindow::sWindow = NULL;


HeaderIconWindow::HeaderIconWindow()
	:
	BWindow(BRect(200, 200, 460, 400), B_TRANSLATE("Header icon"), B_TITLED_WINDOW_LOOK,
		B_NORMAL_WINDOW_FEEL,
		B_NOT_ZOOMABLE | B_NOT_RESIZABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_ASYNCHRONOUS_CONTROLS),
	fPanel(NULL)
{
	fPreview = new IconPreview();
	fSource = new BStringView("source", "");
	fSource->SetAlignment(B_ALIGN_CENTER);

	BButton* choose = new BButton("choose", B_TRANSLATE("Choose file" B_UTF8_ELLIPSIS),
		new BMessage(kChoose));
	BButton* reset = new BButton("default", B_TRANSLATE("Use default"), new BMessage(kDefault));
	BButton* close = new BButton("close", B_TRANSLATE("Close"), new BMessage(B_QUIT_REQUESTED));
	close->MakeDefault(true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(fPreview)
			.AddGlue()
			.End()
		.Add(fSource)
		.AddGroup(B_HORIZONTAL)
			.Add(choose)
			.Add(reset)
			.AddGlue()
			.Add(close)
			.End();

	_Update();
	CenterOnScreen();
}


HeaderIconWindow::~HeaderIconWindow()
{
	if (sWindow == this)
		sWindow = NULL;
	delete fPanel;
}


void
HeaderIconWindow::ShowWindow()
{
	if (sWindow != NULL && sWindow->Lock()) {
		if (sWindow->IsHidden())
			sWindow->Show();
		else
			sWindow->Activate();
		sWindow->Unlock();
		return;
	}
	sWindow = new HeaderIconWindow();
	sWindow->Show();
}


void
HeaderIconWindow::_Update()
{
	BPath path;
	BString text(B_TRANSLATE("The built-in icon"));
	if (HeaderIconStamp() != 0 && IconPath(&path) == B_OK) {
		BNode node(path.Path());
		char name[B_FILE_NAME_LENGTH];
		if (node.InitCheck() == B_OK && node.ReadAttr(kSourceAttribute, B_STRING_TYPE, 0, name, sizeof(name) - 1) > 0)
			text = name;
		else
			text = kIconFileName;
	}
	fSource->SetText(text.String());
	static_cast<IconPreview*>(fPreview)->Reload();
}


void
HeaderIconWindow::_Choose(const entry_ref& ref)
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
		BAlert* alert = new BAlert(B_TRANSLATE("Header icon"),
			B_TRANSLATE("That file is not a vector (HVIF) icon."), B_TRANSLATE("OK"), NULL, NULL,
			B_WIDTH_AS_USUAL, B_STOP_ALERT);
		alert->Go(NULL);
		return;
	}

	BPath target;
	if (IconPath(&target, true) != B_OK) {
		free(data);
		return;
	}
	BFile file(target.Path(), B_CREATE_FILE | B_ERASE_FILE | B_WRITE_ONLY);
	const bool written = file.InitCheck() == B_OK && file.Write(data, length) == (ssize_t)length;
	free(data);
	if (!written)
		return;
	const BString name(ref.name);
	file.WriteAttr(kSourceAttribute, B_STRING_TYPE, 0, name.String(), name.Length() + 1);
	_Update();
	be_app->PostMessage(kHeaderIconChanged);
}


void
HeaderIconWindow::_UseDefault()
{
	BPath path;
	if (IconPath(&path) == B_OK)
		BEntry(path.Path()).Remove();
	_Update();
	be_app->PostMessage(kHeaderIconChanged);
}


void
HeaderIconWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kChoose:
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

		case kDefault:
			_UseDefault();
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}
