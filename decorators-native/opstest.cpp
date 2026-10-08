// Puts a window through resizes, moves, title changes and minimizing on a green backdrop, and looks at the
// screen after every step for pixels that are neither the backdrop, the window nor the frame art close to it:
// stale pieces of a decorator (a "ghost" past the window's edge).
//
//   opstest <rounds> <seed>      prints one line per step that leaves something behind, and saves a screenshot
//                                of it as /tmp/ops_<theme>_<step>.png (the theme is the first word of the title bar)
#include <Application.h>
#include <Bitmap.h>
#include <Screen.h>
#include <View.h>
#include <Window.h>
#include <OS.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool
IsGreen(const uint8* p)
{
	return p[0] == 0 && p[1] == 255 && p[2] == 0;
}


// pixels outside the window's own surroundings that are not the backdrop
static int
Stray(BRect frame, BRect* box)
{
	BScreen screen;
	BBitmap* bitmap = NULL;
	if (screen.GetBitmap(&bitmap, false, NULL) != B_OK || bitmap == NULL)
		return -1;
	BRect allowed(frame.left - 40, frame.top - 80, frame.right + 40, frame.bottom + 40);
	BRect look(100, 100, 1300, 760);
	int count = 0;
	BRect bounds(1e6, 1e6, -1, -1);
	const uint8* bits = (const uint8*)bitmap->Bits();
	int32 bpr = bitmap->BytesPerRow();
	for (int32 y = (int32)look.top; y <= (int32)look.bottom; y++) {
		for (int32 x = (int32)look.left; x <= (int32)look.right; x++) {
			if (allowed.Contains(BPoint(x, y)))
				continue;
			const uint8* pixel = bits + y * bpr + x * 4;
			if (IsGreen(pixel))
				continue;
			count++;
			bounds.left = min_c(bounds.left, x);
			bounds.top = min_c(bounds.top, y);
			bounds.right = max_c(bounds.right, x);
			bounds.bottom = max_c(bounds.bottom, y);
		}
	}
	delete bitmap;
	*box = bounds;
	return count;
}


int
main(int argc, char** argv)
{
	int rounds = argc > 1 ? atoi(argv[1]) : 30;
	srand(argc > 2 ? atoi(argv[2]) : 1);
	const char* theme = getenv("OPS_THEME") ? getenv("OPS_THEME") : "?";

	BApplication app("application/x-vnd.test-ops");
	BScreen screen;
	BWindow* back = new BWindow(screen.Frame(), "bk", B_NO_BORDER_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_NOT_MOVABLE | B_NOT_RESIZABLE | B_NOT_CLOSABLE | B_NOT_ZOOMABLE | B_AVOID_FOCUS);
	BView* bv = new BView(back->Bounds(), "bv", B_FOLLOW_ALL, B_WILL_DRAW);
	bv->SetViewColor(0, 255, 0);
	back->AddChild(bv);
	back->Show();
	snooze(600000);

	BWindow* w = new BWindow(BRect(400, 300, 799, 599), "start", B_TITLED_WINDOW, B_NOT_ZOOMABLE);
	BView* v = new BView(w->Bounds(), "v", B_FOLLOW_ALL, B_WILL_DRAW);
	v->SetViewColor(255, 0, 255);
	w->AddChild(v);
	w->Show();
	w->Activate(true);
	snooze(1200000);

	static const char* kTitles[] = {"a", "a longer title for the window", "x", "/boot/home/config/settings",
		"Appearance", "a title long enough to need cutting short, surely, in a small window"};
	int found = 0;
	for (int step = 0; step < rounds; step++) {
		char what[128];
		int op = rand() % 6;
		w->Lock();
		switch (op) {
			case 0: {
				float width = 220 + rand() % 640, height = 140 + rand() % 360;
				w->ResizeTo(width, height);
				snprintf(what, sizeof(what), "resize %.0fx%.0f", width + 1, height + 1);
				break;
			}
			case 1: {
				float x = 200 + rand() % 500, y = 200 + rand() % 200;
				w->MoveTo(x, y);
				snprintf(what, sizeof(what), "move %.0f,%.0f", x, y);
				break;
			}
			case 2: {
				const char* title = kTitles[rand() % 6];
				w->SetTitle(title);
				snprintf(what, sizeof(what), "title %s", title);
				break;
			}
			case 3:
				w->Minimize(true);
				strcpy(what, "minimize");
				break;
			case 4: {
				float x = 200 + rand() % 500, y = 200 + rand() % 200;
				float width = 220 + rand() % 640, height = 140 + rand() % 360;
				w->MoveTo(x, y);
				w->ResizeTo(width, height);
				snprintf(what, sizeof(what), "move+resize %.0f,%.0f %.0fx%.0f", x, y, width + 1, height + 1);
				break;
			}
			default:
				w->Hide();
				w->Show();
				strcpy(what, "hide+show");
				break;
		}
		w->Unlock();
		snooze(700000);
		if (w->IsMinimized()) {
			w->Lock();
			w->Minimize(false);
			w->Unlock();
			snooze(700000);
		}
		w->Lock();
		BRect frame = w->Frame();
		w->Unlock();

		BRect box;
		int count = Stray(frame, &box);
		if (count > 0) {
			found++;
			printf("%-12s step %2d %-40s frame (%.0f,%.0f)-(%.0f,%.0f): %d stray pixels in (%.0f,%.0f)-(%.0f,%.0f)\n",
				theme, step, what, frame.left, frame.top, frame.right, frame.bottom, count, box.left, box.top,
				box.right, box.bottom);
			char command[128];
			snprintf(command, sizeof(command), "screenshot -s /tmp/ops_%s_%d.png >/dev/null 2>&1", theme, step);
			system(command);
		}
	}
	printf("%-12s %d of %d steps left something behind\n", theme, found, rounds);
	return 0;
}
