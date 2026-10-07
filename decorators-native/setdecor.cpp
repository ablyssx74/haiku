// Usage: setdecor [path of a decorator add-on]   ("" selects the default; no argument prints the current one)
#include <Application.h>
#include <String.h>
#include <DecoratorPrivate.h>
#include <stdio.h>
#include <string.h>

int
main(int argc, char** argv)
{
	BApplication app("application/x-vnd.setdecor");
	if (argc < 2) {
		BString name;
		bool ok = BPrivate::get_decorator(name);
		printf("decorator: \"%s\" (%s)\n", name.String(), ok ? "ok" : "failed");
		return 0;
	}
	status_t status = BPrivate::set_decorator(BString(argv[1]));
	printf("set_decorator(\"%s\"): %s\n", argv[1], strerror(status));
	return status == B_OK ? 0 : 1;
}
