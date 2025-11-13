#PUBLICDIR= /usr0/cs564/public/project
SRC= buf.c hash.c pf.c
OBJ= buf.o hash.o pf.o
HDR = pftypes.h pf.h 

# Slotted page module
SLOT_SRC= slotted.c
SLOT_OBJ= slotted.o
SLOT_HDR= slotted.h

CFLAGS = -std=c89 -pedantic -Wno-implicit-int -Wno-old-style-definition -Wno-builtin-declaration-mismatch

pflayer.o: $(OBJ)
	ld -r -o pflayer.o $(OBJ)

tests: testhash testpf benchpf

testpf: testpf.o pflayer.o
	gcc $(CFLAGS) -o testpf testpf.o pflayer.o

benchpf: benchpf.o pflayer.o
	gcc $(CFLAGS) -o benchpf benchpf.o pflayer.o

testhash: testhash.o pflayer.o
	gcc $(CFLAGS) -o testhash testhash.o pflayer.o

slots: slotted_bench

slotted_bench: slotted_bench.o pflayer.o $(SLOT_OBJ)
	gcc $(CFLAGS) -o slotted_bench slotted_bench.o pflayer.o $(SLOT_OBJ)

slotted_bench.o: $(HDR) $(SLOT_HDR)

$(SLOT_OBJ): $(SLOT_HDR) $(HDR)

$(OBJ): $(HDR)

benchpf.o: $(HDR)

testhash.o: $(HDR)

testpf.o: $(HDR)

lint: 
	lint $(SRC)

install: pflayer.o

