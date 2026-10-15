// Makes three windows stacked into one with tabs, as dragging a tab onto another with the Option key does,
// and brings each to the front in turn every few seconds.
#include <Application.h>
#include <Window.h>
#include <View.h>
#include <StringView.h>
#include <MessageRunner.h>
#include <Message.h>
#include <WindowStack.h>
#include <stdio.h>

static const uint32 kCycle = 'cycl';

class StackApp : public BApplication {
public:
	StackApp() : BApplication("application/x-vnd.stacktest"), fNext(0), fRunner(NULL) {}

	void ReadyToRun() override
	{
		const char* titles[3] = { "Haiku", "Demos", "Appearance" };
		for (int i = 0; i < 3; i++) {
			fWin[i] = new BWindow(BRect(100, 100, 560, 360), titles[i], B_TITLED_WINDOW, B_QUIT_ON_WINDOW_CLOSE);
			BView* v = new BView("v", B_WILL_DRAW);
			v->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
			BStringView* s = new BStringView(NULL, titles[i]);
			v->AddChild(s); s->MoveTo(20, 20);
			fWin[i]->AddChild(v);
			fWin[i]->Show();
		}
		BWindowStack stack(fWin[0]);
		for (int i = 1; i < 3; i++) {
			fWin[i]->Lock(); BWindow* w = fWin[i]; fWin[i]->Unlock();
			status_t r = stack.AddWindow(w);
			printf("stack add %d: %s, count %d\n", i, strerror(r), (int)stack.CountWindows());
			fflush(stdout);
		}
		fWin[2]->Activate();
		fRunner = new BMessageRunner(BMessenger(this), new BMessage(kCycle), 5000000);
	}

	void MessageReceived(BMessage* m) override
	{
		if (m->what == kCycle) {
			fNext = (fNext + 1) % 3;
			fWin[fNext]->Activate();
			printf("active: %d\n", (int)fNext); fflush(stdout);
		} else
			BApplication::MessageReceived(m);
	}

private:
	BWindow* fWin[3];
	int fNext;
	BMessageRunner* fRunner;
};

int main() { StackApp app; app.Run(); return 0; }
