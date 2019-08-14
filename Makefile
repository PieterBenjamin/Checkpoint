
CCOMP = gcc -Wall -g -std=c11

DS = hashtable.o LinkedList.o

all: linkedlist hashtable checkpoint exec 

debug: linkedlist hashtable checkpoint_debug exec 

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
	$(RM) ./DataStructs/*.o
	$(RM) ./*.o