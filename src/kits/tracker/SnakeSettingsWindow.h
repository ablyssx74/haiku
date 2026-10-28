/*
 * The SnakeTracker Settings window of the Desktop's menu, with a tab for each part of SnakeTracker: the core settings (those of
 * the SnakeTracker pane that Tracker's preferences used to have), the wallpaper, the workspaces and the custom HVIF icons.
 */
#ifndef SNAKE_SETTINGS_WINDOW_H
#define SNAKE_SETTINGS_WINDOW_H


#include <Window.h>


class BTabView;


namespace BPrivate {

class SnakeSettingsView;
class SnakeWallpaperView;

class SnakeSettingsWindow : public BWindow {
public:
	// opens the window, or brings the open one to the front
	static void				Open();

	virtual bool			QuitRequested();
	virtual void			MessageReceived(BMessage* message);
	virtual void			WindowActivated(bool active);
	virtual void			WorkspaceActivated(int32 workspace, bool active);

private:
							SnakeSettingsWindow();

			BTabView*		fTabs;
			SnakeSettingsView* fCore;
			SnakeWallpaperView* fWallpaper;
};

}	// namespace BPrivate


#endif	// SNAKE_SETTINGS_WINDOW_H
