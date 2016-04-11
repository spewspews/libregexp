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
rregexec(Reprog *prog, Rune *str, Resub *sem, int msize)
{
	RethreadQ lists[2], *clist, *nlist, *tmp;
	Rethread *t, *next, **availthr;
	Reinst *curinst;
	Rune *rp, last;
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
	last = 1;
	for(rp = str; last != L'\0'; rp++) {
		gen++;
		last = *rp;
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
				if(*rp != curinst->r)
					goto Done;
			case OANY: /* fallthrough */
			Any:
				next = t->next;
				t->pc = curinst + 1;
				t->next = nil;
				*nlist->tail = t;
				nlist->tail = &t->next;
				t = next;
				if(t == nil)
					goto Next;
				curinst = t->pc;
				break;
			case OCLASS:
			Class:
				if(*rp < curinst->r)
					goto Done;
				if(*rp > curinst->r1) {
					curinst++;
					goto Class;
				}
				next = t->next;
				t->pc = curinst->a;
				t->next = nil;
				*nlist->tail = t;
				nlist->tail = &t->next;
				t = next;
				if(t == nil)
					goto Next;
				curinst = t->pc;
				break;
			case ONOTNL:
				if(*rp != L'\n') {
					curinst++;
					break;
				}
				goto Done;
			case OBOL:
				if(rp == str || rp[-1] == '\n') {
					curinst++;
					break;
				}
				goto Done;
			case OEOL:
				if(*rp == 0) {
					curinst++;
					break;
				}
				if(*rp == '\n')
					goto Any;
				goto Done;
			case OJMP:
				curinst = curinst->a;
				break;
			case OSPLIT:
				if(availthr > prog->thrpool) {
					availthr--;
					(*availthr)->next = t->next;
					t->next = *availthr;
					t->next->pc = curinst->b;
					if(msize)
						memcpy(t->next->sem, t->sem, sizeof(Resub)*msize);
				}
				curinst = curinst->a;
				break;
			case OSAVE:
				t->sem[curinst->sub].rsp = rp;
				curinst++;
				break;
			case OUNSAVE:
				/* First match is the left-most longest. */
				if(curinst->sub == 0) {
					if (!firstmatch)
						goto Done;
					firstmatch = 0;
					match = 1;
					if(msize) {
						memcpy(sem, t->sem, sizeof(Resub)*msize);
						sem->rep = rp;
					}
					goto Done;
				}
				t->sem[curinst->sub].rep = rp;
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
