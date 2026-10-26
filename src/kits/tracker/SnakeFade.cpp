/*
 * A fade from one Desktop picture to the next: see SnakeFade.h.
 */


#include "SnakeFade.h"

#include <Bitmap.h>
#include <Message.h>
#include <MessageRunner.h>
#include <Messenger.h>
#include <View.h>
#include <Window.h>

#include <new>

#include "BackgroundImage.h"


namespace BPrivate {

static const uint32 kMsgFadeStep = 'Tfst';
static const bigtime_t kStepInterval = 30000;		// (about 30 steps a second, when the screen keeps up)

static SnakeFade* sFade = NULL;


bool
SnakeFade::Start(BView* view, BBitmap* from, BBitmap* to, bigtime_t duration, BackgroundImage* image)
{
	BWindow* window = view->Window();
	if (window == NULL || from == NULL || to == NULL || from->BitsLength() != to->BitsLength())
		return false;
	if (sFade != NULL)
		sFade->_Finish(false);

	SnakeFade* fade = new(std::nothrow) SnakeFade(view, from, to, duration, image);
	if (fade == NULL)
		return false;
	if (fade->fFrames[0] == NULL || fade->fFrames[1] == NULL) {
		fade->fFrom = fade->fTo = NULL;		// (the caller keeps them)
		delete fade;
		return false;
	}
	sFade = fade;
	window->AddHandler(fade);
	BMessage step(kMsgFadeStep);
	fade->fRunner = new BMessageRunner(BMessenger(fade, window), &step, kStepInterval);
	// the first picture right away, so that the screen goes on showing what it showed
	fade->_Step();
	return true;
}


void
SnakeFade::TellDeskbar()
{
	BMessenger deskbar("application/x-vnd.Be-TSKB");
	if (deskbar.IsValid())
		deskbar.SendMessage('SnkN');
}


void
SnakeFade::Cancel(BackgroundImage* image)
{
	if (sFade != NULL && sFade->fImage == image)
		sFade->_Finish(false);
}


SnakeFade::SnakeFade(BView* view, BBitmap* from, BBitmap* to, bigtime_t duration, BackgroundImage* image)
	:
	BHandler("snake fade"),
	fView(view),
	fFrom(from),
	fTo(to),
	fNext(0),
	fStart(system_time()),
	fDuration(duration > 0 ? duration : 1),
	fImage(image),
	fRunner(NULL)
{
	for (int i = 0; i < 2; i++) {
		fFrames[i] = new(std::nothrow) BBitmap(from->Bounds(), B_RGB32);
		if (fFrames[i] != NULL && fFrames[i]->InitCheck() != B_OK) {
			delete fFrames[i];
			fFrames[i] = NULL;
		}
	}
}


SnakeFade::~SnakeFade()
{
	delete fRunner;
	delete fFrom;
	delete fTo;
	delete fFrames[0];
	delete fFrames[1];
}


void
SnakeFade::MessageReceived(BMessage* message)
{
	if (message->what == kMsgFadeStep)
		_Step();
	else
		BHandler::MessageReceived(message);
}


// A blend of the two pictures, as far along as the time is (in 256ths: the two colors of a pixel are blended two channels at a
// time).
void
SnakeFade::_Step()
{
	const bigtime_t passed = system_time() - fStart;
	if (passed >= fDuration) {
		_Finish(true);
		return;
	}
	const uint32 t = (uint32)(passed * 256 / fDuration), u = 256 - t;
	BBitmap* frame = fFrames[fNext];
	fNext = 1 - fNext;
	const uint32* from = (const uint32*)fFrom->Bits();
	const uint32* to = (const uint32*)fTo->Bits();
	uint32* out = (uint32*)frame->Bits();
	const size_t count = frame->BitsLength() / 4;
	for (size_t i = 0; i < count; i++) {
		const uint32 a = from[i], b = to[i];
		const uint32 redBlue = (((a & 0x00ff00ff) * u + (b & 0x00ff00ff) * t) >> 8) & 0x00ff00ff;
		const uint32 green = (((a & 0x0000ff00) * u + (b & 0x0000ff00) * t) >> 8) & 0x0000ff00;
		out[i] = redBlue | green;
	}
	const BRect bounds(frame->Bounds());
	fView->SetViewBitmap(frame, bounds, bounds, B_FOLLOW_ALL, 0);
	fView->Invalidate();
}


void
SnakeFade::_Finish(bool showPicture)
{
	BWindow* window = fView->Window();
	if (sFade == this)
		sFade = NULL;
	if (showPicture && fImage != NULL) {
		fImage->Show(fView, current_workspace());
		TellDeskbar();
	}
	if (window != NULL)
		window->RemoveHandler(this);
	delete this;
}

}	// namespace BPrivate
