// (the second window makes the first one inactive)
// Magenta window on a green backdrop at a known frame: the screenshot shows where each theme's frame edge falls
#include <Application.h>
#include <Screen.h>
#include <View.h>
#include <Window.h>
#include <OS.h>
int main() {
	BApplication app("application/x-vnd.test-edge");
	BScreen screen;
	BWindow* back = new BWindow(screen.Frame(), "bk", B_NO_BORDER_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
		B_NOT_MOVABLE | B_NOT_RESIZABLE | B_NOT_CLOSABLE | B_NOT_ZOOMABLE | B_AVOID_FOCUS);
	BView* bv = new BView(back->Bounds(), "bv", B_FOLLOW_ALL, B_WILL_DRAW);
	bv->SetViewColor(0, 255, 0);
	back->AddChild(bv);
	back->Show();
	snooze(600000);
	BWindow* w = new BWindow(BRect(400, 300, 799, 599), "edge", B_TITLED_WINDOW, B_NOT_ZOOMABLE | B_NOT_RESIZABLE);
	BView* v = new BView(w->Bounds(), "v", B_FOLLOW_ALL, B_WILL_DRAW);
	v->SetViewColor(255, 0, 255);
	w->AddChild(v);
	w->Show();
	snooze(1500000);
	BWindow* w2 = new BWindow(BRect(1000, 300, 1300, 500), "other", B_TITLED_WINDOW, 0);
	w2->Show();
	w2->Activate(true);
	snooze(5500000);
	return 0;
}
