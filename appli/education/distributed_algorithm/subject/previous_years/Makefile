NAME = tp-description TP-DistribAlg-2015-v1 TP-DistribAlg-2015-v2
PDFS = $(patsubst %,%.pdf,$(NAME))
PDFFLAGS = -halt-on-error -file-line-error
all: $(PDFS)

$(PDFS): %.pdf: %.tex
	pdflatex $(PDFFLAGS) $*.tex
	pdflatex $(PDFFLAGS) $*.tex
	pdflatex $(PDFFLAGS) $*.tex

clean:
	$(RM) *.log *.aux

realclean: clean
	$(RM) $(PDFS)
