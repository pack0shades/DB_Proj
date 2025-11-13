a.out : am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o ../pflayer/pflayer.o main.o amscan.o amprint.o
	cc am.o amfns.o amsearch.o aminsert.o  amstack.o amglobals.o ../pflayer/pflayer.o main.o amscan.o amprint.o 

CFLAGS_AM=-std=gnu89 -Wno-implicit-int -Wno-implicit-function-declaration -Wno-builtin-declaration-mismatch

amlayer.o : am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o amscan.o amprint.o
	ld -r am.o amfns.o amsearch.o aminsert.o  amstack.o amglobals.o amscan.o amprint.o  -o amlayer.o

am.o : am.c am.h pf.h
	cc $(CFLAGS_AM) -c am.c

amfns.o : amfns.c am.h pf.h
	cc $(CFLAGS_AM) -c amfns.c

amsearch.o : amsearch.c am.h pf.h
	cc $(CFLAGS_AM) -c amsearch.c

aminsert.o : aminsert.c am.h pf.h
	cc $(CFLAGS_AM) -c aminsert.c

amscan.o : amscan.c am.h pf.h
	cc $(CFLAGS_AM) -c amscan.c

amstack.o : amstack.c am.h pf.h
	cc $(CFLAGS_AM) -c amstack.c

amglobals.o : amglobals.c am.h
	cc $(CFLAGS_AM) -c amglobals.c

amprint.o : amprint.c am.h pf.h 
	cc $(CFLAGS_AM) -c amprint.c
	
main.o : main.c am.h pf.h 
	cc $(CFLAGS_AM) -c main.c

indexbench: indexbench.o amlayer.o ../pflayer/pflayer.o ../pflayer/slotted.o
	cc indexbench.o amlayer.o ../pflayer/pflayer.o ../pflayer/slotted.o -o indexbench

indexbench.o: indexbench.c am.h ../pflayer/slotted.h
	cc $(CFLAGS_AM) -c indexbench.c

benchindex: indexbench
	CSV_OUT=../pflayer/index_stats.csv CSV_HEADER=1 ./indexbench ../pflayer/students.spf student 1
	CSV_OUT=../pflayer/index_stats.csv ./indexbench ../pflayer/students.spf student 0

indexplots: benchindex plot_index_stats.py
	python3 plot_index_stats.py ../pflayer/index_stats.csv index
	@echo "Plots written:"
	@ls -1 index_*.png | sed -n '1,999p'


