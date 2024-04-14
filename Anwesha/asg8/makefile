all: master mmu process sched

run: master mmu process sched
	./master

master: Master.c
	gcc -o master Master.c

mmu: mmu.c
	gcc -o mmu mmu.c

process: process.c
	gcc -o process process.c

sched: sched.c
	gcc -o sched sched.c

clean:
	rm -f master mmu process sched