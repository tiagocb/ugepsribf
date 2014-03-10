OPTS	= -O3
CHARMDIR = ~/charm++/charm
CHARMC = $(CHARMDIR)/bin/charmc $(OPTS)

OBJS	= teste.o

all: teste

teste: $(OBJS)
	$(CHARMC) -language charm++ -lhwloc -module CommonLBs -o $(output_file) $(OBJS)

projections: $(COMPOBJS)
	$(CHARMC) -language charm++ -tracemode projections -lz -o teste.prj $(OBJS)

teste.decl.h: teste.ci
	$(CHARMC) teste.ci

teste.o: teste.C teste.decl.h
	$(CHARMC) -c teste.C

clean:
	rm -f *.decl.h *.def.h conv-host *.o charmrun *~ teste.prj

