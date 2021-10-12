.SUFFIXES: .g .ps

SOURCE = logfs
TEX = /sprite2/users/douglis/lib/tex

${SOURCE}.ps: ${SOURCE}.dvi
	/usr/local/lib/ps/dvi2ps ${SOURCE}.dvi > ${SOURCE}.ps

FIGS = f1.g 
PSFIGS = f1.ps

${SOURCE}.dvi: ${SOURCE}.tex ${PSFIGS} ${SOURCE}.bbl ${TEX}/defs.tex
	latex ${SOURCE}

GR2PS = gr2ps

.g.ps:
	${GR2PS} -s 0.5 0.5 -t 17 80 -o $@ $<

${SOURCE}.bbl: ${TEX}/bib/os.bib ${TEX}/bib/sosp11.bib
	bibtex ${SOURCE}

print: ${SOURCE}.dvi
	dlw ${SOURCE}.dvi

printfig: f1.lpr
	lpr -h f1.lpr

f1.lpr: f1.g
	gr2ps -o f1.lpr f1.g
	echo "showpage" >> f1.lpr

clean:
	rm -f *.dvi *.ps *.log *.blg

${PSFIGS}: Makefile
