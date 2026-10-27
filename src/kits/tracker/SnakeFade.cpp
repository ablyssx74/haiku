/*
 * A fade from one Desktop picture to the next: see SnakeFade.h.
 */


#include "SnakeFade.h"

#include <math.h>

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
SnakeFade::Start(BView* view, BBitmap* from, BBitmap* to, bigtime_t duration, BackgroundImage* image, int32 effect)
{
	BWindow* window = view->Window();
	if (window == NULL || from == NULL || to == NULL || from->BitsLength() != to->BitsLength())
		return false;
	if (sFade != NULL)
		sFade->_Finish(false);

	SnakeFade* fade = new(std::nothrow) SnakeFade(view, from, to, duration, image, effect);
	if (fade == NULL)
		return false;
	if (fade->fFrames[0] == NULL || fade->fFrames[1] == NULL) {
		fade->fFrom = fade->fTo = NULL;		// (the caller keeps them)
		delete fade;
		return false;
	}
	sFade = fade;
	// hDesktop (the dock) draws the same change under itself: it is told when this one began and how, so that the two
	// start together and keep the same time (the start is system_time(), which both read).
	BMessenger dock("application/x-vnd.hdesktop");
	if (dock.IsValid()) {
		BMessage began('SnkF');
		began.AddInt64("start", fade->fStart);
		began.AddInt32("ms", (int32)(fade->fDuration / 1000));
		began.AddInt32("effect", effect);
		dock.SendMessage(&began);
	}
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


SnakeFade::SnakeFade(BView* view, BBitmap* from, BBitmap* to, bigtime_t duration, BackgroundImage* image, int32 effect)
	:
	BHandler("snake fade"),
	fView(view),
	fFrom(from),
	fTo(to),
	fNext(0),
	fStart(system_time()),
	fDuration(duration > 0 ? duration : 1),
	fImage(image),
	fEffect(effect),
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


// Two colors blended, t of 256 for the second (two channels at a time).
static inline uint32
Blend(uint32 a, uint32 b, uint32 t)
{
	const uint32 u = 256 - t;
	const uint32 redBlue = (((a & 0x00ff00ff) * u + (b & 0x00ff00ff) * t) >> 8) & 0x00ff00ff;
	const uint32 green = (((a & 0x0000ff00) * u + (b & 0x0000ff00) * t) >> 8) & 0x0000ff00;
	return redBlue | green;
}


// The picture at this moment: the fade blends the two as far along as the time is (in 256ths); the wipe, the slide and
// the circle reveal move by the time with an easing (slow, quick, slow), and the edge of the wipe and of the circle is
// soft. hDesktop draws the same thing under its dock, so these numbers are kept in step with it (hdesktop.cpp).
void
SnakeFade::_Step()
{
	const bigtime_t passed = system_time() - fStart;
	if (passed >= fDuration) {
		_Finish(true);
		return;
	}
	BBitmap* frame = fFrames[fNext];
	fNext = 1 - fNext;
	const uint32* from = (const uint32*)fFrom->Bits();
	const uint32* to = (const uint32*)fTo->Bits();
	uint32* out = (uint32*)frame->Bits();
	const BRect bounds(frame->Bounds());
	const int32 width = (int32)bounds.Width() + 1, height = (int32)bounds.Height() + 1;
	const int32 stride = frame->BytesPerRow() / 4;
	const float linear = (float)passed / (float)fDuration;
	const float eased = linear * linear * (3.0f - 2.0f * linear);

	if (fEffect == 1) {
		// wipe: a soft edge, a tenth of the width, goes from the left to the right
		const int32 feather = width / 10 > 1 ? width / 10 : 1;
		const int32 edge = (int32)(eased * (width + feather));
		for (int32 y = 0; y < height; y++) {
			const uint32* f = from + y * stride;
			const uint32* t = to + y * stride;
			uint32* o = out + y * stride;
			for (int32 x = 0; x < width; x++) {
				const int32 d = edge - x;
				o[x] = d <= 0 ? f[x] : d >= feather ? t[x] : Blend(f[x], t[x], (uint32)(d * 256 / feather));
			}
		}
	} else if (fEffect == 2) {
		// slide: the new picture comes in from the right and pushes the old one out to the left
		const int32 shift = (int32)(eased * width);
		for (int32 y = 0; y < height; y++) {
			const uint32* f = from + y * stride;
			const uint32* t = to + y * stride;
			uint32* o = out + y * stride;
			for (int32 x = 0; x < width - shift; x++)
				o[x] = f[x + shift];
			for (int32 x = width - shift; x < width; x++)
				o[x] = t[x - (width - shift)];
		}
	} else if (fEffect == 3) {
		// circle reveal: from the middle of the screen, with a soft edge of 6% of the width
		const float feather = width * 0.06f > 2.0f ? width * 0.06f : 2.0f;
		const float cx = width / 2.0f, cy = height / 2.0f;
		const float maxRadius = sqrtf(cx * cx + cy * cy) + feather;
		const float radius = eased * maxRadius;
		const float inner = radius > feather ? radius - feather : 0.0f;
		for (int32 y = 0; y < height; y++) {
			const uint32* f = from + y * stride;
			const uint32* t = to + y * stride;
			uint32* o = out + y * stride;
			const float dy = y + 0.5f - cy;
			const float outerSquare = radius * radius - dy * dy, innerSquare = inner * inner - dy * dy;
			const float outerHalf = outerSquare > 0 ? sqrtf(outerSquare) : -1.0f;
			const float innerHalf = innerSquare > 0 ? sqrtf(innerSquare) : -1.0f;
			for (int32 x = 0; x < width; x++) {
				const float dx = fabsf(x + 0.5f - cx);
				if (dx <= innerHalf)
					o[x] = t[x];
				else if (dx > outerHalf)
					o[x] = f[x];
				else {
					const float d = sqrtf(dx * dx + dy * dy);
					float a = (radius - d) / feather;
					a = a < 0 ? 0 : a > 1 ? 1 : a;
					o[x] = Blend(f[x], t[x], (uint32)(a * 256));
				}
			}
		}
	} else {
		const uint32 t = (uint32)(passed * 256 / fDuration);
		const size_t count = frame->BitsLength() / 4;
		for (size_t i = 0; i < count; i++)
			out[i] = Blend(from[i], to[i], t);
	}
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
