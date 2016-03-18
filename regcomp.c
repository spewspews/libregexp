#include <u.h>
#include <libc.h>
#include "regex.h"
#include "regcomp.h"

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

static Renode*
e3(Parselex *plex)
{
	char error[128];
	Renode *n;

	switch(lex(plex)) {
	case LANY:
		return node(plex, TANY, nil, nil);
	case LBOL:
		return node(plex, TBOL, nil, nil);
	case LEOL:
		return node(plex, TEOL, nil, nil);
	case LRUNE:
		n = node(plex, TRUNE, nil, nil);
		n->r = plex->rune;
		return n;
	case LCLASS:
		if(plex->nc)
			return buildclassn(plex);
		return buildclass(plex);
	case LLPAR:
		n = e0(plex);
		n = node(plex, TSUB, n, nil);
		if(lex(plex) != LRPAR) {
			regerror("No matching parenthesis");
			break;
		}
		return n;
	default:
		if(plex->rune)
			snprint(error, sizeof(error), "%s: syntax error: %C", plex->orig, plex->rune);
		else
			snprint(error, sizeof(error), "%s: Parsing error", plex->orig);
		regerror(error);
		break;
	}
	return nil;
}

static Renode*
e2(Parselex *plex)
{
	Renode *n;

	n = e3(plex);
	if(lex(plex) == LREP) {
		switch(plex->rune) {
		case L'*':
			return node(plex, TSTAR, n, nil);
		case L'+':
			return node(plex, TPLUS, n, nil);
		case L'?':
			return node(plex, TQUES, n, nil);
		}
	}
	plex->peek = 1;
	return n;
}

static Renode*
invert(Renode *n)
{
	Renode *n1;

	if(n->op != TCAT)
		return n;
	while(n->left->op == TCAT) {
		n1 = n->left;
		n->left = n1->right;
		n1->right = n;
		n = n1;
	}
	return n;
}

static Renode*
e1(Parselex *plex)
{
	Renode *n;
	int sym;

	n = e2(plex);
	for(;;) {
		sym = lex(plex);
		if(sym == LEND || sym == LOR || sym == LRPAR)
			break;
		plex->peek = 1;
		n = node(plex, TCAT, n, e2(plex));
	}
	plex->peek = 1;
	return invert(n);
}

static Renode*
e0(Parselex *plex)
{
	Renode *n;

	n = e1(plex);
	for(;;) {
		if(lex(plex) != LOR)
			break;
		n = node(plex, TOR, n, e1(plex));
	}
	plex->peek = 1;
	return n;
}

static Reprog*
regcomp1(char *regstr, int nl, int lit)
{
	Reprog *reprog;
	Parselex plex;
	Renode *parsetr;
	int regstrlen;

	regstrlen = utflen(regstr);
	reprog = malloc(sizeof(*reprog) + sizeof(Reinst)*(regstrlen*2 + 5));
	plex.nodes = calloc(sizeof(*plex.nodes), regstrlen*2 + 10);
	if(reprog == nil || plex.nodes == nil)
		return nil;
	plex.freep = plex.nodes;
	plex.rawexp = plex.orig = regstr;
	plex.sub = 0;
	plex.getnextr = lit ? getnextrlit : getnextr;
	parsetr = node(&plex, TSUB, e0(&plex), nil);
//	prtree(parsetr, 0, 0);
	reprog->startinst = compile(parsetr, reprog, nl);
	reprog->regstr = regstr;
	free(plex.nodes);
//	prprog(reprog);
	return reprog;
}

Reprog*
regcomp(char *str)
{
	return regcomp1(str, 0, 0);
}

Reprog*
regcomplit(char *str)
{
	return regcomp1(str, 0, 1);
}

Reprog*
regcompnl(char *str)
{
	return regcomp1(str, 1, 0);
}

static Reinst*
compile1(Renode *renode, Reinst *reinst, int *sub, int nl)
{
	Reinst *i;
	int s;

Tailcall:
	if(renode == nil)
		return reinst;
	switch(renode->op) {
	case TCLASS:
		reinst->op = OCLASS;
		reinst->r = renode->r;
		reinst->r1 = renode->r1;
		reinst->a = reinst + 1 + renode->nclass;
		renode = renode->left;
		reinst++;
		goto Tailcall;
	case TCAT:
		reinst = compile1(renode->left, reinst, sub, nl);
		renode = renode->right;
		goto Tailcall;
	case TOR:
		reinst->op = OSPLIT;
		reinst->a = reinst + 1;
		i = compile1(renode->left, reinst->a, sub, nl);
		reinst->b = i + 1;
		i->op = OJMP;
		i->a = compile1(renode->right, reinst->b, sub, nl);
		return i->a;
	case TSTAR:
		reinst->op = OALT;
		reinst->a = reinst + 1;
		i = compile1(renode->left, reinst->a, sub, nl);
		reinst->b = i + 1;
		i->op = OJMP;
		i->a = reinst;
		return reinst->b;
	case TPLUS:
		i = reinst;
		reinst = compile1(renode->left, reinst, sub, nl);
		reinst->op = OALT;
		reinst->a = i;
		reinst->b = reinst + 1;
		return reinst->b;
	case TQUES:
		reinst->op = OALT;
		reinst->a = reinst + 1;
		reinst->b = compile1(renode->left, reinst->a, sub, nl);
		return reinst->b;
	case TSUB:
		reinst->op = OSAVE;
		reinst->sub = s = (*sub)++;
		reinst = compile1(renode->left, reinst+1, sub, nl);
		reinst->op = OUNSAVE;
		reinst->sub = s;
		return reinst + 1;
	case TANY:
		if(nl == 0)
			reinst++->op = ONOTNL;
		reinst->op = OANY;
		return reinst + 1;
	case TRUNE:
		reinst->op = ORUNE;
		reinst->r = renode->r;
		return reinst + 1;
	case TNOTNL:
		reinst->op = ONOTNL;
		return reinst + 1;
	case TEOL:
		reinst->op = OEOL;
		return reinst + 1;
	case TBOL:
		reinst->op = OBOL;
		return reinst + 1;
	}
	return nil;
}

static Reinst*
compile(Renode *parsetr, Reprog *reprog, int nl)
{
	Reinst *reinst, *end;
	int sub;

	reinst = (Reinst*)(reprog+1);
	end = compile1(parsetr, reinst, &sub, nl);
	reprog->len = end - reinst;
	return reinst;
}

static void
getnextr(Parselex *l)
{
	l->literal = 0;
	if(l->done) {
		l->rune = 0;
		return;
	}
	l->rawexp += chartorune(&l->rune, l->rawexp);
	if(*l->rawexp == L'\\') {
		l->rawexp += chartorune(&l->rune, l->rawexp);
		l->literal = 1;
	}
	if(*l->rawexp == 0)
		l->done = 1;
	return;
}

static void
getnextrlit(Parselex *l)
{
	l->literal = 1;
	if(l->done) {
		l->literal = 0;
		l->rune = 0;
		return;
	}
	l->rawexp += chartorune(&l->rune, l->rawexp);
	if(*l->rawexp == 0)
		l->done = 1;
	return;
}

static int
lex(Parselex *l)
{
	if(l->peek) {
		l->peek = 0;
		return l->peeklex;
	}
	l->getnextr(l);
	if(l->literal)
		return l->peeklex = LRUNE;
	switch(l->rune){
	case 0:
		return l->peeklex = LEND;
	case L'*':
	case L'?':
	case L'+':
		return l->peeklex = LREP;
	case L'|':
		return l->peeklex = LOR;
	case L'.':
		return l->peeklex = LANY;
	case L'(':
		return l->peeklex = LLPAR;
	case L')':
		return l->peeklex = LRPAR;
	case L'^':
		return l->peeklex = LBOL;
	case L'$':
		return l->peeklex = LEOL;
	case L'[':
		getclass(l);
		return l->peeklex = LCLASS;
	}
	return l->peeklex = LRUNE;
}

static int
pcmp(void *va, void *vb)
{
	vlong n;
	Rune *a, *b;

	a = va;
	b = vb;

	n = (vlong)b[0] - (vlong)a[0];
	if(n)
		return n;
	return (vlong)b[1] - (vlong)a[1];
}

static void
getclass(Parselex *l)
{
	Rune *p, *q, t;

	l->nc = 0;
	l->getnextr(l);
	if(l->rune == L'^') {
		l->nc = 1;
		l->getnextr(l);
	}
	p = l->cpairs;
	p[0] = l->rune;
	for(;;) {
		if(l->rune == '\\') {
			l->getnextr(l);
			p[0] = l->rune;
		}
		if(l->rune == L']')
			break;
		if(l->rune == 0) {
			regerror("No closing ] for class");
			return;
		}
		p[1] = l->rune;
		p += 2;
		if(p >= l->cpairs + nelem(l->cpairs) - 2) {
			regerror("Class too big");
			return;
		}
		l->getnextr(l);
		if(l->rune != L'-') {
			p[0] = l->rune;
			continue;
		}
		l->getnextr(l);
		if(l->rune == '\\')
			l->getnextr(l);
		if(l->rune == L']')
			break;
		p[-1] = l->rune;
		if(p[-2] > p[-1]) {
			t = p[-2];
			p[-2] = p[-1];
			p[-1] = t;
		}
		l->getnextr(l);
		p[0] = l->rune;
	}
	*p = 0;
	qsort(l->cpairs, (p - l->cpairs)/2, 2*sizeof(*l->cpairs), pcmp);
	q = l->cpairs;
	for(p = l->cpairs+2; *p != 0; p += 2) {
		if(p[1] < q[0] - 1) {
			q[2] = p[0];
			q[3] = p[1];
			q += 2;
			continue;
		}
		q[0] = p[0];
	}
	q[2] = 0;
}

/* classes are in descending order */
static Renode*
buildclassn(Parselex *l)
{
	Renode *n;
	Rune *p;
	int i;

	i = 0;
	p = l->cpairs;
	n = node(l, TCLASS, nil, nil);
	n->r = p[1] + 1;
	n->r1 = Runemax;
	n->nclass = i++;

	for(; *p != 0; p += 2) {
		n = node(l, TCLASS, n, nil);
		n->r = p[3] + 1;
		n->r1 = p[0] - 1;
		n->nclass = i++;
	}
	n->r = 0;
	return node(l, TCAT, node(l, TNOTNL, nil, nil), n);
}

static Renode*
buildclass(Parselex *l)
{
	Renode *n;
	Rune *p;
	int i;

	i = 0;
	n = node(l, TCLASS, nil, nil);
	n->r = Runemax + 1;
	n->nclass = i++;

	for(p = l->cpairs; *p != 0; p += 2) {
		n = node(l, TCLASS, n, nil);
		n->r = p[0];
		n->r1 = p[1];
		n->nclass = i++;
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
		for(i = 0; i < d; i++)
			print("\t");
		print("|\n");
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
	case TNOTNL:
		print("NOTNL: \\n\n");
		break;
	case TCLASS:
		print("CLASS: %C-%C\n", tree->r, tree->r1);
		prtree(tree->left, d, 1);
		break;
	}
}

static void
prprog(Reprog *reprog)
{
	Reinst *inst;

	print("Printing prog of len %d\n", reprog->len);
	for(inst = reprog->startinst; inst < reprog->startinst + reprog->len; inst++)
		prinst(inst);
}

static void
prinst(Reinst *inst)
{
	print("%p ", inst);
	switch(inst->op) {
	case ORUNE:
		print("ORUNE\t%C\n", inst->r);
		break;
	case ONOTNL:
		print("ONOTNL\n");
		break;
	case OCLASS:
		print("OCLASS\t%C-%C %p\n", inst->r, inst->r1, inst->a);
		break;
	case OALT:
		print("OALT\t%p %p\n", inst->a, inst->b);
		break;
	case OSPLIT:
		print("OSPLIT\t%p %p\n", inst->a, inst->b);
		break;
	case OJMP:
		print("OJMP \t%p\n", inst->a);
		break;
	case OSAVE:
		print("OSAVE\t%d\n", inst->sub);
		break;
	case OUNSAVE:
		print("OUNSAVE\t%d\n", inst->sub);
		break;
	case OANY:
		print("OANY \t.\n");
		break;
	case OEOL:
		print("OEOL \t$\n");
		break;
	case OBOL:
		print("OBOL \t^\n");
		break;
	}
}
