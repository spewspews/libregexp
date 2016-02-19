#include <u.h>
#include <libc.h>
#include "regex.h"

enum
{
	LRUNE = 0,
	LEND,
	LREP,
	LOR,
	LANY,
	LLPAR,
	LRPAR,
	LBOL,
	LEOL,
	LCLASS,
	TCAT = 0,
	TOR,
	TSTAR,
	TPLUS,
	TQUES,
	TANY,
	TBOL,
	TEOL,
	TSUB,
	TCLASS,
	TRUNE,
	ORUNE = 0,
	OCLASS,
};

typedef struct Parselex Parselex;
typedef struct Renode Renode;

struct Parselex
{
	/* Parse */
	Renode *freep;
	Renode *nodes;
	/* Lex */
	char *rawexp;
	Rune yyrune;
	Rune yypeek;
	int peek;
	int done;
	int literal;
	Rune cpairs[200+2];
	int cn;
};
struct Renode
{
	int op;
	union
	{
		Rune r;
		Renode *left;
	};
	union
	{
		Rune r1;
		Renode *right;
	};
};

static int yylex(Parselex*);
static Renode *e0(Parselex*);
static void prtree(Renode*, int, int);
static void prprog(Reprog*);
static Renode *buildclass(Parselex*);
static Reinst *compile(Renode*, Reprog*);
void getclass(Parselex*);

static Renode*
node(Parselex *plex, int op, Renode *l, Renode *r)
{
	Renode *n;

	n = plex->freep++;
	n->op = op;
	n->left = l;
	n->right = r;
	return n;
}

void
regerror(char *s)
{
	fprint(2, "error: %s\n", s);
	exits(0);
}

static Renode*
e3(Parselex *plex)
{
	Renode *n;
	int sym;

	switch(sym = yylex(plex)) {
	case LANY:
		return node(plex, TANY, nil, nil);
	case LBOL:
		return node(plex, TBOL, nil, nil);
	case LEOL:
		return node(plex, TEOL, nil, nil);
	case LRUNE:
		n = node(plex, TRUNE, nil, nil);
		n->r = plex->yyrune;
		return n;
	case LCLASS:
		return buildclass(plex);
	case LLPAR:
		n = e0(plex);
		n = node(plex, TSUB, n, nil);
		if(yylex(plex) != LRPAR)
			regerror("no matching parenthesis");
		return n;
	default:
		print("%d\n", sym); 
		regerror("nope");
		break;
	}
	return nil;
}

static Renode*
e2(Parselex *plex)
{
	Renode *n;

	n = e3(plex);
	if(yylex(plex) == LREP) {
		switch(plex->yyrune) {
		case L'*':
			return node(plex, TSTAR, n, nil);
		case L'+':
			return node(plex, TPLUS, n, nil);
		case L'?':
			return node(plex, TQUES, n, nil);
		}
	}
	plex->yypeek = plex->yyrune;
	return n;
}

static Renode*
e1(Parselex *plex)
{
	Renode *n, *n1;
	int sym;

	n = e2(plex);
	for(;;) {
		sym = yylex(plex);
		if(sym == LEND || sym == LOR || sym == LRPAR)
			break;
		plex->yypeek = plex->yyrune;
		n1 = e2(plex);
		n = node(plex, TCAT, n, n1);
	}
	plex->yypeek = plex->yyrune;
	return n;
}

static Renode*
e0(Parselex *plex)
{
	Renode *n, *n1;

	n = e1(plex);
	for(;;) {
		if(yylex(plex) != LOR)
			break;
		n1 = e1(plex);
		n = node(plex, TOR, n, n1);
	}
	plex->yypeek = plex->yyrune;
	return n;
}

Reprog*
regcomp(char *regstr)
{
	Reprog *reprog;
	Parselex plex;
	Renode *parsetr;
	int regstrlen;

	regstrlen = utflen(regstr);
	reprog = malloc(sizeof(*reprog) + sizeof(Reinst)*regstrlen*2);
	plex.nodes = calloc(sizeof(*plex.nodes), regstrlen*2 + 2);
	if(reprog == nil || plex.nodes == nil)
		return nil;
	plex.freep = plex.nodes;
	plex.rawexp = regstr;
	parsetr = e0(&plex);
//	parsetr = node(&plex, TCAT, node(&plex, TSTAR, node(&plex, TANY, nil, nil), nil), parsetr);
	prtree(parsetr, 0, 0);
	reprog->startinst = compile(parsetr, reprog);
	free(plex.nodes);
	prprog(reprog);
	return reprog;
}

static Reinst*
compile1(Renode *renode, Reinst *reinst)
{
	switch(renode->op) {
	case TRUNE:
		reinst->op = ORUNE;
		reinst->r = renode->r;
		return ++reinst;
	case TCLASS:
		reinst->op = OCLASS;
		reinst->r = renode->r;
		reinst->r1 = renode->r1;
		return ++reinst;
	case TCAT:
		reinst = compile1(renode->left, reinst);
		return compile1(renode->right, reinst);
	}
	return nil;
}

static Reinst*
compile(Renode *parsetr, Reprog *reprog)
{
	Reinst *reinst, *end;

	reinst = (Reinst*)(reprog+1);
	end = compile1(parsetr, reinst);
	reprog->len = end - reinst;
	return reinst;
}

static void
getnextr(Parselex *l)
{
	l->literal = 0;
	if(l->done) {
		l->yyrune = 0;
		return;
	}
	l->rawexp += chartorune(&l->yyrune, l->rawexp);
	if(*l->rawexp == L'\\') {
		l->rawexp += chartorune(&l->yyrune, l->rawexp);
		l->literal = 1;
		return;
	}
	if(*l->rawexp == 0)
		l->done = 1;
	return;
}

static int
yylex(Parselex *l)
{
	if(l->yypeek) {
		l->yyrune = l->yypeek;
		l->yypeek = 0;
		return l->peek;
	}
	getnextr(l);
	if(l->literal)
		return l->peek = LRUNE;
	switch(l->yyrune){
	case 0:
		return l->peek = LEND;
	case L'*':
	case L'?':
	case L'+':
		return l->peek = LREP;
	case L'|':
		return l->peek = LOR;
	case L'.':
		return l->peek = LANY;
	case L'(':
		return l->peek = LLPAR;
	case L')':
		return l->peek = LRPAR;
	case L'^':
		return l->peek = LBOL;
	case L'$':
		return l->peek = LEOL;
	case L'[':
		getclass(l);
		return l->peek = LCLASS;
	}
	return l->peek = LRUNE;
}

int
pcmp(void *va, void *vb)
{
	vlong n;
	Rune *a, *b;

	a = va;
	b = vb;

	n = (vlong)a[0] - (vlong)b[0];
	if(n)
		return n;
	return (vlong)a[1] - (vlong)b[1];
}

void
getclass(Parselex *l)
{
	Rune *p, *q, t;

	getnextr(l);
	if(l->yyrune == L'^') {
		l->cn = 1;
		getnextr(l);
	}
	p = l->cpairs;
	p[0] = l->yyrune;
	for(;;) {
		if(l->yyrune == '\\') {
			getnextr(l);
			p[0] = l->yyrune;
		}
		if(l->yyrune == L']')
			break;
		p[1] = l->yyrune;
		p += 2;
		if(p >= l->cpairs + nelem(l->cpairs) - 2)
			regerror("class too big");
		getnextr(l);
		if(l->yyrune != L'-') {
			p[0] = l->yyrune;
			continue;
		}
		getnextr(l);
		if(l->yyrune == '\\')
			getnextr(l);
		if(l->yyrune == L']')
			break;
		p[-1] = l->yyrune;
		if(p[-2] > p[-1]) {
			t = p[-2];
			p[-2] = p[-1];
			p[-1] = t;
		}
		getnextr(l);
		p[0] = l->yyrune;
	}
	*p = 0;
	qsort(l->cpairs, (p - l->cpairs)/2, 2*sizeof(*l->cpairs), pcmp);
	q = l->cpairs;
	for(p = l->cpairs+2; *p != 0; p += 2) {
		if(p[0] > q[1] + 1) {
			q[2] = p[0];
			q[3] = p[1];
			q += 2;
			continue;
		}
		q[1] = p[1];
	}
	q[2] = 0;
}

static Renode*
buildclass(Parselex *l)
{
	Renode *n, *n1;
	Rune *p;

	p = l->cpairs;
	if(p[0] == p[1]) {
		n = node(l, TRUNE, nil, nil);
		n->r = p[0];
	} else {
		n = node(l, TCLASS, nil, nil);
		n->r = p[0];
		n->r1 = p[1];
	}
	for(p += 2; *p != 0; p+=2) {
		if(p[0] == p[1]) {
			n1 = node(l, TRUNE, nil, nil);
			n1->r = p[0];
			n = node(l, TOR, n, n1);
		} else {
			n1 = node(l, TCLASS, nil, nil);
			n1->r = p[0];
			n1->r1 = p[1];
			n = node(l, TOR, n, n1);
		}
	}
	return n;
}
	
static void
prtree(Renode *tree, int d, int f)
{
	int i;

	if(tree == nil)
		return;
	if(f)
	for(i = 0; i < d; i++)
		print("\t");
	switch(tree->op) {
	case TCAT:
		prtree(tree->left, d, 0);
		prtree(tree->right, d, 1);
		break;
	case TOR:
		print("TOR\n");
		prtree(tree->left, d+1, 1);
		prtree(tree->right, d+1, 1);
		break;
	case TSTAR:
		print("*\n");
		prtree(tree->left, d+1, 1);
		break;
	case TPLUS:
		print("+\n");
		prtree(tree->left, d+1, 1);
		break;
	case TQUES:
		print("?\n");
		prtree(tree->left, d+1, 1);
		break;
	case TANY:
		print(".\n");
		prtree(tree->left, d+1, 1);
		break;
	case TBOL:
		print("^\n");
		break;
	case TEOL:
		print("$\n");
		break;
	case TSUB:
		print("SUB\n");
		prtree(tree->left, d+1, 1);
		break;
	case TRUNE:
		print("RUNE: %C\n", tree->r);
		break;
	case TCLASS:
		print("CLASS: %C-%C\n", tree->r, tree->r1);
		break;
	}
}

static void
prprog(Reprog *reprog)
{
	Reinst *inst;

	print("Printing prog of len %d\n", reprog->len);
	for(inst = reprog->startinst; inst < reprog->startinst + reprog->len; inst++) {
		switch(inst->op) {
		case ORUNE:
			print("ORUNE\t%C\n", inst->r);
			break;
		case OCLASS:
			print("OCLASS\t%C-%C\n", inst->r, inst->r1);
			break;
		}
	}
}
