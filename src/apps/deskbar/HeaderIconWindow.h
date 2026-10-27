/*
 * The Deskbar's header icon: the snake in the leaf's block. It can be replaced by a vector icon (an HVIF file) of the
 * user's own, chosen in a small window (Deskbar menu: "Change Header Icon..."). The chosen file is kept as a copy in the
 * Deskbar's settings folder.
 */
#ifndef HEADER_ICON_WINDOW_H
#define HEADER_ICON_WINDOW_H


#include <SupportDefs.h>
#include <Window.h>


class BBitmap;
class BFilePanel;
class BPath;
class BStringView;
class BView;


const uint32 kHeaderIconShow	= 'HiSh';	// to the app: open the window
const uint32 kHeaderIconChanged	= 'HiCh';	// to the app: the icon is another one, the bar is drawn again


// the icon at \a size pixels, from the user's file if there is one, else the built-in snake (the caller owns it)
BBitmap*	HeaderIconBitmap(int32 size);
// changes whenever the icon does (0 for the built-in one)
int64		HeaderIconStamp();


class HeaderIconWindow : public BWindow {
public:
								HeaderIconWindow();
	virtual						~HeaderIconWindow();

	virtual	void				MessageReceived(BMessage* message);

	// opens the window, or brings the one that is open to the front
	static	void				ShowWindow();

private:
			void				_Choose(const entry_ref& ref);
			void				_UseDefault();
			void				_Update();

			BView*				fPreview;
			BStringView*		fSource;
			BFilePanel*			fPanel;

	static	HeaderIconWindow*	sWindow;
};


#endif	// HEADER_ICON_WINDOW_H
