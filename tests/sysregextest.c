#include <u.h>
#include <libc.h>
#include <regexp.h>

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
usage(char *prog)
{
	fprint(2, "%s: %s -l -n -r reps regexp matchstr\n",  prog, prog);
	exits("usage");
}

void
main(int argc, char **argv)
{
	Reprog *reprog;
	Resub *resub, *subp;
	Rune *runestr, r;
	int match, i, lit, nl, slen;
	long reps, creps, nsubm;
	char c, *matchstr;

	lit = 0;
	nl = 0;
	reps = 1;
	creps = 1;
	nsubm = 10;
	ARGBEGIN {
	case 'l':
		lit++;
		break;
	case 'n':
		nl++;
		break;
	case 'r':
		reps = strtol(EARGF(usage(argv0)), nil, 0);
		break;
	case 'c':
		creps = strtol(EARGF(usage(argv0)), nil, 0);
		break;
	case 'm':
		nsubm = strtol(EARGF(usage(argv0)), nil, 0);
		break;
	default:
		usage(argv0);
	} ARGEND

	if(argc != 2)
		usage(argv0);

	resub = calloc(sizeof(*resub), nsubm);
	reprog = nil;
	for(i = 0; i < creps; i++) {
		if(lit)
			reprog = regcomplit(argv[0]);
		else if(nl)
			reprog = regcompnl(argv[0]);
		else
			reprog = regcomp(argv[0]);
	}
	matchstr = argv[1];
	match = 0;
	for(i = 0; i < reps; i++)
		match = regexec(reprog, matchstr, resub, nsubm);

	if(match) {
		for(i = 0; i < nsubm; i++) {
			subp = resub+i;
			if(subp->ep == nil)
				continue;
			print("Match %d: ", i);
			c = *subp->ep;
			*subp->ep = '\0';
			print("%s\n", subp->sp);
			*subp->ep = c;
		}
	} else
		print("no match\n");



	print("\nRUNES\n");
	slen = strlen(matchstr);
	runestr = calloc(sizeof(*runestr), slen+1);
	str2runes(runestr, slen+1, matchstr);
	for(i = 0; i < reps; i++)
		match = rregexec(reprog, runestr, resub, nsubm);

	if(match) {
		for(i = 0; i < nsubm; i++) {
			subp = resub+i;
			if(subp->rep == nil)
				continue;
			print("Match %d: ", i);
			r = *subp->rep;
			*subp->rep = '\0';
			print("%S\n", subp->rsp);
			*subp->rep = r;
		}
	} else
		print("no match\n");
	exits(0);
}
