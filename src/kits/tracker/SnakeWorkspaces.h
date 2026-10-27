/*
 * SnakeTracker workspaces: the transition when the Desktop changes to another workspace's wallpaper (the window "SnakeTracker
 * Workspaces" of the Desktop's menu). The effects are those of the wallpaper's fade (see SnakeWallpaper.h), and Tracker does it
 * with the same code. The settings are a flattened BMessage in their own file:
 *   "ms" int32      how long the transition takes, in milliseconds (0: none)
 *   "effect" int32  0 fade, 1 wipe (left to right), 2 slide (the new picture pushes the old one out to the left),
 *                   3 circle reveal (from the middle of the screen)
 */
#ifndef SNAKE_WORKSPACES_H
#define SNAKE_WORKSPACES_H


#include <File.h>
#include <FindDirectory.h>
#include <Message.h>
#include <Path.h>


static const char* const kWorkspaceSettingsFile = "snaketracker_workspaces";


inline bool
WorkspaceSettingsPath(BPath* path)
{
	return find_directory(B_USER_SETTINGS_DIRECTORY, path) == B_OK && path->Append(kWorkspaceSettingsFile) == B_OK;
}


inline void
ReadWorkspaceSettings(int32* ms, int32* effect)
{
	*ms = 0;
	*effect = 0;
	BPath path;
	if (!WorkspaceSettingsPath(&path))
		return;
	BFile file(path.Path(), B_READ_ONLY);
	BMessage saved;
	if (file.InitCheck() != B_OK || saved.Unflatten(&file) != B_OK)
		return;
	int32 number;
	if (saved.FindInt32("ms", &number) == B_OK && number >= 0)
		*ms = number;
	if (saved.FindInt32("effect", &number) == B_OK && number >= 0 && number <= 3)
		*effect = number;
}


inline void
WriteWorkspaceSettings(int32 ms, int32 effect)
{
	BPath path;
	if (!WorkspaceSettingsPath(&path))
		return;
	BFile file(path.Path(), B_CREATE_FILE | B_ERASE_FILE | B_WRITE_ONLY);
	if (file.InitCheck() != B_OK)
		return;
	BMessage settings;
	settings.AddInt32("ms", ms);
	settings.AddInt32("effect", effect);
	settings.Flatten(&file);
}


#endif	// SNAKE_WORKSPACES_H
