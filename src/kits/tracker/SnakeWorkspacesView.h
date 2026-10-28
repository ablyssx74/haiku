/*
 * The Workspaces tab of the SnakeTracker Settings window: how the wallpaper changes when you go to another workspace (with the
 * Workspaces preferences, the Deskbar or hDesktop). See SnakeWorkspaces.h.
 */
#ifndef SNAKE_WORKSPACES_VIEW_H
#define SNAKE_WORKSPACES_VIEW_H


#include <GroupView.h>


class BPopUpMenu;


namespace BPrivate {

class SnakeWorkspacesView : public BGroupView {
public:
							SnakeWorkspacesView();

	virtual void			MessageReceived(BMessage* message);

private:
			void			_Save();

			BPopUpMenu*		fFadeMenu;
			BPopUpMenu*		fEffectMenu;
};

}	// namespace BPrivate


#endif	// SNAKE_WORKSPACES_VIEW_H
