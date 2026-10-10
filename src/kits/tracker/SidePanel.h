/*
 * The side panel of a Tracker folder window: the user's favourite folders, each opening into its subfolders as it is
 * needed. Clicking a folder shows it in the window. The folder the window shows is kept selected in the tree.
 */
#ifndef _SIDE_PANEL_H
#define _SIDE_PANEL_H


#include <Entry.h>
#include <View.h>


class BMessageRunner;
class BVolumeRoster;
class BScrollView;


namespace BPrivate {

class BContainerWindow;
class SideTree;


class TSidePanel : public BView {
public:
							TSidePanel(BContainerWindow* window);
	virtual					~TSidePanel();

	virtual	void			AttachedToWindow();
	virtual	void			MessageReceived(BMessage* message);

	// selects the folder the window is showing, opening the tree down to it
			void			ShowCurrentFolder();

private:
			BContainerWindow*	fWindow;
			SideTree*			fTree;
			BScrollView*		fScrollView;
			BMessageRunner*		fFollowRunner;
			BVolumeRoster*		fVolumeRoster;
			node_ref			fShown;
};

}	// namespace BPrivate


#endif	// _SIDE_PANEL_H
