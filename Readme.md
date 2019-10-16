# Checkpoint

Git is an incredibly powerful tool, but a supercomputer is [not needed to run doom](https://knowyourmeme.com/memes/it-runs-doom). For this project, I wanted to create  
a version control system for files you just want to keep on your computer. Some standalone files do not merit an  
entire repo to themselves, but being able to save a checkpoint you could come back to would be handy. This program  
addresses that problem using the appropriately versatile language C.


Some DataStructs/ files come from my [Systems Programming class](https://courses.cs.washington.edu/courses/cse333/19su/) and reflect my own original work (in addition to the skeleton that was provided to me by the course)  

The usage is as follows:
```
Checkpoint: copyright 2019, Pieter Benjamin

Usage: ./checkpoint  <option>
Where <filename> is an absolute or relative pathway
to the file you would like to checkpoint.

options:
	create <source file name> <checkpoint name>
	back   <source file name>
	swapto <source file name> <checkpoint name>
	delete <source file name>
		NOTE: "delete" does not remove your source file,
		      but will remove all trace of it from the
		      current checkpoint log for this directory.n 
  list (lists all Checkpoints for the current dir)

PLEASE NOTE:	- Checkpoints will be stored in files labeled with
	  the name of the checkpoint  you provide. If you
	  provide the name of a preexisting file (checkpoint),
	  it will NOT be overwritten.

	- Delete is irreversible.
 ```
![Alt-Text](https://github.com/PieterBenjamin/Checkpoint/blob/master/imgs/use%20example.png)
