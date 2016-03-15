#include <u.h>
#include <libc.h>
#include <thread.h>
#include "regex.h"

typedef struct Thread Thread;
typedef struct Threadlist Threadlist;
typedef struct Resubref Resubref;
typedef struct Resubreflist Resubreflist;

struct Thread
{
	Reinst *pc;
	Resubref *submatch;
};
struct Threadlist
{
	Thread *threads;
	Thread *next;
};
struct Resubref
{
	Ref;
	Resub *sem;
};
struct Resubreflist
{
	Resubref **subs;
	Resubref **next;
};

static void
freesubref(Resubref *subref)
{
	free(subref->sem);
	free(subref);
}

static void
pushsubref(Resubreflist *list, Resubref *sub)
{
	*list->next++ = sub;
}

static Resubref*
popsubref(Resubreflist *list, int msize)
{
	Resubref *sub;
	static int count;

	if(list->next == list->subs) {
		sub = malloc(sizeof(*sub));
		if(sub == nil)
			goto Fail;
		sub->sem = calloc(sizeof(*sub->sem), msize);
		if(sub->sem == nil)
			goto Fail;
	} else
		sub = *(--list->next);
	sub->ref = 0;
	return sub;
Fail:
	regerror("Out of memory");
	return nil;
}

int
regexec(Reprog *prog, char *str, Resub *sem, int msize)
{
	Threadlist lists[2], *clist, *nlist, *tmp;
	Thread *t;
	Reinst *curinst;
	Resubreflist sublist;
	Resubref *submatch, **matchp;
	char *sp;
	Rune r;
	int i, match, firstmatch, first;
	long rstrlen;

	rstrlen = utflen(str);
	for(clist = lists; clist < lists + 2; clist++) {
		clist->threads = calloc(sizeof(*clist->threads), prog->len+rstrlen);
		if(clist->threads == nil) {
			regerror("Out of memory");
			return 0;
		}
		clist->next = clist->threads;
	}
	sublist.subs = calloc(sizeof(*sublist.subs), utflen(prog->regstr));
	if(sublist.subs == nil) {
		regerror("Out of memory");
		return 0;
	}
	sublist.next = sublist.subs;
	clist = lists;
	nlist = lists+1;

	match = 0;
	r = Runemax + 1;
	for(sp = str; r != 0; sp += i) {
		i = chartorune(&r, sp);
		firstmatch = first = 1;
		for(t = clist->threads; t < clist->next; t++) {
			curinst = t->pc;
Again:
			switch(curinst->op) {
			case ORUNE:
				if(r != curinst->r)
					goto Threaddone;
			case OANY: /* fallthrough */
				nlist->next->pc = curinst + 1;
				nlist->next->submatch = t->submatch;
				nlist->next++;
				break;
			case OCLASS:
				if(r < curinst->r)
					goto Threaddone;
				if(r > curinst->r1) {
					curinst++;
					goto Again;
				}
				nlist->next->pc = curinst->a;
				nlist->next->submatch = t->submatch;
				nlist->next++;
				break;
			case ONOTNL:
				if(r != L'\n') {
					curinst++;
					goto Again;
				}
				goto Threaddone;
			case OBOL:
				if(sp == str || *(sp-1) == '\n') {
					curinst++;
					goto Again;
				}
				goto Threaddone;
			case OEOL:
				if(r == 0) {
					curinst++;
					goto Again;
				}
				if(r == '\n') {
					nlist->next->pc = curinst + 1;
					nlist->next->submatch = t->submatch;
					nlist->next++;
				}
				goto Threaddone;
			case OJMP:
				curinst = curinst->a;
				goto Again;
			case OSPLITSUB:
				clist->next->pc = curinst->b;
				if(msize) {
					submatch = popsubref(&sublist, msize);
					incref(submatch);
					memcpy(submatch->sem, t->submatch->sem, sizeof(*submatch->sem)*msize);
					clist->next->submatch = submatch;
				}
				clist->next++;
				curinst = curinst->a;
				goto Again;
			case OSPLIT:
				clist->next->pc = curinst->b;
				if(msize) {
					clist->next->submatch = t->submatch;
					incref(t->submatch);
				}
				clist->next++;
				curinst = curinst->a;
				goto Again;
			case OSAVE:
				if(curinst->sub < msize)
					t->submatch->sem[curinst->sub].sp = sp;
				curinst++;
				goto Again;
			case OUNSAVE:
				if(curinst->sub < msize)
					t->submatch->sem[curinst->sub].ep = sp;
				/* First match is the left-most longest. */
				if(curinst->sub == 0 && firstmatch) {
					firstmatch = 0;
					match = 1;
					if(msize)
						memcpy(sem, t->submatch->sem, sizeof(sem)*msize);
					goto Threaddone;
				}
				curinst++;
				goto Again;
			Threaddone:
				if(msize == 0)
					break;
				if(decref(t->submatch) == 0)
					pushsubref(&sublist, t->submatch);
				break;
			}
		}
		/* Start again once if we haven't found anything. */
		if(first == 1 && match == 0) {
			first = 0;
			t = clist->next++;
			if(msize) {
				t->submatch = popsubref(&sublist, msize);
				incref(t->submatch);
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
	for(matchp = sublist.subs; matchp < sublist.next; matchp++)
		freesubref(*matchp);
	free(sublist.subs);
	return match;
}
