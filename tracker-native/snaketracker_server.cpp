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
 * It also watches for the system's Tracker or Deskbar being started (a restart by name) and replaces it with ours.
 * Distributed under the terms of the MIT license.
 */
#include <Alert.h>
#include <Application.h>
#include <FindDirectory.h>
#include <LaunchRoster.h>
#include <Message.h>
#include <Messenger.h>
#include <Path.h>
#include <Roster.h>
#include <Url.h>
#include <String.h>

#include <curl/curl.h>
#include <OS.h>
#include <image.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>


static const char* kServerSignature = "application/x-vnd.snaketracker-server";
static const char* kAppFolder = "/boot/system/apps/SnakeTracker";

// The version this was built as (the VERSION file; the Makefile passes it in).
#ifndef SNAKETRACKER_VERSION
#define SNAKETRACKER_VERSION "0.0.0"
#endif
static const char* kVersionUrl
	= "https://raw.githubusercontent.com/ablyssx74/haiku/refs/heads/master/tracker-native/VERSION";
static const char* kReleasesUrl = "https://github.com/ablyssx74/haiku/releases";

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
static int32 sSupervising[kProgramCount];	// 1 while a thread keeps program i running


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
	if (path.Path() == NULL)
		return true;	// (it is going away; still counts as running)
	return strncmp(path.Path(), kAppFolder, strlen(kAppFolder)) != 0;
}


// Runs one of our programs, and again when it quits or dies (the launch daemon would, for the system's own; this server
// stopped that job). It waits a few seconds first: a restart right away would run into the copy that is still shutting
// down, and when the system itself is shutting down this server is asked to quit within that time, which ends this.
// (The registrar does not start programs during a shutdown anyway.)
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
			// (one that quits cleanly right away was probably refused, as a second copy, and counts as a failure)
			if (quitNormally && time(NULL) - started >= 5)
				tries = 0;
			else
				tries++;
		}

		if (tries > 20)
			break;
		for (int i = 0; i < 30 && !sStop; i++)
			snooze(100000);
	}
	atomic_set(&sSupervising[program - kPrograms], 0);
	return 0;
}


// Keeps program i running, unless something is already doing that.
static void
StartSupervising(int i)
{
	if (atomic_test_and_set(&sSupervising[i], 1, 0) != 0)
		return;
	thread_id thread = spawn_thread(SuperviseThread, kPrograms[i].job, B_NORMAL_PRIORITY, (void*)&kPrograms[i]);
	if (thread >= 0)
		resume_thread(thread);
	else
		atomic_set(&sSupervising[i], 0);
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

		StartSupervising(i);
	}
	return 0;
}


// The system's Tracker or Deskbar was started (ProcessController's Restart items and the Deskbar ask for them by name, and
// the launch daemon's own definition of that name may still be the system's): put ours in its place.
static int32
ReplaceThread(void* cookie)
{
	const int i = (int)(addr_t)cookie;
	const Program& program = kPrograms[i];
	team_id team = be_roster->TeamFor(program.signature);
	if (team >= 0) {
		app_info info;
		if (be_roster->GetRunningAppInfo(team, &info) == B_OK) {
			BPath path(&info.ref);
			if (path.Path() != NULL && strncmp(path.Path(), kAppFolder, strlen(kAppFolder)) == 0)
				return 0;
		}
		BMessenger(NULL, team).SendMessage(B_QUIT_REQUESTED);
		for (int n = 0; n < 30 && !sStop && be_roster->IsRunning(program.signature); n++)
			snooze(100000);
		if (be_roster->IsRunning(program.signature))
			kill_team(team);
	}
	BLaunchRoster().Stop(program.job);
	for (int n = 0; n < 30 && !sStop && OtherInstanceRunning(program); n++)
		snooze(500000);
	StartSupervising(i);
	return 0;
}


// #pragma mark - update check


static size_t
WriteVersion(char* data, size_t size, size_t count, void* cookie)
{
	BString* text = (BString*)cookie;
	if (text->Length() < 256)
		text->Append(data, size * count);
	return size * count;
}


// "v1.0.48", "1.0.48" or a line with those in it, as one number to compare (-1 when there is no version in it)
static int32
VersionNumber(const char* text)
{
	int major = 0, middle = 0, minor = 0;
	if (sscanf(text, "%*[^0-9]%d.%d.%d", &major, &middle, &minor) != 3
		&& sscanf(text, "%d.%d.%d", &major, &middle, &minor) != 3) {
		return -1;
	}
	return major * 10000 + middle * 100 + minor;
}


// A little after the desktop is up, looks at the VERSION file of the repository on GitHub, and if there is a newer
// SnakeTracker, says so and offers the page to download it from. Nothing else happens without a connection, and an empty
// file ~/config/settings/snaketracker_no_update_check turns it off.
static int32
UpdateThread(void*)
{
	for (int i = 0; i < 300 && !sStop; i++)
		snooze(100000);
	if (sStop)
		return 0;

	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK && path.Append("snaketracker_no_update_check") == B_OK
		&& access(path.Path(), F_OK) == 0) {
		return 0;
	}

	BString remote;
	CURL* curl = curl_easy_init();
	if (curl == NULL)
		return 0;
	// (SNAKETRACKER_UPDATE_URL is for trying the check against another file)
	const char* url = getenv("SNAKETRACKER_UPDATE_URL");
	curl_easy_setopt(curl, CURLOPT_URL, url != NULL && url[0] != '\0' ? url : kVersionUrl);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteVersion);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &remote);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "SnakeTracker-Update-Checker/1.0");
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	CURLcode result = curl_easy_perform(curl);
	long status = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
	curl_easy_cleanup(curl);
	if (result != CURLE_OK || status != 200 || sStop)
		return 0;

	remote.Trim();
	const int32 remoteNumber = VersionNumber(remote.String());
	const int32 localNumber = VersionNumber(SNAKETRACKER_VERSION);
	if (remoteNumber < 0 || localNumber < 0 || remoteNumber <= localNumber)
		return 0;

	BString text("A newer version of SnakeTracker is available (");
	text << remote << "). You are running v" << SNAKETRACKER_VERSION
		<< ".\n\nVisit the GitHub page to download the latest version.";
	BAlert* alert = new BAlert("SnakeTracker update", text.String(), "Later", "Open the GitHub page", NULL,
		B_WIDTH_AS_USUAL, B_INFO_ALERT);
	alert->SetShortcut(0, B_ESCAPE);
	if (alert->Go() == 1) {
		BUrl page(kReleasesUrl, true);
		page.OpenWithPreferredApplication();
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
		be_roster->StartWatching(BMessenger(this), B_REQUEST_LAUNCHED);
		thread = spawn_thread(UpdateThread, "snaketracker update check", B_LOW_PRIORITY, NULL);
		if (thread >= 0)
			resume_thread(thread);
	}

	virtual void MessageReceived(BMessage* message)
	{
		if (message->what != B_SOME_APP_LAUNCHED) {
			BApplication::MessageReceived(message);
			return;
		}
		const char* signature;
		entry_ref ref;
		if (message->FindString("be:signature", &signature) != B_OK || message->FindRef("be:ref", &ref) != B_OK)
			return;
		BPath path(&ref);
		if (path.Path() == NULL || strncmp(path.Path(), kAppFolder, strlen(kAppFolder)) == 0)
			return;
		for (int i = 0; i < kProgramCount; i++) {
			if (strcasecmp(signature, kPrograms[i].signature) != 0)
				continue;
			thread_id thread = spawn_thread(ReplaceThread, "snaketracker replace", B_LOW_PRIORITY, (void*)(addr_t)i);
			if (thread >= 0)
				resume_thread(thread);
		}
	}

	virtual bool QuitRequested()
	{
		be_roster->StopWatching(BMessenger(this));
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
