// Usage: setlook [path of a control look add-on]   ("" selects the default look; no argument prints it)
#include <Application.h>
#include <String.h>
#include <InterfacePrivate.h>
#include <stdio.h>
#include <string.h>

int
main(int argc, char** argv)
{
	// the app_server calls need a connection, which a BApplication provides
	BApplication app("application/x-vnd.setlook");

	if (argc < 2) {
		BString current;
		BPrivate::get_control_look(current);
		printf("control look: \"%s\"\n", current.String());
		return 0;
	}
	status_t status = BPrivate::set_control_look(BString(argv[1]));
	printf("set_control_look(\"%s\"): %s\n", argv[1], strerror(status));
	return status == B_OK ? 0 : 1;
}
