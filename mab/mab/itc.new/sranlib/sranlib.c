/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 * Revisions for SPUR: Copyright (c) 1987 Regents of the University of 
 *       California.
 * All rights reserved.
 *
 * Revised by P. N. Hilfinger
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1980, 1987 Regents of the University of California.\n\
 All rights reserved.\n";
#endif not lint

#ifndef lint
static char rcsid[] = "$Header: /sprite/src/benchmarks/itc/sranlib/RCS/sranlib.c,v 1.2 93/02/11 17:22:51 kupfer Exp $ SPRITE (Berkeley)";
#endif not lint

/*
 * ranlib - create table of contents for archive; string table version
 */
#include <sys/types.h>
#include <ar.h>
#include <ranlib.h>
#include <string.h>
#include "a.out.h"
#include <stdio.h>
#include "sysv.h"

extern long lseek();

struct	ar_hdr	archdr;
#define	OARMAG 0177545
long	arsize;
struct	exec	exp;
FILE	*fi, *fo;
long	off, oldoff;
long	atol(), ftell();
#define TABSZ	3000
int	tnum;
#define	STRTABSZ	30000
int	tssiz;
char	*strtab;
int	ssiz;
int	new;
char	tempnm[] = "__.SYMDEF";
char	firstname[17];
void	stash();
char *malloc(), *calloc();

/*
 * table segment definitions
 */
char	*segalloc();
void	segclean();
struct	tabsegment {
	struct tabsegment	*pnext;
	unsigned		nelem;
	struct ranlib		tab[TABSZ];
} tabbase, *ptabseg;
struct	strsegment {
	struct strsegment	*pnext;
	unsigned		nelem;
	char			stab[STRTABSZ];
} strbase, *pstrseg;

main(argc, argv)
char **argv;
{
	char cmdbuf[BUFSIZ];
	/* magbuf must be an int array so it is aligned on an int-ish
	   boundary, so that we may access its first word as an int! */
	int magbuf[(SARMAG+sizeof(int))/sizeof(int)];
	register int just_touch = 0;
	register struct tabsegment *ptab;
	register struct strsegment *pstr;

	/* check for the "-t" flag" */
	if (argc > 1 && strcmp(argv[1], "-t") == 0) {
		just_touch++;
		argc--;
		argv++;
	}

	--argc;
	while(argc--) {
		fi = fopen(*++argv,"r");
		if (fi == NULL) {
			(void) fprintf(stderr, "ranlib: cannot open %s\n", *argv);
			continue;
		}
		off = SARMAG;
		(void) fread((char *)magbuf, 1, SARMAG, fi);
		if (strncmp((char *)magbuf, ARMAG, SARMAG)) {
			if (magbuf[0] == OARMAG)
				(void) fprintf(stderr, "old format ");
			else
				(void) fprintf(stderr, "not an ");
			(void) fprintf(stderr, "archive: %s\n", *argv);
			continue;
		}
		if (just_touch) {
			register int	len;

			(void) fseek(fi, (long) SARMAG, 0);
			if (fread(cmdbuf, sizeof archdr.ar_name, 1, fi) != 1) {
				(void) fprintf(stderr, "malformed archive: %s\n",
					*argv);
				continue;
			}
			len = strlen(tempnm);
			if (bcmp(cmdbuf, tempnm, len) != 0 ||
			    cmdbuf[len] != ' ') {
				(void) fprintf(stderr, "no symbol table: %s\n", *argv);
				continue;
			}
			(void) fclose(fi);
			fixdate(*argv);
			continue;
		}
		(void) fseek(fi, 0L, 0);
		new = tssiz = tnum = 0;
		segclean();
		if (nextel(fi) == 0) {
			(void) fclose(fi);
			continue;
		}
		do {
			long o;
			register n;
			struct nlist sym;

			(void) fread((char *)&exp, 1, sizeof(struct exec), fi);
			if (N_BADMAG(exp))
				continue;
			if (!strncmp(tempnm, archdr.ar_name, sizeof(archdr.ar_name)))
				continue;
			if (exp.a_syms == 0) {
				(void) fprintf(stderr, "ranlib: warning: %s(%s): no symbol table\n", *argv, archdr.ar_name);
				continue;
			}
			o = N_STROFF(exp) - sizeof (struct exec);
			if (ftell(fi)+o+sizeof(ssiz) >= off) {
				(void) fprintf(stderr, "ranlib: warning: %s(%s): old format .o file\n", *argv, archdr.ar_name);
				continue;
			}
			(void) fseek(fi, o, 1);
			(void) fread((char *)&ssiz, 1, sizeof (ssiz), fi);
			if (ssiz < sizeof ssiz){
				/* sanity check */
				(void) fprintf(stderr, "ranlib: warning: %s(%s): mangled string table\n", *argv, archdr.ar_name);
				continue;
			}
			strtab = (char *)calloc(1, (unsigned int) ssiz);
			if (strtab == 0) {
				(void) fprintf(stderr, "ranlib: ran out of memory\n");
				exit(1);
			}
			(void) fread(strtab+sizeof(ssiz), ssiz - sizeof(ssiz), 1, fi);
			(void) fseek(fi,(long) -(exp.a_syms+ssiz), 1);
			n = exp.a_syms / sizeof(struct nlist);
			while (--n >= 0) {
				(void) fread((char *)&sym, 1, sizeof(sym), fi);
				if (sym.n_un.n_strx == 0)
					continue;
				sym.n_un.n_name = strtab + sym.n_un.n_strx;
				if ((sym.n_type&N_EXT) != 0 
				    && (sym.n_type&N_TYPE) != N_UNDF
				    && (sym.n_type&N_STAB) == 0) {
				    stash(&sym);
				}
			}
			free(strtab);
		} while(nextel(fi));
		new = fixsize();
		(void) fclose(fi);
		fo = fopen(tempnm, "w");
		if(fo == NULL) {
			(void) fprintf(stderr, "can't create temporary\n");
			exit(1);
		}
		tnum *= sizeof (struct ranlib);
		(void) fwrite((char *) &tnum, 1, sizeof (tnum), fo);
		tnum /= sizeof (struct ranlib);
		ptab = &tabbase;
		do {
			(void) fwrite((char *)ptab->tab, (int) ptab->nelem,
			    sizeof(struct ranlib), fo);
		} while (ptab = ptab->pnext);
		(void) fwrite((char *) &tssiz, 1, sizeof (tssiz), fo);
		pstr = &strbase;
		do {
			(void) fwrite(pstr->stab, (int) pstr->nelem, 1, fo);
			tssiz -= pstr->nelem;
		} while (pstr = pstr->pnext);
		/* pad with nulls */
		while (tssiz--) putc('\0', fo);
		(void) fclose(fo);
		if(new)
			(void) sprintf(cmdbuf, "ar rb %s %s %s\n", firstname, *argv, tempnm);
		else
			(void) sprintf(cmdbuf, "ar r %s %s\n", *argv, tempnm);
		if(system(cmdbuf))
			(void) fprintf(stderr, "ranlib: ``%s'' failed\n", cmdbuf);
		else
			fixdate(*argv);
		(void) unlink(tempnm);
	}
	exit(0);
}

nextel(af)
FILE *af;
{
	register r;
	register char *cp;

	oldoff = off;
	(void) fseek(af, off, 0);
	r = fread((char *)&archdr, 1, sizeof(struct ar_hdr), af);
	if (r != sizeof(struct ar_hdr))
		return(0);
	for (cp=archdr.ar_name; cp < & archdr.ar_name[sizeof(archdr.ar_name)]; cp++)
		if (*cp == ' ')
			*cp = '\0';
	arsize = atol(archdr.ar_size);
	if (arsize & 1)
		arsize++;
	off = ftell(af) + arsize;
	return(1);
}

void
stash(s)
	struct nlist *s;
{
	register char *cp;
	register char *strtab;
	register strsiz;
	register struct ranlib *tab;
	register tabsiz;

	if (ptabseg->nelem >= TABSZ) {
		/* allocate a new symbol table segment */
		ptabseg = ptabseg->pnext =
		    (struct tabsegment *) segalloc(sizeof(struct tabsegment));
		ptabseg->pnext = NULL;
		ptabseg->nelem = 0;
	}
	tabsiz = ptabseg->nelem;
	tab = ptabseg->tab;

	if (pstrseg->nelem >= STRTABSZ) {
		/* allocate a new string table segment */
		pstrseg = pstrseg->pnext =
		    (struct strsegment *) segalloc(sizeof(struct strsegment));
		pstrseg->pnext = NULL;
		pstrseg->nelem = 0;
	}
	strsiz = pstrseg->nelem;
	strtab = pstrseg->stab;

	tab[tabsiz].ran_un.ran_strx = tssiz;
	tab[tabsiz].ran_off = oldoff;
redo:
	for (cp = s->n_un.n_name; strtab[strsiz++] = *cp++;)
		if (strsiz >= STRTABSZ) {
			/* allocate a new string table segment */
			pstrseg = pstrseg->pnext =
			    (struct strsegment *) segalloc(sizeof(struct strsegment));
			pstrseg->pnext = NULL;
			strsiz = pstrseg->nelem = 0;
			strtab = pstrseg->stab;
			goto redo;
		}

	tssiz += strsiz - pstrseg->nelem; /* length of the string */
	pstrseg->nelem = strsiz;
	tnum++;
	ptabseg->nelem++;
}

/* allocate a zero filled segment of size bytes */
char *
segalloc(size)
unsigned size;
{
	char *pseg = NULL;

	pseg = malloc(size);
	if (pseg == NULL) {
		(void) fprintf(stderr, "ranlib: ran out of memeory\n");
		exit(1);
	}
	return(pseg);
}

/* free segments */
void
segclean()
{
	register struct tabsegment *ptab;
	register struct strsegment *pstr;

	/*
	 * symbol table
	 *
	 * The first entry is static.
	 */
	ptabseg = &tabbase;
	ptab = ptabseg->pnext;
	while (ptabseg = ptab) {
		ptab = ptabseg->pnext;
		free((char *)ptabseg);
	}
	ptabseg = &tabbase;
	ptabseg->pnext = NULL;
	ptabseg->nelem = 0;

	/*
	 * string table
	 *
	 * The first entry is static.
	 */
	pstrseg = &strbase;
	pstr = pstrseg->pnext;
	while (pstrseg = pstr) {
		pstr = pstrseg->pnext;
		free((char *)pstrseg);
	}
	pstrseg = &strbase;
	pstrseg->pnext = NULL;
	pstrseg->nelem = 0;
}

fixsize()
{
	int i;
	off_t offdelta;
	register struct tabsegment *ptab;

	if (tssiz&1)
		tssiz++;
	offdelta = sizeof(archdr) + sizeof (tnum) + tnum * sizeof(struct ranlib) +
	    sizeof (tssiz) + tssiz;
	off = SARMAG;
	(void) nextel(fi);
	if(strncmp(archdr.ar_name, tempnm, sizeof (archdr.ar_name)) == 0) {
		new = 0;
		offdelta -= sizeof(archdr) + arsize;
	} else {
		new = 1;
		(void) strncpy(firstname, archdr.ar_name, sizeof(archdr.ar_name));
	}
	ptab = &tabbase;
	do {
		for (i = 0; i < ptab->nelem; i++)
			ptab->tab[i].ran_off += offdelta;
	} while (ptab = ptab->pnext);
	return(new);
}

/* patch time */
fixdate(s)
	char *s;
{
	long time();
	char buf[24];
	int fd;

	fd = open(s, 1);
	if(fd < 0) {
		(void) fprintf(stderr, "ranlib: can't reopen %s\n", s);
		return;
	}
	(void) sprintf(buf, "%-*ld", sizeof(archdr.ar_date), time((long *)NULL)+5);
	(void) lseek(fd, (long)(SARMAG + ((char *)archdr.ar_date-(char *)&archdr)), 0);
	(void) write(fd, buf, sizeof(archdr.ar_date));
	(void) close(fd);
}
