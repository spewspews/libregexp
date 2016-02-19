#include <u.h>
#include <libc.h>
#include "regex.h"

void
main(int argc, char **argv)
{
	if(argc != 2) {
		fprint(2, "regex please\n");
		exits("usage");
	}
	regcomp(argv[1]);
	exits(0);
}
