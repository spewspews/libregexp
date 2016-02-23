#include <u.h>
#include <libc.h>
#include "regex.h"

typedef struct Thread Thread;
typedef struct Threadlist Threadlist;

struct Thread
{
	Reinst *pc;
	Resub se[NSUBEXP];
};
struct Threadlist
{
	Thread *threads;
	Thread *next;
};

int
regexec(Reprog *prog, char *str, Resub *se, int msize)
{
	Threadlist lists[2], *clist, *nlist, *tmp;
	Thread *t;
	Reinst *curinst;
	char *sp;
	Rune r;
	int i, match, firstmatch;

	for(clist = lists; clist < lists + 2; clist++) {
		clist->threads = calloc(sizeof(*clist->threads), prog->len+strlen(str));
		if(clist->threads == nil)
			regerror("Out of memory");
		clist->next = clist->threads;
	}
	clist = lists;
	nlist = lists+1;

	match = 0;
	r = 1;
	for(sp = str; r != 0; sp += i) {
		/* We only want the left-most match. */
		if(match == 0) {
			clist->next->pc = prog->startinst;
			clist->next++;
		}
		i = chartorune(&r, sp);
		firstmatch = 1;
		for(t = clist->threads; t < clist->next; t++) {
			curinst = t->pc;
Again:
			switch(curinst->op) {
			case ORUNE:
				if(r == curinst->r)
					goto Next;
				break;
			case OCLASS:
				if((r >= curinst->r && r <= curinst->r1))
					goto Next;
				break;
			case OANY:
			Next:
				nlist->next->pc = curinst + 1;
				memcpy(nlist->next->se, t->se, sizeof(Resub)*msize);
				nlist->next++;
				break;
			case ONOTNL:
				if(r != L'\n') {
					curinst++;
					goto Again;
				}
				break;
			case OBOL:
				if(sp == str || *(sp-1) == '\n') {
					curinst++;
					goto Again;
				}
				break;
			case OEOL:
				if(r == 0 || r == '\n') {
					curinst++;
					goto Again;
				}
				break;
			case OJMP:
				curinst = curinst->a;
				goto Again;
			case OSPLIT:
				clist->next->pc = curinst->b;
				memcpy(clist->next->se, t->se, sizeof(Resub)*msize);
				clist->next++;
				curinst = curinst->a;
				goto Again;
			case OSAVE:
				if(curinst->sub < msize)
					t->se[curinst->sub].sp = sp;
				curinst++;
				goto Again;
			case OUNSAVE:
				t->se[curinst->sub].ep = sp;
				/* Earliest match is the left-most longest. */
				if(curinst->sub == 0 && firstmatch) {
					match = 1;
					memcpy(se, t->se, sizeof(se)*msize);
					firstmatch = 0;
					break;
				}
				curinst++;
				goto Again;
			}
		}
		tmp = clist;
		clist = nlist;
		nlist = tmp;
		nlist->next = nlist->threads;
	}
	return match;
}
