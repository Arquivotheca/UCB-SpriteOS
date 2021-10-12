#

FILES = text.me
PRINTER = -Pcad
PAGES =

xproof: ${FILES}
	soelim ${FILES} | grn ${PRINTER} | dtbl ${PRINTER} | ditroff -me ${PAGES} ${PRINTER} -t > dit.out

default: ${FILES}
	soelim ${FILES} | grn ${PRINTER} | dtbl ${PRINTER} | ditroff -me ${PAGES} ${PRINTER}

soelim:
	soelim ${FILES} > paper.in
