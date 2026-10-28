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
#include <Invoker.h>
#include <Application.h>
#include <FindDirectory.h>
#include <LaunchRoster.h>
#include <Message.h>
#include <Messenger.h>
#include <Path.h>
#include <Roster.h>
#include <Url.h>
#include <String.h>

#include <Directory.h>
#include <Entry.h>
#include <MessageRunner.h>
#include <Node.h>
#include <fs_attr.h>
#include <Screen.h>

#include <curl/curl.h>
#include <OS.h>
#include <image.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <dlfcn.h>
#include <math.h>
#include <string>
#include <vector>

#include "SnakeWallpaper.h"


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
	const char*	stockPath;		// Haiku's own (what runs when ours can not)
};

static const Program kPrograms[] = {
	{ "x-vnd.be-trak", "application/x-vnd.Be-TRAK", "/boot/system/apps/SnakeTracker/Tracker", "/boot/system/Tracker" },
	{ "x-vnd.be-tskb", "application/x-vnd.Be-TSKB", "/boot/system/apps/SnakeTracker/Deskbar", "/boot/system/Deskbar" },
};
static const int kProgramCount = 2;

extern char** environ;

static void DebugLog(const char* format, ...);

static volatile int32 sStop = 0;
static int32 sSupervising[kProgramCount];	// 1 while a thread keeps program i running
// 1 when ours could not be started (a Haiku update that it does not fit, say): Haiku's own runs in its place, and nothing puts
// ours back until this server runs again (at the next start-up, or after an update of SnakeTracker)
static int32 sFallback[kProgramCount];
static int32 sFallbackTold = 0;


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
// Tells the user, once, that Haiku's own Tracker and Deskbar run instead of ours.
static int32
FallbackAlertThread(void* cookie)
{
	BString* reason = (BString*)cookie;
	BString text("SnakeTracker's Tracker or Deskbar could not start (");
	text << reason->String() << ").\n\nHaiku's own Tracker and Deskbar are running instead. SnakeTracker probably needs "
		"an update for this version of Haiku; it tries its own again the next time you start the computer.";
	delete reason;
	BAlert* alert = new BAlert("", text.String(), "OK", NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
	alert->Go();
	return 0;
}


static void StartSupervising(int i);


// Ours could not start: Haiku's own takes its place (kept running the same way), and the user is told once.
static void
StartStock(int i, const char* reason)
{
	if (atomic_test_and_set(&sFallback[i], 1, 0) != 0)
		return;
	DebugLog("falling back to Haiku's own %s: %s", kPrograms[i].stockPath, reason);
	if (atomic_test_and_set(&sFallbackTold, 1, 0) == 0) {
		thread_id thread = spawn_thread(FallbackAlertThread, "snaketracker alert", B_LOW_PRIORITY, new BString(reason));
		if (thread >= 0)
			resume_thread(thread);
	}
	StartSupervising(i);
}


static int32
SuperviseThread(void* cookie)
{
	const Program* program = (const Program*)cookie;
	const int index = program - kPrograms;
	int tries = 0;

	while (!sStop) {
		const bool stock = sFallback[index] != 0;
		const char* argv[] = { stock ? program->stockPath : program->path, NULL };
		time_t started = time(NULL);
		BString failure;

		thread_id thread = load_image(1, argv, (const char**)environ);
		if (thread < 0) {
			tries++;
			failure << "it could not be loaded: " << strerror(thread);
		} else {
			resume_thread(thread);
			// (wait_for_thread() says 0 even for a program that was killed or crashed; waitpid() tells them apart)
			int status = 0;
			bool quitNormally = waitpid(thread, &status, 0) == thread && WIFEXITED(status)
				&& WEXITSTATUS(status) == 0;
			const time_t ran = time(NULL) - started;
			// (one that quits cleanly right away was probably refused, as a second copy, and counts as a failure; one that ran for
			// a while and then crashed is not a program that can't start)
			if (ran >= 20 || (quitNormally && ran >= 5))
				tries = 0;
			else
				tries++;
			if (!quitNormally)
				failure << "it stopped at once";
		}

		// three failures in a row: Haiku's own instead of ours
		if (!stock && tries >= 3) {
			StartStock(index, failure.Length() > 0 ? failure.String() : "it would not start");
			tries = 0;
		}
		if (tries > 20)
			break;
		for (int i = 0; i < 30 && !sStop; i++)
			snooze(100000);
	}
	atomic_set(&sSupervising[index], 0);
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


// The system's Tracker saves its settings file as it quits, with only the settings it knows: SnakeTracker's own lines and
// "HideDesktopIcons" are dropped, so every time it is replaced by ours (at start-up, and by a restart by name) they were lost.
// The file is kept before the system's Tracker is stopped and put back before ours starts.
static bool
TrackerSettingsPath(BPath* path)
{
	return find_directory(B_USER_SETTINGS_DIRECTORY, path) == B_OK && path->Append("Tracker/TrackerSettings") == B_OK;
}


static BString
ReadTrackerSettings()
{
	BString text;
	BPath path;
	if (!TrackerSettingsPath(&path))
		return text;
	FILE* file = fopen(path.Path(), "r");
	if (file == NULL)
		return text;
	char buffer[1024];
	size_t count;
	while ((count = fread(buffer, 1, sizeof(buffer), file)) > 0 && text.Length() < 65536)
		text.Append(buffer, count);
	fclose(file);
	return text;
}


static void
RestoreTrackerSettings(const BString& saved)
{
	BPath path;
	if (saved.Length() == 0 || !TrackerSettingsPath(&path) || ReadTrackerSettings() == saved)
		return;
	FILE* file = fopen(path.Path(), "w");
	if (file == NULL)
		return;
	fwrite(saved.String(), 1, saved.Length(), file);
	fclose(file);
}


static bool
IsTracker(const Program& program)
{
	return strcmp(program.signature, "application/x-vnd.Be-TRAK") == 0;
}


// After start-up: when the launch daemon is the one that starts ours (so nothing here keeps them running), a Tracker or Deskbar that
// is still not running half a minute on may have failed to load (an update of Haiku can change what it needs). Ours is started
// here to see; if it can not even be loaded, or dies at once, Haiku's own takes its place.
static int32
BootWatchThread(void*)
{
	for (int i = 0; i < 300 && !sStop; i++)
		snooze(100000);
	for (int i = 0; i < kProgramCount && !sStop; i++) {
		const Program& program = kPrograms[i];
		if (sFallback[i] || sSupervising[i] || be_roster->IsRunning(program.signature))
			continue;
		const char* argv[] = { program.path, NULL };
		thread_id thread = load_image(1, argv, (const char**)environ);
		if (thread < 0) {
			BString reason("it could not be loaded: ");
			reason << strerror(thread);
			StartStock(i, reason.String());
			continue;
		}
		resume_thread(thread);
		// (a program that stays up is the answer; one that quits cleanly was refused as a second copy)
		int status = 0;
		bool crashed = false;
		for (int n = 0; n < 50 && !sStop; n++) {
			if (waitpid(thread, &status, WNOHANG) == thread) {
				crashed = !(WIFEXITED(status) && WEXITSTATUS(status) == 0);
				break;
			}
			snooze(100000);
		}
		if (crashed)
			StartStock(i, "it stopped at once");
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

		const BString saved = IsTracker(program) ? ReadTrackerSettings() : BString();
		roster.Stop(program.job);
		// wait (up to ~15 s) until the system's copy is gone
		for (int n = 0; n < 30 && !sStop && OtherInstanceRunning(program); n++)
			snooze(500000);
		if (IsTracker(program))
			RestoreTrackerSettings(saved);

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
	if (sFallback[i])
		return 0;	// (Haiku's own runs because ours can not)
	team_id team = be_roster->TeamFor(program.signature);
	BString saved;
	if (team >= 0) {
		app_info info;
		if (be_roster->GetRunningAppInfo(team, &info) == B_OK) {
			BPath path(&info.ref);
			if (path.Path() != NULL && strncmp(path.Path(), kAppFolder, strlen(kAppFolder)) == 0)
				return 0;
		}
		if (IsTracker(program))
			saved = ReadTrackerSettings();
		BMessenger(NULL, team).SendMessage(B_QUIT_REQUESTED);
		for (int n = 0; n < 30 && !sStop && be_roster->IsRunning(program.signature); n++)
			snooze(100000);
		if (be_roster->IsRunning(program.signature))
			kill_team(team);
	}
	BLaunchRoster().Stop(program.job);
	for (int n = 0; n < 30 && !sStop && OtherInstanceRunning(program); n++)
		snooze(500000);
	RestoreTrackerSettings(saved);
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


// ---- the wallpaper ----
// The images come from the folders of the settings (SnakeWallpaper.h). A change is Haiku's own: the same "be:bgndimginfo"
// attribute of the Desktop folder that the Backgrounds preferences write, and the same message to Tracker to show it.

static const uint32 kMsgWallpaperTick = 'SnWT';

static bool
IsImageName(const char* name)
{
	static const char* const kExtensions[] = { ".png", ".jpg", ".jpeg", ".webp", ".bmp", ".gif", ".tga", ".tif", ".tiff" };
	const size_t length = strlen(name);
	for (size_t i = 0; i < sizeof(kExtensions) / sizeof(kExtensions[0]); i++) {
		const size_t extension = strlen(kExtensions[i]);
		if (length > extension && strcasecmp(name + length - extension, kExtensions[i]) == 0)
			return true;
	}
	return false;
}


// Every image in the folder and below it (not through links, not hidden, not too deep or too many).
static void
CollectImages(const char* folder, int depth, std::vector<std::string>& images)
{
	if (depth > 8 || images.size() >= 20000)
		return;
	BDirectory directory(folder);
	if (directory.InitCheck() != B_OK)
		return;
	BEntry entry;
	while (directory.GetNextEntry(&entry, false) == B_OK) {
		char name[B_FILE_NAME_LENGTH];
		if (entry.GetName(name) != B_OK || name[0] == '.' || entry.IsSymLink())
			continue;
		BPath path;
		if (entry.GetPath(&path) != B_OK)
			continue;
		if (entry.IsDirectory())
			CollectImages(path.Path(), depth + 1, images);
		else if (entry.IsFile() && IsImageName(name))
			images.push_back(path.Path());
	}
}


// ---- online pictures ----
// Once a day each chosen source's picture(s) of the day are fetched into the cache folder (a folder for each source, the last
// two weeks of them kept), and join the images the wallpaper is taken from. The title and the copyright stay with the file, in
// attributes (SnakeTracker:Title, SnakeTracker:Copyright).

static const uint32 kMsgWallpaperFetched = 'SnWF';
static const uint32 kMsgWallpaperDaily = 'SnWD';
static const char* const kUserAgent = "SnakeTracker/" SNAKETRACKER_VERSION " (https://github.com/ablyssx74/haiku)";
static const long kMaxPicture = 40L * 1024 * 1024;
static const int kKeepPictures = 14;

// When ~/config/settings/snaketracker_wallpaper_debug exists, what the downloads do is written to snaketracker_wallpaper.log.
static void
DebugLog(const char* format, ...)
{
	BPath flag, log;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &flag) != B_OK || flag.Append("snaketracker_wallpaper_debug") != B_OK
		|| !BEntry(flag.Path()).Exists() || find_directory(B_USER_SETTINGS_DIRECTORY, &log) != B_OK
		|| log.Append("snaketracker_wallpaper.log") != B_OK)
		return;
	FILE* file = fopen(log.Path(), "a");
	if (file == NULL)
		return;
	va_list arguments;
	va_start(arguments, format);
	vfprintf(file, format, arguments);
	va_end(arguments);
	fputc('\n', file);
	fclose(file);
}


static size_t
AppendToString(char* data, size_t size, size_t count, void* cookie)
{
	std::string* text = (std::string*)cookie;
	if (text->size() + size * count > (size_t)kMaxPicture)
		return 0;
	text->append(data, size * count);
	return size * count;
}


static bool
HttpGetOnce(const std::string& url, std::string* body, long timeout, const char* header)
{
	CURL* curl = curl_easy_init();
	if (curl == NULL)
		return false;
	struct curl_slist* headers = NULL;
	if (header != NULL) {
		headers = curl_slist_append(headers, header);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}
	body->clear();
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, AppendToString);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, body);
	// (the Art Institute's server turns away a User-Agent that has an address in it)
	curl_easy_setopt(curl, CURLOPT_USERAGENT,
		url.find("artic.edu") != std::string::npos ? "SnakeTracker/" SNAKETRACKER_VERSION : kUserAgent);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
	const CURLcode result = curl_easy_perform(curl);
	long status = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
	DebugLog("GET %.120s -> curl %d, status %ld, %zu bytes", url.c_str(), (int)result, status, body->size());
	if (status != 200 && !body->empty())
		DebugLog("  answer: %.300s", body->c_str());
	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);
	return result == CURLE_OK && status == 200 && !body->empty();
}


// attempts: some services answer with an error or not at all every now and then, and do fine a moment later
static bool
HttpGet(const std::string& url, std::string* body, int attempts = 1, long timeout = 120, const char* header = NULL)
{
	for (int i = 0; i < attempts; i++) {
		if (i > 0)
			snooze(3000000);
		if (HttpGetOnce(url, body, timeout, header))
			return true;
	}
	return false;
}


static void
AppendUtf8(std::string* out, unsigned code)
{
	if (code < 0x80)
		out->push_back((char)code);
	else if (code < 0x800) {
		out->push_back((char)(0xC0 | (code >> 6)));
		out->push_back((char)(0x80 | (code & 0x3F)));
	} else {
		out->push_back((char)(0xE0 | (code >> 12)));
		out->push_back((char)(0x80 | ((code >> 6) & 0x3F)));
		out->push_back((char)(0x80 | (code & 0x3F)));
	}
}


// The string value of "key": "..." found at or after from (the value is decoded); end: just after it.
static bool
JsonString(const std::string& text, const char* key, size_t from, std::string* value, size_t* end = NULL)
{
	const std::string quoted = std::string("\"") + key + "\"";
	size_t at = text.find(quoted, from);
	while (at != std::string::npos) {
		size_t i = at + quoted.size();
		while (i < text.size() && (text[i] == ' ' || text[i] == '\n' || text[i] == '\t'))
			i++;
		if (i < text.size() && text[i] == ':') {
			i++;
			while (i < text.size() && (text[i] == ' ' || text[i] == '\n' || text[i] == '\t'))
				i++;
			if (i < text.size() && text[i] == '"') {
				i++;
				value->clear();
				while (i < text.size() && text[i] != '"') {
					if (text[i] == '\\' && i + 1 < text.size()) {
						i++;
						switch (text[i]) {
							case 'n': value->push_back('\n'); break;
							case 't': value->push_back('\t'); break;
							case 'u':
								if (i + 4 < text.size()) {
									AppendUtf8(value, (unsigned)strtoul(text.substr(i + 1, 4).c_str(), NULL, 16));
									i += 4;
								}
								break;
							default: value->push_back(text[i]); break;
						}
					} else
						value->push_back(text[i]);
					i++;
				}
				if (end != NULL)
					*end = i;
				return true;
			}
		}
		at = text.find(quoted, at + 1);
	}
	return false;
}


struct Picture {
	std::string id;			// the name of the file, without its extension
	std::string url;
	std::string fallback;	// another address, if the first one is refused
	std::string notify;		// an address to ask once the picture is downloaded (Unsplash wants to be told)
	std::string title;
	std::string copyright;
};


static std::string
DateString(time_t when, const char* format)
{
	char text[32];
	struct tm local;
	localtime_r(&when, &local);
	strftime(text, sizeof(text), format, &local);
	return text;
}


static bool
WideScreen()
{
	return BScreen().Frame().Width() > 2200;
}


// Bing's pictures of the last few days.
static void
ListBing(std::vector<Picture>* pictures)
{
	std::string json;
	if (!HttpGet("https://www.bing.com/HPImageArchive.aspx?format=js&idx=0&n=4&mkt=en-US", &json))
		return;
	size_t at = 0;
	std::string date, base, copyright, title;
	while (JsonString(json, "startdate", at, &date, &at)) {
		if (!JsonString(json, "urlbase", at, &base, &at))
			break;
		JsonString(json, "copyright", at, &copyright, &at);
		JsonString(json, "title", at, &title, &at);
		Picture picture;
		picture.id = "bing-" + date;
		picture.url = "https://www.bing.com" + base + (WideScreen() ? "_UHD.jpg" : "_1920x1080.jpg");
		picture.title = title;
		picture.copyright = copyright;
		pictures->push_back(picture);
	}
}


// Wikimedia Commons' picture of the day, for the last few days.
static void
ListWikimedia(std::vector<Picture>* pictures)
{
	for (int back = 0; back < 3; back++) {
		const std::string day = DateString(time(NULL) - back * 86400, "%Y/%m/%d");
		std::string json;
		if (!HttpGet("https://api.wikimedia.org/feed/v1/wikipedia/en/featured/" + day, &json))
			continue;
		const size_t image = json.find("\"image\":{");
		if (image == std::string::npos)
			continue;
		std::string title, thumb, original;
		JsonString(json, "title", image, &title);
		// (the picture's object holds its title, a thumbnail and the original, each with a "source")
		const size_t thumbnail = json.find("\"thumbnail\"", image);
		size_t afterThumb = image;
		if (thumbnail != std::string::npos && JsonString(json, "source", thumbnail, &thumb, &afterThumb))
			JsonString(json, "source", afterThumb, &original);
		if (thumb.empty() && original.empty())
			continue;
		Picture picture;
		std::string date = day;
		for (size_t i = 0; i < date.size(); i++) {
			if (date[i] == '/')
				date[i] = '-';
		}
		picture.id = "wikimedia-" + date;
		// (the thumbnail's address names its width: ask for the width of the screen)
		const std::string width = WideScreen() ? "/3840px-" : "/1920px-";
		const size_t px = thumb.find("/960px-");
		if (px != std::string::npos)
			picture.url = thumb.substr(0, px) + width + thumb.substr(px + 7);
		else
			picture.url = thumb;
		picture.fallback = original;
		picture.title = title.compare(0, 5, "File:") == 0 ? title.substr(5) : title;
		picture.copyright = "Wikimedia Commons, Picture of the Day";
		pictures->push_back(picture);
	}
}


// Flickr's "interesting" pictures of the day (needs the user's key).
static void
ListFlickr(const BMessage& settings, std::vector<Picture>* pictures)
{
	const std::string key = settings.GetString("flickrkey", "");
	if (key.empty())
		return;
	std::string json;
	if (!HttpGet("https://www.flickr.com/services/rest/?method=flickr.interestingness.getList&per_page=6&format=json"
			"&nojsoncallback=1&extras=owner_name,url_l,url_h,url_k&api_key=" + key, &json, 2, 30))
		return;
	const std::string today = DateString(time(NULL), "%Y%m%d");
	int count = 0;
	// (each picture's object starts with its "id")
	size_t at = json.find("{\"id\":\"");
	while (at != std::string::npos && count < 4) {
		const size_t next = json.find("{\"id\":\"", at + 1);
		const std::string chunk = json.substr(at, next == std::string::npos ? std::string::npos : next - at);
		at = next;
		std::string id, title, owner, url;
		if (!JsonString(chunk, "id", 0, &id))
			continue;
		if (!JsonString(chunk, "url_k", 0, &url) && !JsonString(chunk, "url_h", 0, &url)
			&& !JsonString(chunk, "url_l", 0, &url))
			continue;
		JsonString(chunk, "title", 0, &title);
		JsonString(chunk, "ownername", 0, &owner);
		Picture picture;
		picture.id = "flickr-" + today + "-" + id;
		picture.url = url;
		picture.title = title.empty() ? "Flickr" : title;
		picture.copyright = "Photo by " + (owner.empty() ? std::string("a Flickr member") : owner) + " on Flickr";
		pictures->push_back(picture);
		count++;
	}
}


// A few random landscape photos from Unsplash (needs the user's key). Its rules ask to say whose photo it is, and to tell it
// when one is downloaded.
static void
ListUnsplash(const BMessage& settings, std::vector<Picture>* pictures)
{
	const std::string key = settings.GetString("unsplashkey", "");
	if (key.empty())
		return;
	std::string json;
	if (!HttpGet("https://api.unsplash.com/photos/random?orientation=landscape&count=3&client_id=" + key, &json, 2, 30))
		return;
	const std::string today = DateString(time(NULL), "%Y%m%d");
	size_t at = json.find("\"urls\":{");
	int number = 1;
	while (at != std::string::npos) {
		const size_t next = json.find("\"urls\":{", at + 1);
		const std::string chunk = json.substr(at, next == std::string::npos ? std::string::npos : next - at);
		at = next;
		std::string raw, name;
		if (!JsonString(chunk, "raw", 0, &raw))
			continue;
		const size_t user = chunk.find("\"user\":{");		// (the photographer's, not a sponsor's)
		JsonString(chunk, "name", user == std::string::npos ? 0 : user, &name);
		std::string download;
		JsonString(chunk, "download_location", 0, &download);
		Picture picture;
		char suffix[16];
		snprintf(suffix, sizeof(suffix), "-%d", number++);
		picture.id = "unsplash-" + today + suffix;
		picture.url = raw + (raw.find('?') == std::string::npos ? "?" : "&") + "w=" + (WideScreen() ? "3840" : "1920")
			+ "&fit=max&q=80&fm=jpg";
		picture.notify = download.empty() ? "" : download + (download.find('?') == std::string::npos ? "?" : "&")
			+ "client_id=" + key;
		picture.title = "Unsplash";
		picture.copyright = "Photo by " + (name.empty() ? std::string("an Unsplash photographer") : name) + " on Unsplash";
		pictures->push_back(picture);
	}
}


// A whole number: the value of "key": 123 found at or after from (-1 when there is none).
static long
JsonNumber(const std::string& text, const char* key, size_t from = 0)
{
	const std::string quoted = std::string("\"") + key + "\":";
	size_t at = text.find(quoted, from);
	if (at == std::string::npos)
		return -1;
	at += quoted.size();
	while (at < text.size() && text[at] == ' ')
		at++;
	if (at >= text.size() || text[at] < '0' || text[at] > '9')
		return -1;
	return strtol(text.c_str() + at, NULL, 10);
}


// The pieces of a list of objects that each begin with marker (the first of each object's keys).
static std::vector<std::string>
Objects(const std::string& json, const char* marker)
{
	std::vector<std::string> objects;
	size_t at = json.find(marker);
	while (at != std::string::npos) {
		const size_t next = json.find(marker, at + 1);
		objects.push_back(json.substr(at, next == std::string::npos ? std::string::npos : next - at));
		at = next;
	}
	return objects;
}


// Wallhaven's most popular wallpapers of the day (safe for work; it needs no key).
static void
ListWallhaven(std::vector<Picture>* pictures)
{
	std::string json;
	if (!HttpGet(std::string("https://wallhaven.cc/api/v1/search?categories=100&purity=100&sorting=toplist&topRange=1d"
			"&ratios=16x9,16x10&atleast=") + (WideScreen() ? "3840x2160" : "1920x1080"), &json, 2, 30))
		return;
	const std::string today = DateString(time(NULL), "%Y%m%d");
	const std::vector<std::string> objects = Objects(json, "{\"id\":\"");
	int count = 0;
	for (size_t i = 0; i < objects.size() && count < 4; i++) {
		std::string id, path, page;
		if (!JsonString(objects[i], "id", 0, &id) || !JsonString(objects[i], "path", 0, &path))
			continue;
		JsonString(objects[i], "url", 0, &page);
		Picture picture;
		picture.id = "wallhaven-" + today + "-" + id;
		picture.url = path;
		picture.title = "Wallhaven " + id;
		picture.copyright = "wallhaven.cc" + (page.empty() ? std::string() : " - " + page);
		pictures->push_back(picture);
		count++;
	}
}


// Random photos from Picsum (they come from Unsplash photographers; it needs no key).
static void
ListPicsum(std::vector<Picture>* pictures)
{
	std::string json;
	char page[64];
	snprintf(page, sizeof(page), "page=%d&limit=40", 1 + rand() % 20);
	if (!HttpGet(std::string("https://picsum.photos/v2/list?") + page, &json, 2, 30))
		return;
	const std::string today = DateString(time(NULL), "%Y%m%d");
	const std::vector<std::string> objects = Objects(json, "{\"id\":\"");
	std::vector<size_t> good;
	for (size_t i = 0; i < objects.size(); i++) {
		const long width = JsonNumber(objects[i], "width"), height = JsonNumber(objects[i], "height");
		if (width >= 3000 && height > 0 && width > height * 13 / 10)
			good.push_back(i);
	}
	for (int count = 0; count < 3 && !good.empty(); count++) {
		const size_t pick = (size_t)(rand() % good.size());
		const std::string& object = objects[good[pick]];
		good.erase(good.begin() + pick);
		std::string id, author;
		if (!JsonString(object, "id", 0, &id))
			continue;
		JsonString(object, "author", 0, &author);
		Picture picture;
		picture.id = "picsum-" + today + "-" + id;
		picture.url = "https://picsum.photos/id/" + id + (WideScreen() ? "/3840/2160.jpg" : "/1920/1080.jpg");
		picture.title = "Picsum Photos";
		picture.copyright = "Photo by " + (author.empty() ? std::string("a photographer") : author) + " (Picsum Photos)";
		pictures->push_back(picture);
	}
}


// Paintings and drawings of the Art Institute of Chicago that are in the public domain, the wide ones.
static void
ListArtic(std::vector<Picture>* pictures)
{
	std::string json;
	char page[96];
	snprintf(page, sizeof(page), "&limit=40&page=%d", 1 + rand() % 20);	// (it answers only for the first thousand)
	if (!HttpGet(std::string("https://api.artic.edu/api/v1/artworks/search?q=landscape&query%5Bterm%5D%5Bis_public_domain%5D=true"
			"&fields=id,title,artist_display,image_id,thumbnail") + page, &json, 2, 30))
		return;
	const std::string today = DateString(time(NULL), "%Y%m%d");
	const std::vector<std::string> objects = Objects(json, "{\"_score\":");
	std::vector<size_t> good;
	for (size_t i = 0; i < objects.size(); i++) {
		const long width = JsonNumber(objects[i], "width"), height = JsonNumber(objects[i], "height");
		if (width >= 2500 && height > 0 && width > height * 13 / 10)
			good.push_back(i);
	}
	for (int count = 0; count < 3 && !good.empty(); count++) {
		const size_t pick = (size_t)(rand() % good.size());
		const std::string& object = objects[good[pick]];
		good.erase(good.begin() + pick);
		std::string image, title, artist;
		if (!JsonString(object, "image_id", 0, &image))
			continue;
		JsonString(object, "title", 0, &title);
		JsonString(object, "artist_display", 0, &artist);
		const size_t newline = artist.find('\n');
		if (newline != std::string::npos)
			artist = artist.substr(0, newline);
		Picture picture;
		picture.id = "artic-" + today + "-" + image.substr(0, 8);
		picture.url = "https://www.artic.edu/iiif/2/" + image + "/full/1686,/0/default.jpg";
		picture.title = title.empty() ? "Art Institute of Chicago" : title;
		picture.copyright = (artist.empty() ? std::string("Unknown artist") : artist) + " - Art Institute of Chicago (public domain)";
		pictures->push_back(picture);
	}
}


// Pexels' curated photos (needs the user's key).
static void
ListPexels(const BMessage& settings, std::vector<Picture>* pictures)
{
	const std::string key = settings.GetString("pexelskey", "");
	if (key.empty())
		return;
	std::string json;
	const std::string header = "Authorization: " + key;
	if (!HttpGet("https://api.pexels.com/v1/curated?per_page=12", &json, 2, 30, header.c_str()))
		return;
	const std::string today = DateString(time(NULL), "%Y%m%d");
	const std::vector<std::string> objects = Objects(json, "{\"id\":");
	int count = 0;
	for (size_t i = 0; i < objects.size() && count < 4; i++) {
		const long width = JsonNumber(objects[i], "width"), height = JsonNumber(objects[i], "height"), id = JsonNumber(objects[i], "id");
		if (id < 0 || width < 1920 || height <= 0 || width <= height * 13 / 10)
			continue;
		std::string original, photographer;
		if (!JsonString(objects[i], "original", 0, &original))
			continue;
		JsonString(objects[i], "photographer", 0, &photographer);
		char number[32];
		snprintf(number, sizeof(number), "%ld", id);
		Picture picture;
		picture.id = "pexels-" + today + "-" + number;
		picture.url = original + (original.find('?') == std::string::npos ? "?" : "&") + "auto=compress&cs=tinysrgb&w="
			+ (WideScreen() ? "3840" : "1920");
		picture.title = "Pexels";
		picture.copyright = "Photo by " + (photographer.empty() ? std::string("a Pexels photographer") : photographer) + " on Pexels";
		pictures->push_back(picture);
		count++;
	}
}


// Pixabay's editor's choice photos (needs the user's key).
static void
ListPixabay(const BMessage& settings, std::vector<Picture>* pictures)
{
	const std::string key = settings.GetString("pixabaykey", "");
	if (key.empty())
		return;
	std::string json;
	if (!HttpGet("https://pixabay.com/api/?image_type=photo&orientation=horizontal&min_width=1920&editors_choice=true"
			"&per_page=8&key=" + key, &json, 2, 30))
		return;
	const std::string today = DateString(time(NULL), "%Y%m%d");
	const std::vector<std::string> objects = Objects(json, "{\"id\":");
	int count = 0;
	for (size_t i = 0; i < objects.size() && count < 4; i++) {
		const long id = JsonNumber(objects[i], "id");
		std::string url, user, tags;
		if (id < 0 || (!JsonString(objects[i], "fullHDURL", 0, &url) && !JsonString(objects[i], "largeImageURL", 0, &url)))
			continue;
		JsonString(objects[i], "user", 0, &user);
		JsonString(objects[i], "tags", 0, &tags);
		char number[32];
		snprintf(number, sizeof(number), "%ld", id);
		Picture picture;
		picture.id = "pixabay-" + today + "-" + number;
		picture.url = url;
		picture.title = tags.empty() ? "Pixabay" : tags;
		picture.copyright = "Photo by " + (user.empty() ? std::string("a Pixabay contributor") : user) + " on Pixabay";
		pictures->push_back(picture);
		count++;
	}
}


static std::string
Extension(const std::string& url)
{
	size_t end = url.find_first_of("?&#");
	const std::string path = url.substr(0, end);
	const size_t dot = path.rfind('.');
	if (dot == std::string::npos || path.size() - dot > 6)
		return ".jpg";
	return path.substr(dot);
}


static bool
LooksLikeImage(const std::string& data)
{
	if (data.size() < 30000)
		return false;
	const unsigned char* b = (const unsigned char*)data.data();
	return (b[0] == 0xFF && b[1] == 0xD8)											// JPEG
		|| (b[0] == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G')				// PNG
		|| (b[0] == 'R' && b[1] == 'I' && b[2] == 'F' && b[3] == 'F')				// WebP
		|| (b[0] == 'G' && b[1] == 'I' && b[2] == 'F');								// GIF
}


// Takes the pictures of a source that are not in its folder yet; the oldest beyond kKeepPictures go.
static void
FetchSource(const BMessage& settings, const char* source)
{
	std::vector<Picture> pictures;
	if (strcmp(source, "bing") == 0)
		ListBing(&pictures);
	else if (strcmp(source, "wikimedia") == 0)
		ListWikimedia(&pictures);
	else if (strcmp(source, "wallhaven") == 0)
		ListWallhaven(&pictures);
	else if (strcmp(source, "picsum") == 0)
		ListPicsum(&pictures);
	else if (strcmp(source, "artic") == 0)
		ListArtic(&pictures);
	else if (strcmp(source, "pexels") == 0)
		ListPexels(settings, &pictures);
	else if (strcmp(source, "pixabay") == 0)
		ListPixabay(settings, &pictures);
	else if (strcmp(source, "flickr") == 0)
		ListFlickr(settings, &pictures);
	else if (strcmp(source, "unsplash") == 0)
		ListUnsplash(settings, &pictures);
	DebugLog("%s: %zu pictures listed", source, pictures.size());
	BPath folder;
	if (!WallpaperCacheDir(source, &folder) || create_directory(folder.Path(), 0755) != B_OK)
		return;
	for (size_t i = 0; i < pictures.size(); i++) {
		const Picture& picture = pictures[i];
		const std::string name = picture.id + Extension(picture.url);
		BPath target(folder.Path(), name.c_str());
		if (BEntry(target.Path()).Exists())
			continue;
		std::string data;
		if (!HttpGet(picture.url, &data) && (picture.fallback.empty() || !HttpGet(picture.fallback, &data)))
			continue;
		// (a picture, and one big enough to be a wallpaper: a service that has moved may answer with its logo or a web page)
		if (!LooksLikeImage(data) && !picture.fallback.empty()) {
			if (!HttpGet(picture.fallback, &data))
				continue;
		}
		if (!LooksLikeImage(data))
			continue;
		// (written under another name, so that a half-written picture is never taken for one)
		BPath partial(folder.Path(), (name + ".part").c_str());
		BFile file(partial.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		if (file.InitCheck() != B_OK || file.Write(data.data(), data.size()) != (ssize_t)data.size())
			continue;
		file.WriteAttr("SnakeTracker:Title", B_STRING_TYPE, 0, picture.title.c_str(), picture.title.size() + 1);
		file.WriteAttr("SnakeTracker:Copyright", B_STRING_TYPE, 0, picture.copyright.c_str(), picture.copyright.size() + 1);
		file.Unset();
		BEntry(partial.Path()).Rename(name.c_str(), true);
		if (!picture.notify.empty()) {
			std::string ignored;
			HttpGet(picture.notify, &ignored, 1, 20);
		}
	}

	// the folder keeps the newest ones (the names carry the date)
	std::vector<std::string> names;
	BDirectory directory(folder.Path());
	BEntry entry;
	while (directory.GetNextEntry(&entry, false) == B_OK) {
		char name[B_FILE_NAME_LENGTH];
		if (entry.GetName(name) == B_OK && IsImageName(name))
			names.push_back(name);
	}
	std::sort(names.begin(), names.end());
	for (size_t i = 0; i + kKeepPictures < names.size(); i++) {
		BPath old(folder.Path(), names[i].c_str());
		BEntry(old.Path()).Remove();
	}
	// (when it was done, so that it is not done again today)
	BPath marker(folder.Path(), ".fetched");
	BFile done(marker.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	const std::string today = DateString(time(NULL), "%Y-%m-%d");
	if (!pictures.empty())
		done.Write(today.data(), today.size());
}


// Does a source still have to be fetched today?
static bool
NeedsFetch(const char* source)
{
	BPath folder, marker;
	if (!WallpaperCacheDir(source, &folder))
		return false;
	marker.SetTo(folder.Path(), ".fetched");
	BFile file(marker.Path(), B_READ_ONLY);
	char buffer[16] = "";
	if (file.InitCheck() == B_OK)
		file.Read(buffer, sizeof(buffer) - 1);
	return DateString(time(NULL), "%Y-%m-%d") != buffer;
}


struct FetchJob {
	BMessage settings;			// (the keys)
	std::vector<std::string> sources;
	bool apply;
};


static status_t
FetchThread(void* cookie)
{
	FetchJob* job = (FetchJob*)cookie;
	for (size_t i = 0; i < job->sources.size(); i++)
		FetchSource(job->settings, job->sources[i].c_str());
	BMessage done(kMsgWallpaperFetched);
	done.AddBool("apply", job->apply);
	delete job;
	be_app_messenger.SendMessage(&done);
	return B_OK;
}


// What the Desktop folder's background setting says now (the placement, the offset, the outline of the icon labels, the pictures).
struct CurrentBackground {
	int32 mode;
	BPoint offset;
	bool outline;
	std::vector<std::string> showing;

	CurrentBackground() : mode(2), offset(0, 0), outline(true) {}
};


static void
ReadCurrentBackground(BNode& node, CurrentBackground* current)
{
	attr_info info;
	if (node.GetAttrInfo("be:bgndimginfo", &info) != B_OK || info.size <= 0)
		return;
	char* buffer = new char[info.size];
	BMessage saved;
	if (node.ReadAttr("be:bgndimginfo", info.type, 0, buffer, (size_t)info.size) == info.size
		&& saved.Unflatten(buffer) == B_OK) {
		saved.FindInt32("be:bgndimginfomode", &current->mode);
		saved.FindPoint("be:bgndimginfooffset", &current->offset);
		saved.FindBool("be:bgndimginfoerasetext", &current->outline);
		const char* path;
		for (int32 i = 0; saved.FindString("be:bgndimginfopath", i, &path) == B_OK; i++)
			current->showing.push_back(path);
	}
	delete[] buffer;
}


// Writes the pictures (one for all workspaces, or one for each) as the Desktop folder's background setting, and has Tracker show
// them. mode: the placement (-1: as it is).
static bool
WriteBackgrounds(BNode& node, const CurrentBackground& current, const std::vector<std::string>& paths, bool perWorkspace,
	int32 mode)
{
	BMessage container;
	for (size_t k = 0; k < paths.size(); k++) {
		container.AddBool("be:bgndimginfoerasetext", current.outline);
		container.AddString("be:bgndimginfopath", paths[k].c_str());
		container.AddInt32("be:bgndimginfoworkspaces", perWorkspace ? (int32)(1u << k) : (int32)B_ALL_WORKSPACES);
		container.AddPoint("be:bgndimginfooffset", current.offset);
		container.AddInt32("be:bgndimginfomode", mode >= 0 ? mode : current.mode);
		container.AddInt32("be:bgndimginfoset", 0);
	}
	const ssize_t size = container.FlattenedSize();
	if (size <= 0)
		return false;
	char* buffer = new char[size];
	bool done = false;
	if (container.Flatten(buffer, size) == B_OK && node.WriteAttr("be:bgndimginfo", B_MESSAGE_TYPE, 0, buffer, size) == size) {
		BMessenger("application/x-vnd.Be-TRAK").SendMessage('Tbgr');
		done = true;
	}
	delete[] buffer;
	return done;
}


// Puts the chosen images into the Desktop folder's background setting (one for all workspaces, or one for each) and has Tracker
// show them. The placement and the text outline stay as they are unless the settings say otherwise.
static void
ChangeWallpaper(const BMessage& settings, uint32* cursor)
{
	std::vector<std::string> images;
	const char* folder;
	for (int32 i = 0; settings.FindString("folder", i, &folder) == B_OK; i++)
		CollectImages(folder, 0, images);
	for (int32 i = 0; settings.FindString("online", i, &folder) == B_OK; i++) {
		BPath cache;
		if (WallpaperCacheDir(folder, &cache))
			CollectImages(cache.Path(), 0, images);
	}
	if (images.empty())
		return;
	std::sort(images.begin(), images.end());

	const bool perWorkspace = settings.GetBool("perworkspace", false);
	const bool random = settings.GetBool("random", true);
	const int32 count = perWorkspace ? count_workspaces() : 1;

	BPath desktop;
	if (find_directory(B_DESKTOP_DIRECTORY, &desktop) != B_OK)
		return;
	BNode node(desktop.Path());
	if (node.InitCheck() != B_OK)
		return;
	CurrentBackground current;
	ReadCurrentBackground(node, &current);

	std::vector<std::string> chosen;
	for (int32 k = 0; k < count; k++) {
		size_t pick;
		if (random) {
			pick = (size_t)(rand() % images.size());
			// (not one that shows now, or one that was just chosen for another workspace)
			for (int tries = 0; tries < 8 && images.size() > 1; tries++) {
				const bool seen = std::find(chosen.begin(), chosen.end(), images[pick]) != chosen.end()
					|| std::find(current.showing.begin(), current.showing.end(), images[pick]) != current.showing.end();
				if (!seen)
					break;
				pick = (size_t)(rand() % images.size());
			}
		} else
			pick = (size_t)((*cursor)++ % images.size());
		chosen.push_back(images[pick]);
	}
	WriteBackgrounds(node, current, chosen, perWorkspace, settings.GetInt32("mode", -1));
}


// ---- an SVG wallpaper in the accent colour ----
// The SVG is a template: {accent}, {light}, {lighter}, {dark}, {darker}, {analog1}, {analog2} and {complement} in it are
// colours made from the accent colour (the one Tracker uses, from the SnakeTracker preferences or Haiku's selection colour).
// It is drawn at the size of the screen by librsvg (loaded when it is needed: the package is not required) into a PNG, which is
// the wallpaper; when the accent changes it is drawn again.

static const uint32 kMsgWallpaperColors = 'SnWC';

// ---- an update of the package ----
// When the package is installed again (a newer version), the programs that run keep the old files until they are started again.
// The server notices that its own file has changed, and asks whether Tracker and the Deskbar (and the server itself) may be
// restarted now. ~/config/settings/snaketracker_no_update_ask turns the question off.
static const uint32 kMsgCheckUpdate = 'SnCU';
static const uint32 kMsgUpdateAnswer = 'SnUA';
static const uint32 kMsgRestartServer = 'SnRS';
static const char* const kServerFile = "/boot/system/servers/snaketracker_server";

struct FileId {
	dev_t device;
	ino_t inode;
	time_t modified;
	off_t size;

	bool operator==(const FileId& other) const
	{
		return device == other.device && inode == other.inode && modified == other.modified && size == other.size;
	}
};


static bool
ReadFileId(FileId* id)
{
	struct stat st;
	if (stat(kServerFile, &st) != 0)
		return false;
	id->device = st.st_dev;
	id->inode = st.st_ino;
	id->modified = st.st_mtime;
	id->size = st.st_size;
	return true;
}


static bool
UpdateQuestionOff()
{
	BPath path;
	return find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK && path.Append("snaketracker_no_update_ask") == B_OK
		&& BEntry(path.Path()).Exists();
}

static const uint32 kAccentQuery = 'SnkA';			// (to Tracker: the accent colour it shows)

struct SvgApi {
	bool tried;
	bool ok;
	void* (*newFromData)(const unsigned char*, size_t, void**);
	int (*renderDocument)(void*, void*, const double*, void**);
	void (*unref)(void*);
	void* (*surfaceCreate)(int, int, int);
	void* (*create)(void*);
	void (*destroy)(void*);
	void (*surfaceDestroy)(void*);
	int (*writePng)(void*, const char*);
};

static SvgApi sSvg = { false, false, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };


static bool
LoadSvgApi()
{
	if (sSvg.tried)
		return sSvg.ok;
	sSvg.tried = true;
	void* rsvg = dlopen("librsvg-2.so.2", RTLD_NOW | RTLD_GLOBAL);
	if (rsvg == NULL)
		rsvg = dlopen("librsvg-2.so", RTLD_NOW | RTLD_GLOBAL);
	void* cairo = dlopen("libcairo.so.2", RTLD_NOW | RTLD_GLOBAL);
	void* gobject = dlopen("libgobject-2.0.so.0", RTLD_NOW | RTLD_GLOBAL);
	if (rsvg == NULL || cairo == NULL || gobject == NULL)
		return false;
	sSvg.newFromData = (void* (*)(const unsigned char*, size_t, void**))dlsym(rsvg, "rsvg_handle_new_from_data");
	sSvg.renderDocument = (int (*)(void*, void*, const double*, void**))dlsym(rsvg, "rsvg_handle_render_document");
	sSvg.unref = (void (*)(void*))dlsym(gobject, "g_object_unref");
	sSvg.surfaceCreate = (void* (*)(int, int, int))dlsym(cairo, "cairo_image_surface_create");
	sSvg.create = (void* (*)(void*))dlsym(cairo, "cairo_create");
	sSvg.destroy = (void (*)(void*))dlsym(cairo, "cairo_destroy");
	sSvg.surfaceDestroy = (void (*)(void*))dlsym(cairo, "cairo_surface_destroy");
	sSvg.writePng = (int (*)(void*, const char*))dlsym(cairo, "cairo_surface_write_to_png");
	sSvg.ok = sSvg.newFromData != NULL && sSvg.renderDocument != NULL && sSvg.unref != NULL && sSvg.surfaceCreate != NULL
		&& sSvg.create != NULL && sSvg.destroy != NULL && sSvg.surfaceDestroy != NULL && sSvg.writePng != NULL;
	return sSvg.ok;
}


struct Rgb {
	double r, g, b;		// 0..1
};


static Rgb
MixRgb(const Rgb& a, const Rgb& b, double t)
{
	Rgb c = { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
	return c;
}


// The color with its hue turned by degrees (and its saturation scaled).
static Rgb
TurnHue(const Rgb& c, double degrees, double saturation)
{
	const double mx = std::max(c.r, std::max(c.g, c.b)), mn = std::min(c.r, std::min(c.g, c.b)), d = mx - mn;
	double h = 0;
	if (d > 0) {
		if (mx == c.r)
			h = fmod((c.g - c.b) / d, 6.0);
		else if (mx == c.g)
			h = (c.b - c.r) / d + 2;
		else
			h = (c.r - c.g) / d + 4;
		h *= 60;
		if (h < 0)
			h += 360;
	}
	double s = mx > 0 ? d / mx : 0;
	const double v = mx;
	h = fmod(h + degrees + 360, 360);
	s = std::min(1.0, s * saturation);
	const double chroma = v * s, x = chroma * (1 - fabs(fmod(h / 60, 2.0) - 1)), m = v - chroma;
	double r = 0, g = 0, b = 0;
	if (h < 60) { r = chroma; g = x; }
	else if (h < 120) { r = x; g = chroma; }
	else if (h < 180) { g = chroma; b = x; }
	else if (h < 240) { g = x; b = chroma; }
	else if (h < 300) { r = x; b = chroma; }
	else { r = chroma; b = x; }
	Rgb c2 = { r + m, g + m, b + m };
	return c2;
}


static std::string
HexColor(const Rgb& c)
{
	char text[16];
	snprintf(text, sizeof(text), "#%02x%02x%02x", (int)(std::max(0.0, std::min(1.0, c.r)) * 255 + 0.5),
		(int)(std::max(0.0, std::min(1.0, c.g)) * 255 + 0.5), (int)(std::max(0.0, std::min(1.0, c.b)) * 255 + 0.5));
	return text;
}


static void
ReplaceAll(std::string* text, const std::string& token, const std::string& value)
{
	size_t at = 0;
	while ((at = text->find(token, at)) != std::string::npos) {
		text->replace(at, token.size(), value);
		at += value.size();
	}
}


static std::string
ApplyAccent(std::string svg, const rgb_color& accent)
{
	const Rgb a = { accent.red / 255.0, accent.green / 255.0, accent.blue / 255.0 };
	const Rgb white = { 1, 1, 1 }, black = { 0, 0, 0 };
	ReplaceAll(&svg, "{accent}", HexColor(a));
	ReplaceAll(&svg, "{lighter}", HexColor(MixRgb(a, white, 0.65)));
	ReplaceAll(&svg, "{light}", HexColor(MixRgb(a, white, 0.35)));
	ReplaceAll(&svg, "{darker}", HexColor(MixRgb(a, black, 0.75)));
	ReplaceAll(&svg, "{dark}", HexColor(MixRgb(a, black, 0.4)));
	ReplaceAll(&svg, "{analog1}", HexColor(TurnHue(a, 30, 1.0)));
	ReplaceAll(&svg, "{analog2}", HexColor(TurnHue(a, -30, 1.0)));
	ReplaceAll(&svg, "{complement}", HexColor(TurnHue(a, 180, 0.8)));
	return svg;
}


// The templates that come with SnakeTracker (16:9, they fill any screen).
static const char* const kSvgWaves = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 1920 1080' preserveAspectRatio='xMidYMid slice'>"
	"<defs><linearGradient id='bg' x1='0' y1='0' x2='0' y2='1'><stop offset='0' stop-color='{dark}'/><stop offset='1' stop-color='{darker}'/></linearGradient></defs>"
	"<rect width='1920' height='1080' fill='url(#bg)'/>"
	"<path d='M0 640 C320 540 640 760 960 660 S1600 560 1920 680 L1920 1080 L0 1080 Z' fill='{analog2}' fill-opacity='0.35'/>"
	"<path d='M0 720 C300 640 700 840 1040 740 S1640 660 1920 760 L1920 1080 L0 1080 Z' fill='{accent}' fill-opacity='0.55'/>"
	"<path d='M0 820 C360 760 720 920 1100 840 S1660 780 1920 850 L1920 1080 L0 1080 Z' fill='{light}' fill-opacity='0.55'/>"
	"<path d='M0 930 C400 880 800 990 1200 930 S1700 890 1920 940 L1920 1080 L0 1080 Z' fill='{lighter}' fill-opacity='0.5'/>"
	"</svg>";

static const char* const kSvgBubbles = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 1920 1080' preserveAspectRatio='xMidYMid slice'>"
	"<defs><radialGradient id='bg' cx='0.3' cy='0.3' r='1'><stop offset='0' stop-color='{accent}'/><stop offset='1' stop-color='{darker}'/></radialGradient></defs>"
	"<rect width='1920' height='1080' fill='url(#bg)'/>"
	"<circle cx='420' cy='300' r='260' fill='{light}' fill-opacity='0.22'/>"
	"<circle cx='1480' cy='230' r='170' fill='{analog1}' fill-opacity='0.30'/>"
	"<circle cx='1080' cy='700' r='360' fill='{lighter}' fill-opacity='0.14'/>"
	"<circle cx='300' cy='860' r='190' fill='{analog2}' fill-opacity='0.30'/>"
	"<circle cx='1640' cy='880' r='240' fill='{complement}' fill-opacity='0.18'/>"
	"<circle cx='760' cy='160' r='90' fill='{lighter}' fill-opacity='0.25'/>"
	"</svg>";

static const char* const kSvgBands = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 1920 1080' preserveAspectRatio='xMidYMid slice'>"
	"<defs><linearGradient id='bg' x1='0' y1='0' x2='1' y2='1'><stop offset='0' stop-color='{dark}'/><stop offset='1' stop-color='{accent}'/></linearGradient></defs>"
	"<rect width='1920' height='1080' fill='url(#bg)'/>"
	"<polygon points='0,1080 0,640 1400,0 1920,0 1920,120 560,1080' fill='{light}' fill-opacity='0.16'/>"
	"<polygon points='0,1080 640,1080 1920,260 1920,520 1100,1080' fill='{analog1}' fill-opacity='0.22'/>"
	"<polygon points='1200,1080 1920,640 1920,1080' fill='{darker}' fill-opacity='0.45'/>"
	"<polygon points='0,0 700,0 0,420' fill='{darker}' fill-opacity='0.35'/>"
	"</svg>";


static bool
ReadWholeFile(const char* path, std::string* text)
{
	BFile file(path, B_READ_ONLY);
	off_t size;
	if (file.InitCheck() != B_OK || file.GetSize(&size) != B_OK || size <= 0 || size > 8 * 1024 * 1024)
		return false;
	text->resize((size_t)size);
	return file.Read(&(*text)[0], (size_t)size) == (ssize_t)size;
}


// Draws the template in the accent colour into a PNG and sets it as the wallpaper (the picture of every workspace). Returns false
// when it could not be done (no librsvg, or the template is not a good one).
static bool
ApplySvgWallpaper(const BMessage& settings, const rgb_color& accent)
{
	std::string template_;
	const std::string file = settings.GetString("svgfile", "builtin:waves");
	if (file == "builtin:waves")
		template_ = kSvgWaves;
	else if (file == "builtin:bubbles")
		template_ = kSvgBubbles;
	else if (file == "builtin:bands")
		template_ = kSvgBands;
	else if (!ReadWholeFile(file.c_str(), &template_)) {
		DebugLog("svg: cannot read %s", file.c_str());
		return false;
	}
	if (!LoadSvgApi()) {
		DebugLog("svg: librsvg/cairo/gobject could not be loaded");
		return false;
	}

	const BRect screen = BScreen().Frame();
	const int width = (int)screen.Width() + 1, height = (int)screen.Height() + 1;
	const std::string svg = ApplyAccent(template_, accent);

	BPath folder;
	if (find_directory(B_USER_CACHE_DIRECTORY, &folder, true) != B_OK || folder.Append("snaketracker_wallpapers/svg") != B_OK
		|| create_directory(folder.Path(), 0755) != B_OK)
		return false;
	char name[48];
	snprintf(name, sizeof(name), "svg-%02x%02x%02x-%dx%d.png", accent.red, accent.green, accent.blue, width, height);
	BPath target(folder.Path(), name);

	void* handle = sSvg.newFromData((const unsigned char*)svg.data(), svg.size(), NULL);
	if (handle == NULL) {
		DebugLog("svg: the template is not a good SVG");
		return false;
	}
	void* surface = sSvg.surfaceCreate(0 /* ARGB32 */, width, height);
	void* cairo = surface != NULL ? sSvg.create(surface) : NULL;
	bool drawn = false;
	if (cairo != NULL) {
		const double viewport[4] = { 0, 0, (double)width, (double)height };
		drawn = sSvg.renderDocument(handle, cairo, viewport, NULL) != 0 && sSvg.writePng(surface, target.Path()) == 0;
		sSvg.destroy(cairo);
	}
	if (surface != NULL)
		sSvg.surfaceDestroy(surface);
	sSvg.unref(handle);
	DebugLog("svg: drawn %s: %d", target.Path(), (int)drawn);
	if (!drawn)
		return false;

	BPath desktop;
	if (find_directory(B_DESKTOP_DIRECTORY, &desktop) != B_OK)
		return false;
	BNode node(desktop.Path());
	if (node.InitCheck() != B_OK)
		return false;
	CurrentBackground current;
	ReadCurrentBackground(node, &current);
	std::vector<std::string> paths(1, target.Path());
	const bool done = WriteBackgrounds(node, current, paths, false, 2);

	// (the pictures of earlier colors are not needed any more)
	BDirectory directory(folder.Path());
	BEntry entry;
	while (directory.GetNextEntry(&entry, false) == B_OK) {
		char found[B_FILE_NAME_LENGTH];
		if (entry.GetName(found) == B_OK && strcmp(found, name) != 0 && strncmp(found, "svg-", 4) == 0)
			entry.Remove();
	}
	return done;
}


class ServerApp : public BApplication {
public:
	ServerApp()
		:
		BApplication(kServerSignature),
		fWallpaperRunner(NULL),
		fUpdateRunner(NULL),
		fRestartRunner(NULL),
		fHaveId(false),
		fChanged(false),
		fAsked(false),
		fColorRunner(NULL),
		fLastAccent(0),
		fFirstRunner(NULL),
		fDailyRunner(NULL),
		fFetching(false),
		fApplyAfter(false),
		fCursor(0)
	{
	}

	// Fetches what the online sources have today (when they have not been fetched yet), in the background; apply: and then
	// changes the wallpaper.
	void StartFetch(bool apply)
	{
		if (fFetching) {
			fApplyAfter = fApplyAfter || apply;
			return;
		}
		FetchJob* job = new FetchJob;
		job->apply = apply;
		job->settings = fWallpaper;
		const char* source;
		for (int32 i = 0; fWallpaper.FindString("online", i, &source) == B_OK; i++) {
			// (Flickr and Unsplash only with the user's key)
			if ((strcmp(source, "flickr") == 0 && !fWallpaper.HasString("flickrkey"))
				|| (strcmp(source, "unsplash") == 0 && !fWallpaper.HasString("unsplashkey"))
				|| (strcmp(source, "pexels") == 0 && !fWallpaper.HasString("pexelskey"))
				|| (strcmp(source, "pixabay") == 0 && !fWallpaper.HasString("pixabaykey")))
				continue;
			if (NeedsFetch(source))
				job->sources.push_back(source);
		}
		thread_id thread = -1;
		if (!job->sources.empty()) {
			fFetching = true;
			thread = spawn_thread(FetchThread, "snaketracker pictures", B_LOW_PRIORITY, job);
		}
		if (thread >= 0)
			resume_thread(thread);
		else {
			delete job;
			fFetching = false;
			if (apply)
				ChangeWallpaper(fWallpaper, &fCursor);
		}
	}

	// The accent colour Tracker shows (the SnakeTracker color or Haiku's selection color, as the preferences say).
	rgb_color AccentColor()
	{
		BMessage question(kAccentQuery), answer;
		int32 packed;
		if (BMessenger("application/x-vnd.Be-TRAK").SendMessage(&question, &answer, 500000, 500000) == B_OK
			&& answer.FindInt32("accent", &packed) == B_OK)
			return make_color((packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF);
		return ui_color(B_MENU_SELECTION_BACKGROUND_COLOR);
	}

	// The SVG wallpaper, in the accent color of now (force: even when the color is the one it was drawn in).
	void ShowSvg(bool force)
	{
		const rgb_color accent = AccentColor();
		const uint32 packed = ((uint32)accent.red << 16) | ((uint32)accent.green << 8) | accent.blue;
		if (!force && packed == fLastAccent)
			return;
		if (ApplySvgWallpaper(fWallpaper, accent))
			fLastAccent = packed;
		else if (force) {
			BAlert* alert = new BAlert("", "The SVG wallpaper could not be drawn. It needs the librsvg package, "
				"and a template that is a good SVG file.", "OK");
			alert->Go(NULL);
		}
	}

	// The package was installed again: ask if Tracker and the Deskbar may be restarted now (the programs that run still have the
	// old files). A change has to hold for another look first: the package may still be settling.
	void CheckForUpdate()
	{
		FileId now;
		const bool read = ReadFileId(&now);
		DebugLog("update check: asked %d, have %d, read %d, same %d, off %d, changed %d", (int)fAsked, (int)fHaveId, (int)read,
			(int)(read && now == fStartId), (int)UpdateQuestionOff(), (int)fChanged);
		if (fAsked || !fHaveId || !ReadFileId(&now) || now == fStartId || UpdateQuestionOff()) {
			fChanged = false;
			return;
		}
		if (!fChanged) {
			fChanged = true;
			return;
		}
		fAsked = true;
		BAlert* alert = new BAlert("", "SnakeTracker was updated.\n\nTracker and the Deskbar (and the SnakeTracker server) "
			"still run the old version. Restart them now to use the new one? Open Tracker windows will close.",
			"Later", "Restart now", NULL, B_WIDTH_AS_USUAL, B_INFO_ALERT);
		alert->SetShortcut(0, B_ESCAPE);
		alert->Go(new BInvoker(new BMessage(kMsgUpdateAnswer), BMessenger(this)));
	}

	// Quits Tracker and the Deskbar (they are started again by this server, now from the new files) and then the server, which a
	// helper starts again.
	void RestartEverything()
	{
		BMessenger("application/x-vnd.Be-TRAK").SendMessage(B_QUIT_REQUESTED);
		BMessenger("application/x-vnd.Be-TSKB").SendMessage(B_QUIT_REQUESTED);
		// (the helper outlives the server: it starts the new one once this one is gone)
		system("(sleep 14; launch_roster start x-vnd.snaketracker-server) >/dev/null 2>&1 &");
		BMessage again(kMsgRestartServer);
		fRestartRunner = new BMessageRunner(BMessenger(this), &again, 9 * 1000000LL, 1);
	}

	void LoadWallpaper()
	{
		ReadWallpaperSettings(&fWallpaper);
		delete fWallpaperRunner;
		fWallpaperRunner = NULL;
		delete fColorRunner;
		fColorRunner = NULL;
		if (fWallpaper.GetBool("svg", false)) {
			// (the SVG wallpaper takes the place of the pictures: it is drawn again when the accent color changes)
			BMessage colors(kMsgWallpaperColors);
			fColorRunner = new BMessageRunner(BMessenger(this), &colors, 2000000);
			return;
		}
		if (!fWallpaper.GetBool("enabled", false))
			return;
		const bigtime_t interval = (bigtime_t)fWallpaper.GetInt32("interval", 30) * 60 * 1000000;
		BMessage tick(kMsgWallpaperTick);
		fWallpaperRunner = new BMessageRunner(BMessenger(this), &tick, interval);
	}

	virtual void ReadyToRun()
	{
		srand((unsigned)system_time());
		fHaveId = ReadFileId(&fStartId);
		BMessage check(kMsgCheckUpdate);
		fUpdateRunner = new BMessageRunner(BMessenger(this), &check, 15 * 1000000LL);
		LoadWallpaper();
		// the pictures of the day: once the desktop has settled, and every hour after that (it fetches only on a new day)
		BMessage daily(kMsgWallpaperDaily);
		fFirstRunner = new BMessageRunner(BMessenger(this), &daily, 30 * 1000000LL, 1);
		fDailyRunner = new BMessageRunner(BMessenger(this), &daily, 3600 * 1000000LL);
		thread_id thread = spawn_thread(HandoffThread, "snaketracker handoff", B_LOW_PRIORITY, NULL);
		if (thread >= 0)
			resume_thread(thread);
		thread = spawn_thread(BootWatchThread, "snaketracker boot watch", B_LOW_PRIORITY, NULL);
		if (thread >= 0)
			resume_thread(thread);
		be_roster->StartWatching(BMessenger(this), B_REQUEST_LAUNCHED);
		thread = spawn_thread(UpdateThread, "snaketracker update check", B_LOW_PRIORITY, NULL);
		if (thread >= 0)
			resume_thread(thread);
	}

	virtual void MessageReceived(BMessage* message)
	{
		switch (message->what) {
			case kMsgCheckUpdate:
				CheckForUpdate();
				return;
			case kMsgUpdateAnswer:
			{
				int32 which = 0;
				if (message->FindInt32("which", &which) == B_OK && which == 1)
					RestartEverything();
				return;
			}
			case kMsgRestartServer:
				PostMessage(B_QUIT_REQUESTED);
				return;
			case kMsgWallpaperPing:
			{
				BMessage pong(kMsgWallpaperPong);
				pong.AddString("build", SNAKE_BUILD_ID);
				message->SendReply(&pong);
				return;
			}
			case kMsgWallpaperReload:
				LoadWallpaper();
				StartFetch(false);
				if (fWallpaper.GetBool("svg", false))
					ShowSvg(true);
				return;
			case kMsgWallpaperColors:
				if (fWallpaper.GetBool("svg", false))
					ShowSvg(false);
				return;
			case kMsgWallpaperNext:
				ReadWallpaperSettings(&fWallpaper);
				if (fWallpaper.GetBool("svg", false))
					ShowSvg(true);
				else
					StartFetch(true);
				return;
			case kMsgWallpaperDaily:
				StartFetch(false);
				return;
			case kMsgWallpaperFetched:
			{
				const bool apply = message->GetBool("apply", false) || fApplyAfter;
				fFetching = false;
				fApplyAfter = false;
				if (apply)
					ChangeWallpaper(fWallpaper, &fCursor);
				return;
			}
			case kMsgWallpaperTick:
				if (fWallpaper.GetBool("enabled", false))
					ChangeWallpaper(fWallpaper, &fCursor);
				return;
		}
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
			if (strcasecmp(signature, kPrograms[i].signature) != 0 || sFallback[i])
				continue;
			thread_id thread = spawn_thread(ReplaceThread, "snaketracker replace", B_LOW_PRIORITY, (void*)(addr_t)i);
			if (thread >= 0)
				resume_thread(thread);
		}
	}

	virtual bool QuitRequested()
	{
		delete fWallpaperRunner;
		fWallpaperRunner = NULL;
		delete fColorRunner;
		fColorRunner = NULL;
		delete fUpdateRunner;
		fUpdateRunner = NULL;
		delete fFirstRunner;
		delete fDailyRunner;
		fFirstRunner = fDailyRunner = NULL;
		be_roster->StopWatching(BMessenger(this));
		sStop = 1;
		return true;
	}

private:
	BMessage fWallpaper;
	BMessageRunner* fWallpaperRunner;
	BMessageRunner* fUpdateRunner;
	BMessageRunner* fRestartRunner;
	FileId fStartId;
	bool fHaveId;
	bool fChanged;
	bool fAsked;
	BMessageRunner* fColorRunner;
	uint32 fLastAccent;
	BMessageRunner* fFirstRunner;
	BMessageRunner* fDailyRunner;
	bool fFetching;
	bool fApplyAfter;
	uint32 fCursor;
};


int
main()
{
	ServerApp app;
	app.Run();
	return 0;
}
