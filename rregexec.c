#include <u.h>
#include <libc.h>
#include "regex.h"

typedef struct Thread Thread;
typedef struct Threadlist Threadlist;
typedef struct Resublist Resublist;

struct Thread
{
	Reinst *pc;
	Resub *se;
};
struct Threadlist
{
	Thread *threads;
	Thread *next;
};
struct Resublist
{
	Resub *se;
	Resub *next;
};

int
rregexec(Reprog *prog, Rune *str, Resub *se, int msize)
{
	Threadlist lists[2], *clist, *nlist, *tmp;
	Thread *t;
	Reinst *curinst;
	Resublist selist;
	Rune *rp, last;
	int match, firstmatch, first;
	long rstrlen;

	rstrlen = runestrlen(str);
	for(clist = lists; clist < lists + 2; clist++) {
		clist->threads = calloc(sizeof(*clist->threads), prog->len+rstrlen);
		if(clist->threads == nil) {
			regerror("Out of memory");
			return 0;
		}
		clist->next = clist->threads;
	}
	clist = lists;
	nlist = lists+1;
	selist.se = calloc(sizeof(*se), (rstrlen + prog->len/2) * msize);
	if(selist.se == nil) {
		regerror("Out of memory");
		return 0;
	}
	selist.next = selist.se;

	match = 0;
	last = 1;
	for(rp = str; last != L'\0'; rp++) {
		last = *rp;
		firstmatch = first = 1;
		for(t = clist->threads; t < clist->next; t++) {
			curinst = t->pc;
Again:
			switch(curinst->op) {
			case ORUNE:
				if(*rp != curinst->r)
					break;
			case OANY: /* fallthrough */
				nlist->next->pc = curinst + 1;
				nlist->next->se = t->se;
				nlist->next++;
				break;
			case OCLASS:
				if(*rp < curinst->r)
					break;
				if(*rp > curinst->r1) {
					curinst++;
					goto Again;
				}
				nlist->next->pc = curinst->a;
				nlist->next->se = t->se;
				nlist->next++;
				break;
			case ONOTNL:
				if(*rp != L'\n') {
					curinst++;
					goto Again;
				}
				free(t->se);
				break;
			case OBOL:
				if(rp == str || *(rp-1) == L'\n') {
					curinst++;
					goto Again;
				}
				break;
			case OEOL:
				if(*rp == 0) {
					curinst++;
					goto Again;
				}
				if(*rp == '\n') {
					nlist->next->pc = curinst + 1;
					nlist->next->se = t->se;
					nlist->next++;
				}
				break;
			case OJMP:
				curinst = curinst->a;
				goto Again;
			case OSPLITSUB:
				clist->next->pc = curinst->b;
				if(msize) {
					clist->next->se = selist.next;
					selist.next += msize;
				}
				memcpy(clist->next->se, t->se, sizeof(Resub)*msize);
				clist->next++;
				curinst = curinst->a;
				goto Again;
			case OSPLIT:
				clist->next->pc = curinst->b;
				clist->next->se = t->se;
				clist->next++;
				curinst = curinst->a;
				goto Again;
			case OSAVE:
				if(curinst->sub < msize)
					t->se[curinst->sub].rsp = rp;
				curinst++;
				goto Again;
			case OUNSAVE:
				if(curinst->sub < msize)
					t->se[curinst->sub].rep = rp;
				/* Earliest match is the left-most longest. */
				if(curinst->sub == 0 && firstmatch) {
					firstmatch = 0;
					match = 1;
					memcpy(se, t->se, sizeof(se)*msize);
					break;
				}
				curinst++;
				goto Again;
			}
		}
		/* Start again once if we haven't found anything. */
		if(first == 1 && match == 0) {
			first = 0;
			t = clist->next++;
			if(msize) {
				t->se = selist.next;// = calloc(sizeof(*t->se), msize);
				selist.next += msize;
			}
			curinst = prog->startinst;
			goto Again;
		}
		/* If we have a match and no extant threads, we are done. */
		if(match == 1 && nlist->next == nlist->threads)
			break;
		tmp = clist;
		clist = nlist;
		nlist = tmp;
		nlist->next = nlist->threads;
	}

	for(clist = lists; clist < lists + 2; clist++)
		free(clist->threads);
	free(selist.se);
	return match;
}
