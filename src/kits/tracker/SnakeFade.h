/*
 * A fade from one Desktop picture to the next (SnakeTracker): the two pictures, each as the Desktop shows it, are blended a
 * step at a time (by the time that has passed, so it takes as long as it was asked to even when the steps are slow) and each
 * blend is the view's bitmap; at the end the real picture is shown again, as Haiku's own code shows it.
 */
#ifndef SNAKE_FADE_H
#define SNAKE_FADE_H


#include <Handler.h>
#include <OS.h>


class BBitmap;
class BMessageRunner;
class BView;


namespace BPrivate {

class BackgroundImage;

class SnakeFade : public BHandler {
public:
	// Takes the two bitmaps (the view's size, B_RGB32) and begins; false when it can't (they are not taken then).
	// effect: 0 fade, 1 wipe, 2 slide, 3 circle reveal (see SnakeWallpaper.h).
	static bool				Start(BView* view, BBitmap* from, BBitmap* to, bigtime_t duration, BackgroundImage* image,
								int32 effect = 0);
	// Tells the Deskbar that the Desktop shows another picture now (it takes the screen behind its rounded corners again).
	static void				TellDeskbar();
	// Stops a fade that belongs to this picture (the picture is about to go).
	static void				Cancel(BackgroundImage* image);

	virtual void			MessageReceived(BMessage* message);

private:
							SnakeFade(BView* view, BBitmap* from, BBitmap* to, bigtime_t duration,
								BackgroundImage* image, int32 effect);
	virtual					~SnakeFade();

			void			_Step();
			void			_Finish(bool showPicture);

			BView*			fView;
			BBitmap*		fFrom;
			BBitmap*		fTo;
			BBitmap*		fFrames[2];
			int32			fNext;
			bigtime_t		fStart;
			bigtime_t		fDuration;
			BackgroundImage* fImage;
			int32			fEffect;
			BMessageRunner*	fRunner;
};

}	// namespace BPrivate


#endif	// SNAKE_FADE_H
