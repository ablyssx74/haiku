/*
 * snaketracker_server: a small background server that makes sure the snake-trail Tracker and Deskbar are the ones
 * running.
 *
 * The package's launch file (data/user_launch/x-snake-tracker) tells the launch daemon to start the builds in
 * /system/apps/SnakeTracker, but the daemon applies its launch files in the order they were installed, and a later
 * system file (or a leftover file of your own in ~/config/settings/launch) can undo that. This server starts with the
 * desktop and looks at what the daemon is going to start. If it has it right there is nothing to do and the server
 * sits idle. Otherwise it stops the system's Tracker and/or Deskbar, starts ours and starts them again if they crash.
 *
 * It is a real application (a background one), so the system can ask it to quit when it shuts down, and it answers.
 * Distributed under the terms of the MIT license.
 */
#include <Application.h>
#include <LaunchRoster.h>
#include <Message.h>
#include <Path.h>
#include <Roster.h>
#include <String.h>

#include <OS.h>
#include <image.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>


static const char* kServerSignature = "application/x-vnd.snaketracker-server";
static const char* kAppFolder = "/boot/system/apps/SnakeTracker";

struct Program {
	const char*	job;			// the launch daemon's name for the service
	const char*	signature;		// the program's application signature
	const char*	path;			// ours
};

static const Program kPrograms[] = {
	{ "x-vnd.be-trak", "application/x-vnd.Be-TRAK", "/boot/system/apps/SnakeTracker/Tracker" },
	{ "x-vnd.be-tskb", "application/x-vnd.Be-TSKB", "/boot/system/apps/SnakeTracker/Deskbar" },
};
static const int kProgramCount = 2;

extern char** environ;

static volatile int32 sStop = 0;


// does the launch daemon start ours for this service?
static bool
UsesOurs(const char* job)
{
	BLaunchRoster roster;
	BMessage info;
	if (roster.GetJobInfo(job, info) != B_OK)
		return false;

	// (the launch line is somewhere in the description; look for our folder in all of it)
	ssize_t size = info.FlattenedSize();
	if (size <= 0)
		return false;
	char* buffer = (char*)malloc(size + 1);
	if (buffer == NULL)
		return false;
	bool found = false;
	if (info.Flatten(buffer, size) == B_OK) {
		buffer[size] = '\0';
		for (ssize_t i = 0; i + 12 <= size; i++) {
			if (memcmp(buffer + i, "SnakeTracker", 12) == 0) {
				found = true;
				break;
			}
		}
	}
	free(buffer);
	return found;
}


// is a program with this signature running that is not ours?
static bool
OtherInstanceRunning(const Program& program)
{
	app_info info;
	if (be_roster->GetAppInfo(program.signature, &info) != B_OK)
		return false;
	BPath path(&info.ref);
	return strncmp(path.Path(), kAppFolder, strlen(kAppFolder)) != 0;
}


// Runs one of our programs, again if it dies abnormally or right away (the system's copy may still be shutting down,
// and Tracker and Deskbar refuse to run twice). A normal quit after it ran for a while ends it: that is what the
// system asking it to quit at shutdown looks like, and the user quitting it.
static int32
SuperviseThread(void* cookie)
{
	const Program* program = (const Program*)cookie;
	int tries = 0;

	while (!sStop) {
		const char* argv[] = { program->path, NULL };
		time_t started = time(NULL);

		thread_id thread = load_image(1, argv, (const char**)environ);
		if (thread < 0) {
			tries++;
		} else {
			resume_thread(thread);
			// (wait_for_thread() says 0 even for a program that was killed or crashed; waitpid() tells them apart)
			int status = 0;
			bool quitNormally = waitpid(thread, &status, 0) == thread && WIFEXITED(status)
				&& WEXITSTATUS(status) == 0;
			int ran = (int)(time(NULL) - started);
			if (quitNormally && ran >= 10)
				break;
			tries++;
		}

		if (tries > 20)
			break;
		for (int i = 0; i < 20 && !sStop; i++)
			snooze(100000);
	}
	return 0;
}


static int32
HandoffThread(void*)
{
	// give the launch daemon a moment to have read all its files
	for (int i = 0; i < 10 && !sStop; i++)
		snooze(100000);

	BLaunchRoster roster;
	for (int i = 0; i < kProgramCount && !sStop; i++) {
		const Program& program = kPrograms[i];
		if (UsesOurs(program.job))
			continue;

		roster.Stop(program.job);
		// wait (up to ~15 s) until the system's copy is gone
		for (int n = 0; n < 30 && !sStop && OtherInstanceRunning(program); n++)
			snooze(500000);

		thread_id thread = spawn_thread(SuperviseThread, program.job, B_NORMAL_PRIORITY, (void*)&program);
		if (thread >= 0)
			resume_thread(thread);
	}
	return 0;
}


class ServerApp : public BApplication {
public:
	ServerApp()
		:
		BApplication(kServerSignature)
	{
	}

	virtual void ReadyToRun()
	{
		thread_id thread = spawn_thread(HandoffThread, "snaketracker handoff", B_LOW_PRIORITY, NULL);
		if (thread >= 0)
			resume_thread(thread);
	}

	virtual bool QuitRequested()
	{
		sStop = 1;
		return true;
	}
};


int
main()
{
	ServerApp app;
	app.Run();
	return 0;
}
