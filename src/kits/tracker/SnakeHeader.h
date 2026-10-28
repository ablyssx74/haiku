/*
 * The Deskbar's header (the block at the top, with the SnakeTracker icon): what the Custom HVIF Icons tab of the SnakeTracker
 * Settings window sets and the Deskbar reads. The icon is a copy of the user's HVIF file in the Deskbar's settings folder; the
 * rest is in a file of its own, a flattened BMessage:
 *   "extra" int32   pixels the header is made taller than it is (0 to kHeaderMostExtra); the icon grows with it
 *   "align" int32   where the icon sits across the header: 0 left, 1 center, 2 right
 *   "valign" int32  where it sits up and down in it: 0 top, 1 center, 2 bottom
 *   "size" int32    how much of the header the icon fills, in percent (kHeaderLeastSize to 100; 100 fills it)
 * The same tab can also give Tracker an icon of its own (an HVIF file kept as snaketracker_tracker_icon.hvif in the settings
 * folder): the Deskbar shows it for Tracker's tab, and hDesktop (if it runs) for Tracker's icon in the dock.
 */
#ifndef SNAKE_HEADER_H
#define SNAKE_HEADER_H


#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>

#include <sys/stat.h>


static const char* const kHeaderSettingsFile = "snaketracker_header";
static const char* const kHeaderIconFile = "header_icon.hvif";
static const char* const kHeaderSourceAttribute = "deskbar:source";
static const int32 kHeaderMostExtra = 64;
static const int32 kHeaderLeastSize = 30;
enum { kHeaderAlignLeft = 0, kHeaderAlignCenter = 1, kHeaderAlignRight = 2 };
enum { kHeaderAlignTop = 0, kHeaderAlignMiddle = 1, kHeaderAlignBottom = 2 };
static const char* const kTrackerIconFile = "snaketracker_tracker_icon.hvif";

// To the Deskbar: the header's icon, height or alignment, or Tracker's icon, changed.
static const uint32 kMsgHeaderChanged = 'HiCh';
// To hDesktop (the dock): Tracker's icon changed.
static const uint32 kMsgTrackerIconChanged = 'SnkI';


inline bool
HeaderSettingsPath(BPath* path)
{
	return find_directory(B_USER_SETTINGS_DIRECTORY, path) == B_OK && path->Append(kHeaderSettingsFile) == B_OK;
}


// The place of the chosen icon (the folder is made when asked to).
inline bool
HeaderIconPath(BPath* path, bool create = false)
{
	if (find_directory(B_USER_SETTINGS_DIRECTORY, path, create) != B_OK || path->Append("deskbar") != B_OK)
		return false;
	if (create && create_directory(path->Path(), 0755) != B_OK)
		return false;
	return path->Append(kHeaderIconFile) == B_OK;
}


// The place of the icon the user chose for Tracker (the settings folder itself).
inline bool
TrackerIconPath(BPath* path)
{
	return find_directory(B_USER_SETTINGS_DIRECTORY, path) == B_OK && path->Append(kTrackerIconFile) == B_OK;
}


inline void
ReadHeaderSettings(int32* extra, int32* align, int32* valign = NULL, int32* size = NULL)
{
	*extra = 0;
	*align = kHeaderAlignCenter;
	if (valign != NULL)
		*valign = kHeaderAlignMiddle;
	if (size != NULL)
		*size = 100;
	BPath path;
	if (!HeaderSettingsPath(&path))
		return;
	BFile file(path.Path(), B_READ_ONLY);
	BMessage saved;
	if (file.InitCheck() != B_OK || saved.Unflatten(&file) != B_OK)
		return;
	int32 number;
	if (saved.FindInt32("extra", &number) == B_OK && number >= 0 && number <= kHeaderMostExtra)
		*extra = number;
	if (saved.FindInt32("align", &number) == B_OK && number >= 0 && number <= 2)
		*align = number;
	if (valign != NULL && saved.FindInt32("valign", &number) == B_OK && number >= 0 && number <= 2)
		*valign = number;
	if (size != NULL && saved.FindInt32("size", &number) == B_OK && number >= kHeaderLeastSize && number <= 100)
		*size = number;
}


inline void
WriteHeaderSettings(int32 extra, int32 align, int32 valign = kHeaderAlignMiddle, int32 size = 100)
{
	BPath path;
	if (!HeaderSettingsPath(&path))
		return;
	BFile file(path.Path(), B_CREATE_FILE | B_ERASE_FILE | B_WRITE_ONLY);
	if (file.InitCheck() != B_OK)
		return;
	BMessage settings;
	settings.AddInt32("extra", extra);
	settings.AddInt32("align", align);
	settings.AddInt32("valign", valign);
	settings.AddInt32("size", size);
	settings.Flatten(&file);
}


// Changes whenever the settings file does (0 when there is none): the Deskbar looks for that now and then.
inline int64
HeaderSettingsStamp()
{
	BPath path;
	struct stat st;
	if (!HeaderSettingsPath(&path) || stat(path.Path(), &st) != 0)
		return 0;
	return (int64)st.st_mtime * 1000003 + (int64)st.st_size + 1;
}


#endif	// SNAKE_HEADER_H
