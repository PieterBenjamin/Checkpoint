
CCOMP = gcc -Wall -g -std=c11

DS = DataStructs/hashtable.o DataStructs/LinkedList.o

all: linkedlist hashtable checkpoint exec clean

debug: linkedlist hashtable checkpoint_debug exec clean

linkedlist: DataStructs/LinkedList.h DataStructs/LinkedList.c
	$(CCOMP) -c DataStructs/LinkedList.c

hashtable: DataStructs/HashTable.c DataStructs/HashTable.h DataStructs/LinkedList.h
	$(CCOMP) -c DataStructs/HashTable.c

checkpoint: checkpoint.c checkpoint.h
	$(CCOMP) -c $< 

checkpoint_debug: checkpoint.c checkpoint.h
	$(CCOMP) -c -DDEBUG_ $<

exec: checkpoint.o $(DS)
	$(CCOMP) -o Checkpoint checkpoint.o hashtable.o linkedlist.o


clean:
	rm *.o