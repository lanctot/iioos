all: iioos.pdf

iioos.pdf: iioos.tex iioos.bib
	latexmk -pdf iioos.tex

clean:
	latexmk -c
