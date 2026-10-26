/*
 * SnakeTracker wallpaper: what the settings window (in Tracker) and the SnakeTracker server (which changes the wallpaper
 * on a timer) share. The wallpaper itself is Haiku's own: the server writes the same "be:bgndimginfo" attribute of the
 * Desktop folder that the Backgrounds preferences write, and asks Tracker to show it again.
 */
#ifndef SNAKE_WALLPAPER_H
#define SNAKE_WALLPAPER_H


#include <Directory.h>
#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>
#include <String.h>

#include <string.h>


static const char* const kSnakeServerSignature = "application/x-vnd.snaketracker-server";

// To the server: read the settings file again (and set the timer); change the wallpaper now.
static const uint32 kMsgWallpaperReload = 'SnWR';
static const uint32 kMsgWallpaperNext = 'SnWN';
// To the server: are you the one that knows these messages? (an older server, still running after an update, does not answer)
static const uint32 kMsgWallpaperPing = 'SnWP';
static const uint32 kMsgWallpaperPong = 'SnWO';

// The pictures that online sources have today (Bing, Wikimedia Commons, Wallhaven, Picsum, the Art Institute of Chicago, Flickr, Unsplash, Pexels, Pixabay) are kept in a folder for each, below the cache folder.
static const char* const kOnlineSources[] = { "bing", "wikimedia", "wallhaven", "picsum", "artic", "flickr", "unsplash", "pexels", "pixabay" };
static const int32 kOnlineSourceCount = 9;
// (the last four need a key of the user's own, in the settings as "flickrkey", "unsplashkey", "pexelskey" and "pixabaykey")

// The settings file is a flattened BMessage:
//   "enabled" bool       change the wallpaper by itself
//   "folder" string...   the folders (and everything below them) the images are taken from
//   "interval" int32     minutes between two changes
//   "random" bool        a random image each time (off: one after another, sorted by path)
//   "perworkspace" bool  a different image on each workspace (off: one for all)
//   "online" string...   the online sources whose picture of the day is fetched once a day and joins the images
//   "svg" bool           an SVG wallpaper in the accent color takes the place of the pictures
//   "svgfile" string     its template: "builtin:waves", "builtin:bubbles", "builtin:bands" or the path of an SVG file
//   "fadems" int32       a fade from one picture to the next, this many milliseconds (0: none; Tracker does it)
//   "mode" int32         placement: -1 as it is now, else Haiku's (0 manual, 1 centered, 2 scaled to fit, 3 tiled)
static const char* const kWallpaperSettingsFile = "snaketracker_wallpaper";


inline void
WallpaperDefaults(BMessage* settings)
{
	settings->MakeEmpty();
	settings->AddBool("enabled", false);
	settings->AddInt32("interval", 30);
	settings->AddBool("random", true);
	settings->AddBool("perworkspace", false);
	settings->AddInt32("mode", -1);
	settings->AddInt32("fadems", 0);
	settings->AddBool("svg", false);
}


inline bool
WallpaperSettingsPath(BPath* path)
{
	return find_directory(B_USER_SETTINGS_DIRECTORY, path) == B_OK && path->Append(kWallpaperSettingsFile) == B_OK;
}


// The defaults, with what the file says over them.
inline void
ReadWallpaperSettings(BMessage* settings)
{
	WallpaperDefaults(settings);
	BPath path;
	if (!WallpaperSettingsPath(&path))
		return;
	BFile file(path.Path(), B_READ_ONLY);
	BMessage saved;
	if (file.InitCheck() != B_OK || saved.Unflatten(&file) != B_OK)
		return;
	bool flag;
	int32 number;
	if (saved.FindBool("enabled", &flag) == B_OK)
		settings->ReplaceBool("enabled", flag);
	if (saved.FindInt32("interval", &number) == B_OK && number >= 1)
		settings->ReplaceInt32("interval", number);
	if (saved.FindBool("random", &flag) == B_OK)
		settings->ReplaceBool("random", flag);
	if (saved.FindBool("perworkspace", &flag) == B_OK)
		settings->ReplaceBool("perworkspace", flag);
	if (saved.FindInt32("mode", &number) == B_OK)
		settings->ReplaceInt32("mode", number);
	if (saved.FindInt32("fadems", &number) == B_OK && number >= 0)
		settings->ReplaceInt32("fadems", number);
	if (saved.FindBool("svg", &flag) == B_OK)
		settings->ReplaceBool("svg", flag);
	const char* svgFile;
	if (saved.FindString("svgfile", &svgFile) == B_OK)
		settings->AddString("svgfile", svgFile);
	const char* folder;
	for (int32 i = 0; saved.FindString("folder", i, &folder) == B_OK; i++)
		settings->AddString("folder", folder);
	for (int32 i = 0; saved.FindString("online", i, &folder) == B_OK; i++)
		settings->AddString("online", folder);
	static const char* const kKeys[] = { "flickrkey", "unsplashkey", "pexelskey", "pixabaykey" };
	for (int i = 0; i < 4; i++) {
		if (saved.FindString(kKeys[i], &folder) == B_OK)
			settings->AddString(kKeys[i], folder);
	}
}


inline bool
WallpaperCacheDir(const char* source, BPath* path)
{
	return find_directory(B_USER_CACHE_DIRECTORY, path, true) == B_OK && path->Append("snaketracker_wallpapers") == B_OK
		&& path->Append(source) == B_OK;
}


inline status_t
WriteWallpaperSettings(const BMessage& settings)
{
	BPath path;
	if (!WallpaperSettingsPath(&path))
		return B_ERROR;
	BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (file.InitCheck() != B_OK)
		return file.InitCheck();
	return settings.Flatten(&file);
}


#endif	// SNAKE_WALLPAPER_H
