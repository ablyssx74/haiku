/*
 * The Custom HVIF Icons tab of the SnakeTracker Settings window: the icon in the Deskbar's header (an HVIF file of the user's, or
 * the SnakeTracker snake), how much taller the header is, and where the icon sits in it. The Deskbar reads what is set here (see
 * SnakeHeader.h) and is told when it changes.
 */
#ifndef SNAKE_HEADER_ICON_VIEW_H
#define SNAKE_HEADER_ICON_VIEW_H


#include <GroupView.h>


class BFilePanel;
class BPopUpMenu;
class BSlider;
class BStringView;


namespace BPrivate {

class HeaderPreview;
class TrackerIconPreview;

class SnakeHeaderIconView : public BGroupView {
public:
							SnakeHeaderIconView();
	virtual					~SnakeHeaderIconView();

	virtual void			MessageReceived(BMessage* message);

private:
			void			_Choose(const entry_ref& ref);
			void			_UseDefault();
			void			_ChooseTracker(const entry_ref& ref);
			void			_UseDefaultTracker();
			void			_Update();
			void			_SaveSettings();
			void			_TellDeskbar();
			void			_TellDock();

			HeaderPreview*	fPreview;
			BStringView*	fSource;
			BStringView*	fHeightLabel;
			BSlider*		fHeight;
			BStringView*	fSizeLabel;
			BSlider*		fSize;
			BPopUpMenu*		fAlignMenu;
			BPopUpMenu*		fVAlignMenu;
			BFilePanel*		fPanel;
			TrackerIconPreview* fTrackerPreview;
			BStringView*	fTrackerSource;
			BFilePanel*		fTrackerPanel;
};

}	// namespace BPrivate


#endif	// SNAKE_HEADER_ICON_VIEW_H
