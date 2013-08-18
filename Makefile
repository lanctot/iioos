all: iioos.pdf

iioos.pdf: iioos.tex iioos.bib
	latexmk -pdf iioos.tex

# marc likes having this one:
view: iioos.pdf
	evince iioos.pdf

clean:
	latexmk -c
