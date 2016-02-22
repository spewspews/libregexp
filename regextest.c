#include <u.h>
#include <libc.h>
#include "regex.h"

void
main(int argc, char **argv)
{
	Reprog *reprog;
	Resub resub[10], *subp;
	int match, i;
	char c;

	if(argc != 3) {
		fprint(2, "regex and match string please\n");
		exits("usage");
	}
	reprog = regcomp(argv[1]);
	match = regexec(reprog, argv[2], resub, nelem(resub));
	if(match) {
		for(i = 0; i < nelem(resub); i++) {
			subp = resub+i;
			if(subp->ep == nil)
				break;
			print("Match %d: ", i);
			c = *subp->ep;
			*subp->ep = '\0';
			print("%s\n", subp->sp);
			*subp->ep = c;
		}
	} else
		print("No match.\n");
	exits(0);
}
