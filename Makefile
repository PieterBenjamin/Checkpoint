# Copyright 2019, Pieter Benjamin, pieter0benjamin@gmail.com

CCOMP = gcc -Wall -g -std=c11

DS = HashTable.o LinkedList.o

all: linkedlist hashtable checkpoint exec clean 

debug: linkedlist hashtable checkpoint_debug exec clean 

linkedlist: DataStructs/LinkedList.h DataStructs/LinkedList.c
	$(CCOMP) -c DataStructs/LinkedList.c

hashtable: DataStructs/HashTable.c DataStructs/HashTable.h DataStructs/LinkedList.h
	$(CCOMP) -c DataStructs/HashTable.c

checkpoint: checkpoint*
	$(CCOMP) -c $< 
	$(CCOMP) -c checkpoint_tree.c
	$(CCOMP) -c checkpoint_filehandler.c


checkpoint_debug: checkpoint*
	$(CCOMP) -c -DDEBUG_ $<
	$(CCOMP) -c -DDEBUG_ checkpoint_tree.c
	$(CCOMP) -c -DDEBUG_ checkpoint_filehandler.c


exec: checkpoint.o checkpoint_tree.o checkpoint_filehandler.o $(DS)
	$(CCOMP) -o Checkpoint checkpoint.o checkpoint_tree.o checkpoint_filehandler.o $(DS)


clean:
	$(RM) ./DataStructs/*.o
	$(RM) ./*.o
