#include <u.h>
#include <libc.h>
#include "regex.h"

void
str2runes(Rune *dst, int dlen, char *src)
{
	Rune *ep;

	ep = dst + dlen-1;
	while(*src != 0) {
		src += chartorune(dst++, src);
		if(dst == ep)
			break;
	}
	*dst = L'\0';
}

void
main(int argc, char **argv)
{
	Reprog *reprog;
	Resub resub[10], *subp;
	Rune *runestr, runesub[1024], r;
	int match, i, lit, nl, slen;
	char sub[1024], c;

	lit = 0;
	nl = 0;
	ARGBEGIN {
	case 'l':
		lit++;
		break;
	case 'n':
		nl++;
		break;
	} ARGEND

	if(argc != 2) {
		fprint(2, "regex and match string please\n");
		exits("usage");
	}

	if(lit)
		reprog = regcomplit(argv[0]);
	else if(nl)
		reprog = regcompnl(argv[0]);
	else
		reprog = regcomp(argv[0]);
	match = regexec(reprog, argv[1], resub, nelem(resub));
	if(match) {
		for(i = 0; i < nelem(resub); i++) {
			subp = resub+i;
			if(subp->ep == nil)
				continue;
			print("Match %d: ", i);
			c = *subp->ep;
			*subp->ep = '\0';
			print("%s\n", subp->sp);
			*subp->ep = c;
		}
		regsub("& | \\1 | \\2 | \\3", sub, nelem(sub), resub, nelem(resub));
		print("\nSubstitution string:\n");
		print("%s\n", sub);
	} else
		print("no match\n");
	print("\nNO SUBS MATCH\n");
	match = regexec(reprog, argv[1], nil, 0);
	if(match)
		print("yes match\n");
	else
		print("no match\n");



	print("\nRUNES\n");
	slen = strlen(argv[1]);
	runestr = calloc(sizeof(*runestr), slen+1);
	str2runes(runestr, slen+1, argv[1]);
	match = rregexec(reprog, runestr, resub, nelem(resub));
	if(match) {
		for(i = 0; i < nelem(resub); i++) {
			subp = resub+i;
			if(subp->rep == nil)
				continue;
			print("Match %d: ", i);
			r = *subp->rep;
			*subp->rep = '\0';
			print("%S\n", subp->rsp);
			*subp->rep = r;
		}
		rregsub(L"& | \\1 | \\2 | \\3", runesub, nelem(runesub), resub, nelem(resub));
		print("\nSubstitution string:\n");
		print("%S\n", runesub);
	} else
		print("no match\n");
	print("\nNO SUBS MATCH\n");
	match = rregexec(reprog, runestr, nil, 0);
	if(match)
		print("yes match\n");
	else
		print("no match\n");
	exits(0);
}
