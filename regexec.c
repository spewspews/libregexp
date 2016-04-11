#include <u.h>
#include <libc.h>
#include <newregexp.h>
#include "regimpl.h"

typedef struct RethreadQ RethreadQ;
struct RethreadQ
{
	Rethread *head;
	Rethread **tail;
};

int
regexec(Reprog *prog, char *str, Resub *sem, int msize)
{
	RethreadQ lists[2], *clist, *nlist, *tmp;
	Rethread *t, *nextthr, **availthr;
	Reinst *curinst;
	Rune r;
	char *sp;
	int i, match, firstmatch, first, gen;

	if(msize > NSUBEXPM)
		msize = NSUBEXPM;

	if(prog->startinst->gen != 0) {
		for(curinst = prog->startinst; curinst < prog->startinst + prog->len; curinst++)
			curinst->gen = 0;
	}

	clist = lists;
	clist->head = nil;
	clist->tail = &clist->head;
	nlist = lists + 1;
	nlist->head = nil;
	nlist->tail = &nlist->head;

	for(i = 0; i < prog->nthr; i++)
		prog->thrpool[i] = prog->threads + i;
	availthr = prog->thrpool + prog->nthr;

	gen = 0;
	match = 0;
	r = Runemax + 1; 
	for(sp = str; r != L'\0'; sp += i) {
		gen++;
		i = chartorune(&r, sp);
		first = firstmatch = 1;
		t = clist->head;
		if(t == nil)
			goto Next;
		curinst = t->pc;
Again:
		for(;;) {
			if(curinst->gen == gen)
				goto Done;
			curinst->gen = gen;
			switch(curinst->op) {
			case ORUNE:
				if(r != curinst->r)
					goto Done;
			case OANY: /* fallthrough */
			Any:
				nextthr = t->next;
				t->pc = curinst + 1;
				t->next = nil;
				*nlist->tail = t;
				nlist->tail = &t->next;
				if(nextthr == nil)
					goto Next;
				t = nextthr;
				curinst = t->pc;
				break;
			case OCLASS:
			Class:
				if(r < curinst->r)
					goto Done;
				if(r > curinst->r1) {
					curinst++;
					goto Class;
				}
				nextthr = t->next;
				t->pc = curinst->a;
				t->next = nil;
				*nlist->tail = t;
				nlist->tail = &t->next;
				if(nextthr == nil)
					goto Next;
				t = nextthr;
				curinst = t->pc;
				break;
			case ONOTNL:
				if(r != L'\n') {
					curinst++;
					break;
				}
				goto Done;
			case OBOL:
				if(sp == str || sp[-1] == '\n') {
					curinst++;
					break;
				}
				goto Done;
			case OEOL:
				if(r == 0) {
					curinst++;
					break;
				}
				if(r == '\n')
					goto Any;
				goto Done;
			case OJMP:
				curinst = curinst->a;
				break;
			case OSPLIT:
				nextthr = *--availthr;
				nextthr->next = t->next;
				nextthr->pc = curinst->b;
				if(msize)
					memcpy(nextthr->sem, t->sem, sizeof(Resub)*msize);
				t->next = nextthr;
				curinst = curinst->a;
				break;
			case OSAVE:
				t->sem[curinst->sub].sp = sp;
				curinst++;
				break;
			case OUNSAVE:
				if(curinst->sub == 0) {
					/* First match is the left-most longest. */
					if (!firstmatch)
						goto Done;
					firstmatch = 0;
					match = 1;
					if(msize) {
						memcpy(sem, t->sem, sizeof(Resub)*msize);
						sem->ep = sp;
					}
					goto Done;
				}
				t->sem[curinst->sub].ep = sp;
				curinst++;
				break;
			Done:
				*availthr++ = t;
				t = t->next;
				if(t == nil)
					goto Next;
				curinst = t->pc;
				break;
			}
		}
Next:
		/* Start again once if we haven't found anything. */
		if(first == 1 && match == 0) {
			first = 0;
			t = *--availthr;
			t->next = nil;
			if(msize)
				memset(t->sem, 0, sizeof(Resub)*msize);
			curinst = prog->startinst;
			goto Again;
		}
		/* If we have a match and no extant threads, we are done. */
		if(match == 1 && nlist->head == nil)
			break;
		tmp = clist;
		clist = nlist;
		nlist = tmp;
		nlist->head = nil;
		nlist->tail = &nlist->head;
	}
	return match;
}
