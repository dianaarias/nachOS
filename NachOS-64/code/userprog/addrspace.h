// addrspace.h
//	Data structures to keep track of executing user programs
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"

#define UserStackSize		1024 	// increase this as necessary!
#define StackPages      UserStackSize/PageSize

class AddrSpace {
  public:
    AddrSpace(OpenFile *executable);	// Create an address space,
					                            // initializing it with the program
					                            // stored in the file "executable"
    AddrSpace(AddrSpace *fatherSpace, int fatherPageSize, int *fatherAddr); //Constructor copia para fork
    ~AddrSpace();			                   // De-allocate an address space

    void InitRegisters();		// Initialize user-level CPU registers,
					                  // before jumping to user code

    void SaveState();			// Save/restore address space-specific
    void RestoreState();		// info on a context switch
    
    //For fork
    int codeSize;
    int *addrSpaceUsage;

    //Get for private atribute pageTable
    TranslationEntry* getPageTable(){
      return pageTable;
    }

#ifdef VM
    int swapCounter;  //Counter for swap
    int TLBCounter;   //Counter to index TLB
    char* zeroOut; //This array can be used to clean pages by filling them with zeros

    //Checks when to put page on TLB and does so
    void load(int pageFault); //Catch page fault
    int goToTLB();            //Put page on TLB


#endif

  private:
    TranslationEntry *pageTable;	// Assume linear page table translation for now!
    unsigned int numPages;		    // Number of pages in the virtual address space
};

#endif // ADDRSPACE_H
