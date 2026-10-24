/*
 * Rounded outer corners for the Deskbar's window (SnakeTracker).
 *
 * A window can't be cut into a rounded shape, so the Deskbar's window loses its square border and four small views, one
 * over each corner, paint the screen behind the Deskbar (taken while the Deskbar is hidden for a moment) outside an arc;
 * thin views along the sides draw the outline the border was.
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

private:
			enum { kTopLeft, kTopRight, kBottomLeft, kBottomRight, kCornerCount };
			enum { kTop, kBottom, kLeft, kRight, kEdgeCount };

			BBitmap*			_Picture(int corner, const BRect& rect, BBitmap* behind, const BRect& behindRect,
									const rgb_color& border);
			void				_Remove();

			BWindow*			fDeskbar;
			SnakeCornerView*	fCorners[kCornerCount];
			SnakeEdgeView*		fEdges[kEdgeCount];
			BRect				fRects[kCornerCount];
			bool				fWanted[kCornerCount];
			bool				fActive;
			int32				fWorkspace;
			window_look			fOriginalLook;
};


#endif	// SNAKE_CORNERS_H
