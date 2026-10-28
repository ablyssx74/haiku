#include "HeaderIcon.h"

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
#include "SnakeHeader.h"
#include "icons.h"


static const char* kIconFileName = "header_icon.hvif";
static const off_t kMostIconBytes = 256 * 1024;


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


// What the settings file says, looked at again when it has changed.
static void
ReadSettings(int32* extra, int32* align, int32* valign = NULL, int32* size = NULL)
{
	static int64 stamp = -1;
	static int32 cachedExtra = 0, cachedAlign = kHeaderAlignCenter, cachedVAlign = kHeaderAlignMiddle, cachedSize = 100;
	const int64 now = HeaderSettingsStamp();
	if (now != stamp) {
		stamp = now;
		ReadHeaderSettings(&cachedExtra, &cachedAlign, &cachedVAlign, &cachedSize);
	}
	*extra = cachedExtra;
	*align = cachedAlign;
	if (valign != NULL)
		*valign = cachedVAlign;
	if (size != NULL)
		*size = cachedSize;
}


int32
HeaderExtraHeight()
{
	int32 extra, align;
	ReadSettings(&extra, &align);
	return extra;
}


int32
HeaderAlignment()
{
	int32 extra, align;
	ReadSettings(&extra, &align);
	return align;
}


int32
HeaderVerticalAlignment()
{
	int32 extra, align, valign;
	ReadSettings(&extra, &align, &valign);
	return valign;
}


bool
CustomTrackerIcon(BBitmap* icon)
{
	BPath path;
	if (icon == NULL || !TrackerIconPath(&path))
		return false;
	size_t length;
	uint8* data = ReadIcon(path.Path(), &length);
	if (data == NULL)
		return false;
	const status_t status = BIconUtils::GetVectorIcon(data, length, icon);
	free(data);
	return status == B_OK;
}


int32
HeaderIconPercent()
{
	int32 extra, align, valign, size;
	ReadSettings(&extra, &align, &valign, &size);
	return size;
}
