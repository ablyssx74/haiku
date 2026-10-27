/*
 * The "SnakeTracker Workspaces" window of the Desktop's menu: how the wallpaper changes when you go to another workspace (with
 * the Workspaces preferences, the Deskbar or hDesktop). See SnakeWorkspaces.h.
 */
#ifndef SNAKE_WORKSPACES_WINDOW_H
#define SNAKE_WORKSPACES_WINDOW_H


#include <Window.h>


class BPopUpMenu;


namespace BPrivate {

class SnakeWorkspacesWindow : public BWindow {
public:
	// opens the window, or brings the open one to the front
	static void				Open();

	virtual bool			QuitRequested();
	virtual void			MessageReceived(BMessage* message);

private:
							SnakeWorkspacesWindow();

			void			_Save();

			BPopUpMenu*		fFadeMenu;
			BPopUpMenu*		fEffectMenu;
};

}	// namespace BPrivate


#endif	// SNAKE_WORKSPACES_WINDOW_H
