#include <u.h>
#include <libc.h>
#include "regex.h"

void
main(int argc, char **argv)
{
	Reprog *reprog;
	Resub resub[10], *subp;
	int match, i, lit;
	char c;

	lit = 0;
	ARGBEGIN {
	case 'l':
		lit = 1;
		break;
	} ARGEND

	if(argc != 2) {
		fprint(2, "regex and match string please\n");
		exits("usage");
	}

	if(lit)
		reprog = regcomplit(argv[0]);
	else
		reprog = regcomp(argv[0]);
	match = regexec(reprog, argv[1], resub, nelem(resub));
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
