/*
 * The Wallpaper tab of the SnakeTracker Settings window: the folders the wallpapers come from, how often and in what
 * order they change, and whether each workspace has its own. The settings go to a file the SnakeTracker server reads; the
 * server changes the wallpaper with Haiku's own settings (see SnakeWallpaper.h).
 */
#ifndef SNAKE_WALLPAPER_VIEW_H
#define SNAKE_WALLPAPER_VIEW_H


#include <GroupView.h>
#include <String.h>


class BButton;
class BCheckBox;
class BFilePanel;
class BListView;
class BStringView;
class BTextControl;
class BMenuField;
class BPopUpMenu;


namespace BPrivate {

class SnakeWallpaperView : public BGroupView {
public:
							SnakeWallpaperView();
	virtual					~SnakeWallpaperView();

	virtual void			MessageReceived(BMessage* message);

	// the window was brought forward, or went to another workspace: the picture shown is the current one again
	void					Refresh() { _ShowCurrent(); }
	// the window is closing: what was typed is kept (and the server, if it is running, reads it again; one of another build is
	// left alone: starting that over is not for a window that is closing, which may be a Tracker that is quitting)
	void					Save();

private:
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


#endif	// SNAKE_WALLPAPER_VIEW_H
