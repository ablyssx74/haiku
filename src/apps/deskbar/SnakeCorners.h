/*
 * Rounded outer corners for the Deskbar's window (SnakeTracker).
 *
 * A window can't be cut into a rounded shape, so the Deskbar's window has no border of the window system (it is made without
 * one: changing a window's look at run time makes the window system reset its size limits) and four small views, one over
 * each corner, paint the screen behind the Deskbar (taken while the Deskbar is hidden for a moment) outside an arc; thin
 * views along the sides draw the outline the border was.
 */
#ifndef SNAKE_CORNERS_H
#define SNAKE_CORNERS_H


#include <InterfaceDefs.h>
#include <Rect.h>
#include <Window.h>
#include <SupportDefs.h>


class BBitmap;
class BWindow;
class SnakeCornerView;
class SnakeEdgeView;


class SnakeCorners {
public:
								SnakeCorners(BWindow* deskbar);
								~SnakeCorners();

			// Called from the Deskbar window's own thread, once its frame has settled. Takes the screen behind the
			// corners again only when they have moved. hidden: the Deskbar is auto-hidden (no corners then).
			void				Update(bool hidden);

			// Called just before the window gets another frame: a corner that will lie on ground the Deskbar does not
			// cover now takes its picture of it now, and the Deskbar need not be hidden afterwards.
			void				Prepare(const BRect& newFrame);

			// The wallpaper changed: what the corners show of the screen behind them is old.
			void				WallpaperChanged();

private:
			enum { kBackdropCount = 4, kStripCount = 4 };
			// The screen behind the Deskbar's side columns and rows (the corners lie in them, whatever the Deskbar's size).
			struct Backdrop {
				int32		workspace;
				BBitmap*	strips[kStripCount];
				BRect		rects[kStripCount];
				bigtime_t	when;
			};
			enum { kTopLeft, kTopRight, kBottomLeft, kBottomRight, kCornerCount };
			enum { kTop, kBottom, kLeft, kRight, kEdgeCount };

			BBitmap*			_Picture(int corner, const BRect& rect, BBitmap* behind, const BRect& behindRect,
									const rgb_color& border);
			void				_Remove();
			Backdrop*			_Backdrop(int32 workspace, const BRect& screenFrame);
			Backdrop*			_Capture(int32 workspace, const BRect& frame, const BRect& screenFrame);
			int					_Strip(const Backdrop* backdrop, const BRect& rect);
			void				_DropAhead();
			bool				_FromTracker(const bool* redo, const BRect* rects, int32 workspace, BBitmap** shots);
			void				_PlaceEdges(const bool* want, bool atLeft, bool atRight, bool atTop, bool atBottom,
									const rgb_color& line);

			BWindow*			fDeskbar;
			SnakeCornerView*	fCorners[kCornerCount];
			SnakeEdgeView*		fEdges[kEdgeCount];
			BRect				fRects[kCornerCount];
			bool				fWanted[kCornerCount];
			bool				fActive;
			int32				fWorkspace;
			Backdrop			fBackdrops[kBackdropCount];
			BBitmap*			fAhead[kCornerCount];
			BRect				fAheadRects[kCornerCount];
			int32				fAheadWorkspace;
			bool				fTrackerAnswers;		// (Tracker told the last time: no screen grabs are needed)
};


#endif	// SNAKE_CORNERS_H
