/*
 * The "SnakeTracker Wallpaper" window of the Desktop's menu: the folders the wallpapers come from, how often and in what
 * order they change, and whether each workspace has its own. The settings go to a file the SnakeTracker server reads; the
 * server changes the wallpaper with Haiku's own settings (see SnakeWallpaper.h).
 */
#ifndef SNAKE_WALLPAPER_WINDOW_H
#define SNAKE_WALLPAPER_WINDOW_H


#include <String.h>
#include <Window.h>


class BButton;
class BCheckBox;
class BFilePanel;
class BListView;
class BStringView;
class BTextControl;
class BMenuField;
class BPopUpMenu;


namespace BPrivate {

class SnakeWallpaperWindow : public BWindow {
public:
	// opens the window, or brings the open one to the front
	static void				Open();

	virtual bool			QuitRequested();
	virtual void			MessageReceived(BMessage* message);
	virtual void			WindowActivated(bool active);
	virtual void			WorkspaceActivated(int32 workspace, bool active);

private:
							SnakeWallpaperWindow();
	virtual					~SnakeWallpaperWindow();

			void			_Load();
			void			_Save(bool changeNow, bool notify = true);
			void			_AddFolder(const char* path);
			void			_UpdateControls();
			void			_ShowCurrent();
			void			_MarkSvg(const char* file);
			bool			_ServerReady();

			BCheckBox*		fEnable;
			BListView*		fFolders;
			BButton*		fAdd;
			BButton*		fRemove;
			BPopUpMenu*		fIntervalMenu;
			BPopUpMenu*		fOrderMenu;
			BPopUpMenu*		fWorkspaceMenu;
			BPopUpMenu*		fModeMenu;
			BPopUpMenu*		fFadeMenu;
			BPopUpMenu*		fEffectMenu;
			BCheckBox*		fOnline[9];
			BTextControl*	fKey[4];		// the keys of Flickr, Unsplash, Pexels and Pixabay
			BStringView*	fCurrent;
			BCheckBox*		fSvg;
			BPopUpMenu*		fSvgMenu;
			BString			fSvgFile;		// the template that was chosen with "Other file"
			BFilePanel*		fSvgPanel;
			BButton*		fNext;
			BFilePanel*		fPanel;
};

}	// namespace BPrivate


#endif	// SNAKE_WALLPAPER_WINDOW_H
