// addrspace.cc
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------
AddrSpace::AddrSpace(OpenFile *executable)
{
	NoffHeader noffH;
	unsigned int i, size;
	addrSpaceUsage = new int();
	*addrSpaceUsage = 1;

	#ifdef VM
		zeroOut = new char[PageSize];
		for (int index = 0; index <= PageSize; index++) {
			zeroOut[index] = 0;
		}
		swapCounter = 0;
		TLBCounter = 0;
	#endif

	executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
	if ((noffH.noffMagic != NOFFMAGIC) && (WordToHost(noffH.noffMagic) == NOFFMAGIC)){
		SwapHeader(&noffH);
	}
	DEBUG('A', "Checking if program is executable\n");
	ASSERT(noffH.noffMagic == NOFFMAGIC);
	// Calculate address space size
	size = noffH.code.size + noffH.initData.size + noffH.uninitData.size + UserStackSize;	// we need to increase the size
						 // to leave room for the stack
	numPages = divRoundUp(size, PageSize);
	size = numPages * PageSize;

	#ifndef VM
		ASSERT(numPages <= NumPhysPages);		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory
	#endif

	DEBUG('A', "Initializing address space, num pages %d, size %d\n",numPages, size);
	pageTable = new TranslationEntry[numPages];

	#ifdef VM
		unsigned int initDataPages = divRoundUp(noffH.code.size+noffH.initData.size, PageSize);
	#endif
	// Initiate the page table, ask the bit map to find an available page in main memory
	// Only one thread can be looking for memory at a time, so use the mutex
	bitMSem->P();
	for (i = 0; i < numPages; i++){
		pageTable[i].virtualPage = i;
	#ifdef VM
		pageTable[i].physicalPage = -1;
		pageTable[i].valid = false;
		if (i < initDataPages) {
			pageTable[i].inExec = true;
		}
		else {
			pageTable[i].inExec = false;
		}
	#else
		int physicalPage;
		physicalPage = memMap->Find();
		for (int in = 0; in < NumPhysPages; in++)
		{
			DEBUG('t', "%d ", memMap->Test(in));
		}
		DEBUG('t', "  %d\n", physicalPage);
		if (physicalPage == -1) {
			printf("No available space for the program : ABORT\n");
			ASSERT(false);
		}
		else
		{
			pageTable[i].physicalPage = physicalPage;
			DEBUG('A', "Assigned physical page is %d for virtual page %d\n", physicalPage, i);
		}
		bzero(&(machine->mainMemory[PageSize * physicalPage]), PageSize);	// Initialization of the reserved physical page
		memMap->Mark( physicalPage );		// Marks the page as used
		pageTable[i].valid = true;
	#endif
		pageTable[i].use = false;
		pageTable[i].dirty = false;
		pageTable[i].readOnly = false;
		DEBUG ('A', "Assigned VP = %d -> PP = %d\n", pageTable[i].virtualPage, pageTable[i].physicalPage);
	}
	bitMSem->V();
	// Release the mutex

	#ifndef VM
		unsigned int code_data_size, inFileAddress, mainMemory_address;

		code_data_size = noffH.code.size + noffH.initData.size;	//Calculate how big is the code and data size together
		inFileAddress = noffH.code.inFileAddr;

		// Does the mapping of the code and data segments to main memory
		for (unsigned int virtualAddr = 0; virtualAddr < numPages && (virtualAddr * PageSize) <= code_data_size; virtualAddr++) {
			mainMemory_address = (pageTable[virtualAddr].physicalPage) * PageSize;
			DEBUG('A', "Assigned physical address is %d for virtual address %d\n", mainMemory_address, virtualAddr*PageSize);
			executable->ReadAt(&(machine->mainMemory[mainMemory_address]), PageSize, inFileAddress);
			inFileAddress += PageSize;
		}
	#endif
	//Put the code pages as Read-Only
	codeSize = noffH.code.size / PageSize;
	for (int indexRO = 0; indexRO < codeSize; indexRO++)
	{
		pageTable[indexRO].readOnly = true;
		DEBUG('A', "Page Table #%d assigned as Read-Only\n", indexRO);
	}

}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// Constructor for children.
// Copying father's AddrSpace.
//----------------------------------------------------------------------
AddrSpace::AddrSpace(AddrSpace *space, int fatherCodePageSize, int *fatherAddrUsage) {
#ifdef VM
	zeroOut = new char[PageSize];
	for (int i = 0; i <= PageSize; i++) {
		zeroOut[i] = 0;
	}

	swapCounter = 0;

	TLBCounter = 0;
#endif

	int physicalPage;
	numPages = space->numPages;
	pageTable = new TranslationEntry[numPages];
	codeSize = fatherCodePageSize;
	addrSpaceUsage = fatherAddrUsage;
	(*addrSpaceUsage)++;

	unsigned int i = 0;
    for (i = 0; i < numPages - StackPages; i++) {	// Make the thread share the data and code memory but not the stack
		pageTable[i] = space->pageTable[i];

    }

	// Reserve the stack memory, ask the bit map to find an available page in main memory
	// Only one thread can be looking for memory at a time, so use the mutex
	bitMSem->P();
	for (int stackIndex = 0; stackIndex < StackPages; stackIndex++){
		pageTable[i].virtualPage = i;
	#ifdef VM
		pageTable[i].physicalPage = -1;
		pageTable[i].valid = false;
		pageTable[i].inExec = false;
	#else
		physicalPage = memMap->Find();
		for (int in = 0; in < NumPhysPages; in++)
			DEBUG('t', "%d ", memMap->Test(in));
		DEBUG('t', "  %d\n", physicalPage);
		if (physicalPage == -1) {
			printf("No available space for the program : ABORT\n");
			ASSERT(false);
		} else {
			pageTable[i].physicalPage = physicalPage;
			DEBUG('A', "Assigned physical page is %d for virtual page %d\n",physicalPage, i);
		}
		bzero(&(machine->mainMemory[PageSize * physicalPage]), PageSize);	// Initialization of the reserved physical page
		memMap->Mark( physicalPage );		// Marks the page as used
		pageTable[i].valid = true;
	#endif
		pageTable[i].use = false;
		pageTable[i].dirty = false;
		pageTable[i].readOnly = false;
		i++;
	}
	bitMSem->V();

	for (int indexRO = 0; indexRO < codeSize; indexRO++) {
		pageTable[indexRO].readOnly = true;
		DEBUG('A', "Page Table #%d assigned as Read-Only\n", indexRO);
	}

}


//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  
//----------------------------------------------------------------------
AddrSpace::~AddrSpace()
{
	(*addrSpaceUsage)--;
	unsigned int i;
	if (*addrSpaceUsage == 0) {
	i = 0;
	delete addrSpaceUsage;
	}
	else {
		i = numPages - StackPages;
	}
	for (; i < numPages ; i++) {
		#ifdef VM
			if (pageTable[i].valid) {
				DEBUG ('A', "Deleting physical page %d\n", pageTable[i].physicalPage);
				memMap->Clear(pageTable[i].physicalPage);
				bzero(&(machine->mainMemory[PageSize * pageTable[i].physicalPage]), PageSize);	// Initialization of the reserved physical page
			}
			else {
				if (pageTable[i].dirty) {
					swapBitMap->Clear(pageTable[i].physicalPage);
					DEBUG ('A', "Deleting swap page %d\n", pageTable[i].physicalPage);
				}
			}
		#else
			DEBUG ('A', "Deleting physical page %d\n", pageTable[i].physicalPage);
			memMap->Clear(pageTable[i].physicalPage);
		#endif
	}
	#ifdef VM
		delete zeroOut;
	#endif
	delete pageTable;
}

void AddrSpace::copyName(const char* name) {
	filename = new char[strlen(name) + 1];
	std::copy(name, name + strlen(name), filename);
	filename[strlen(name)] = 0;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------
void AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
        machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//----------------------------------------------------------------------
void AddrSpace::SaveState(){
#ifdef VM
    for(int i=0; i<TLBSize; i++){ //index TLB for context switch
        if(machine->tlb[i].valid){
            pageTable[machine->tlb[i].virtualPage].dirty = machine->tlb[i].dirty; //Keep dirty bits from one context on the other
            machine->tlb[i].valid = false; //Not supposed to be executed if not in current context
        }
    }
#endif
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------
void AddrSpace::RestoreState()
{
#ifdef VM
    for(int i=0; i<TLBSize; i++){
        machine->tlb[i].valid = false;
    }
#else
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
#endif
}

#ifdef VM
//----------------------------------------------------------------------
// AddrSpace::load
// Decides whether to load a page in the TLB when a page fault is detected.
//----------------------------------------------------------------------
void AddrSpace::load(int pageFault)
{
	int tlbIndex = 0;
    //Checks for any free space on TLB
    for(int i=0; i < TLBSize; i++){
        if(machine->tlb[i].valid){
            tlbIndex++;
        }
        else{
            break;
        }
    }
	if (tlbIndex >= TLBSize) { //If there's free space on TLB
		tlbIndex = goToTLB();
		pageTable[machine->tlb[tlbIndex].virtualPage].dirty = machine->tlb[tlbIndex].dirty;
		DEBUG ('a', "Page #%d has been saved on page table\n", tlbIndex);
	}
	if (!pageTable[pageFault].valid) { //Choose victim for page fault
		chooseVictim((PageType)getSection(&pageTable[pageFault]), pageTable[pageFault].dirty, false, &pageTable[pageFault]);
		pageTable[pageFault].use = true;
		stats->numPageFaults++; 
	}
    machine->tlb[tlbIndex].valid = pageTable[pageFault].valid;
    machine->tlb[tlbIndex].dirty = pageTable[pageFault].dirty;
    machine->tlb[tlbIndex].physicalPage = pageTable[pageFault].physicalPage;
	machine->tlb[tlbIndex].virtualPage = pageTable[pageFault].virtualPage;
	machine->tlb[tlbIndex].readOnly = pageTable[pageFault].readOnly;

	DEBUG ('a', "Page #%d has been loaded in TLB\n", pageFault);
}

//----------------------------------------------------------------------
// AddrSpace::goToTLB
// Finds a place in the TLB for a specific page when a page fault is detected.
// Implementation based on Second Chance Algorithm
//----------------------------------------------------------------------
int AddrSpace::goToTLB()
{
	bool selected = false;
	int passedPages;
	while (!selected) {
		passedPages = 0;
		// Look for (0,0) pages
		while (passedPages < TLBSize && !selected) {
			if (!machine->tlb[TLBCounter].use && !machine->tlb[TLBCounter].dirty){
                selected = true;
            }
			else{ 
				TLBCounter = (TLBCounter + 1) % TLBSize;
            }
			passedPages++;
		}
		if (!selected) {
			// If no (0,0) look for (0,1) pages
			passedPages = 0;
			while (passedPages < TLBSize && !selected) {
				if (!machine->tlb[TLBCounter].use && machine->tlb[TLBCounter].dirty){
					selected = true;
                }
				else{ 
					TLBCounter = (TLBCounter + 1) % TLBSize;
                }
				passedPages++;
			}
			if (!selected) {
				// If no pages with use bit == 0
				// use bit = 0 and start search again
				passedPages = 0;
				while (passedPages < TLBSize) {
					machine->tlb[TLBCounter].use = false;
					TLBCounter = (TLBCounter + 1) % TLBSize;
					passedPages++;
				}
			}
		}
	}
	return TLBCounter;
}


//----------------------------------------------------------------------
// AddrSpace::getSection
// Get section case
//----------------------------------------------------------------------
int AddrSpace::getSection(TranslationEntry* entry) {
	if (entry->readOnly) {
		return TEXT;
	} else if (entry->inExec) {
		return INITDATA; //Initialized data
	} else {
		return NONINITSTACK; //Unitialized data/stack
	}
}

//----------------------------------------------------------------------
// AddrSpace::chooseVictim
// Process page fault: choose a victim for SWAP depending on 
// page fault's page type
//----------------------------------------------------------------------
void AddrSpace::chooseVictim (PageType type, bool dirty, bool victim, TranslationEntry* pageEntry) {
	DEBUG('a', "El tipo es %d y es victima = %d\n", type, victim);
	int physicalPage = -1;
	switch (type) {
		case TEXT: //Page is readOnly (user code)
			if (dirty) {
				printf("Code segment is dirty\n");
				ASSERT(false);
			} else {
				if (victim) {
					ZeroOutOutPageMem(pageEntry);
				} else {
					physicalPage = findMem();
					pageEntry->physicalPage = physicalPage;
					SwapExec(pageEntry);
				}
			}
		break;

		case INITDATA: //Page contains initialized variables
			if (dirty) {
				if (victim) {
					SwapOut(pageEntry);
				} else {
					physicalPage = findMem();
					SwapIn(pageEntry, physicalPage);
				}
			} else {
				if (victim) {
					ZeroOutOutPageMem(pageEntry);
				} else {
					physicalPage = findMem();
					pageEntry->physicalPage = physicalPage;
					SwapExec(pageEntry);
				}
			}
		break;

		case NONINITSTACK: //Page contains uninitialized variables, which mostly belong to stack memory
			if (dirty) {
				if (victim) {
					SwapOut(pageEntry);
				} else {
					physicalPage = findMem();
					SwapIn(pageEntry, physicalPage);
				}
			} else {
				if (victim) {
					ZeroOutOutPageMem(pageEntry);
				} else {
					physicalPage = findMem();
					pageEntry->physicalPage = physicalPage;
					ZeroOutInPageMem(pageEntry);
				}
			}
		break;
	}
}

//----------------------------------------------------------------------
// AddrSpace::findMem
// Finds a physicalPage address for SWAP
// Used on chooseVictim
//----------------------------------------------------------------------
int AddrSpace::findMem () {
	int physicalPage = memMap->Find();
	if (physicalPage == -1) {
		physicalPage = goToMemory();
	}
	return physicalPage;
}

//----------------------------------------------------------------------
// AddrSpace::goToMemory
// Finds VM space for victim
//----------------------------------------------------------------------
int AddrSpace::goToMemory () {
	while (ipt[scMem_counter]->use) {
		ipt[scMem_counter]->use = false;
		scMem_counter = (scMem_counter + 1) % NumPhysPages;
	}
	DEBUG('A', "El indice de sc de memoria es: %d\n", scMem_counter);
	PageType section = (PageType)getSection(ipt[scMem_counter]);
	chooseVictim(section, ipt[scMem_counter]->dirty, true, ipt[scMem_counter]);
	return scMem_counter;
}

//----------------------------------------------------------------------
// AddrSpace::ZeroOutInPageMem
// Fills incoming page with zeros
//----------------------------------------------------------------------
void AddrSpace::ZeroOutInPageMem (TranslationEntry* inPage) {
	inPage->valid = true;
	int physicalPage = inPage->physicalPage;
	ipt[physicalPage] = inPage;
	
	bzero(&(machine->mainMemory[physicalPage*PageSize]), PageSize);
}

//----------------------------------------------------------------------
// AddrSpace::SwapIn
// Stores page in physical memory. SWAP in
// Cleans old address by zeroing it out
//----------------------------------------------------------------------
void AddrSpace::SwapIn (TranslationEntry* inPage, int physicalPage) {

	ipt[physicalPage] = inPage;
	int swapPage = inPage->physicalPage;
	if (swapPage < 0 || swapPage > swapSize || !swapBitMap->Test(swapPage)) {
		printf("Invalid swap entry found on assigned Swap Page, should not happen.\n");
		ASSERT(false);
	}
	inPage->physicalPage = physicalPage;
	swap->ReadAt(&(machine->mainMemory[physicalPage*PageSize]), PageSize, swapPage * PageSize);
	DEBUG('a', "Swap puts vp #%d in mem from pos: %d, to pp #%d\n", inPage->virtualPage, swapPage, physicalPage);
	swapBitMap->Clear(swapPage);
	swap->WriteAt(zeroOut, PageSize, swapPage * PageSize);

	inPage->valid = true;
}

//----------------------------------------------------------------------
// AddrSpace::SwapExec
// Stores page containing executable data in physical memory. SWAP in
//----------------------------------------------------------------------
void AddrSpace::SwapExec (TranslationEntry* inPage) {
	OpenFile fileExec = *(fileSystem->Open(filename));
	int physicalPage = inPage->physicalPage;
	ipt[physicalPage] = inPage;
	DEBUG('a', "Assigned physical page is %d for virtual page %d\n", inPage->physicalPage, inPage->virtualPage);

	NoffHeader noffH;

	fileExec.ReadAt((char *)&noffH, sizeof(noffH), 0);
	int inFileAddress = noffH.code.inFileAddr + (inPage->virtualPage * PageSize);
	int mainMemory_address = (inPage->physicalPage) * PageSize;
	fileExec.ReadAt(&(machine->mainMemory[mainMemory_address]), PageSize, inFileAddress);

	inPage->valid = true;
}

//----------------------------------------------------------------------
// AddrSpace::SwapOut
// Frees up physical address. SWAP out.
//----------------------------------------------------------------------
void AddrSpace::SwapOut (TranslationEntry* outPage) {
	swapCounter++;
	int swapPage = swapBitMap->Find();
	if (swapPage < 0) {
		printf("Not enough space in the swap, the program aborts.\n");
		ASSERT(false);
	}

	int physicalPage = outPage->physicalPage;
	DEBUG('a', "swap takes out vp #%d to position %d from pp #%d\n", outPage->virtualPage, swapPage, physicalPage);
	outPage->physicalPage = swapPage;
	outPage->valid = false;

	for (int i = 0; i < TLBSize; i++) {
		if (machine->tlb[i].virtualPage == outPage->virtualPage && &(pageTable)[outPage->virtualPage] == outPage) {
			machine->tlb[i].valid = false;
		}
	}

	swap->WriteAt(&(machine->mainMemory[physicalPage*PageSize]), PageSize, swapPage * PageSize);
	
	bzero(&(machine->mainMemory[PageSize * physicalPage]), PageSize);	// Initialization of the reserved physical page
	ipt[physicalPage] = 0;
}

//----------------------------------------------------------------------
// AddrSpace::ZeroOutOutPageMem
// Fills outgoing page with zeros
//----------------------------------------------------------------------
void AddrSpace::ZeroOutOutPageMem (TranslationEntry* outPage) {
	outPage->valid = false;
	for (int i = 0; i < TLBSize; i++) {
		if (machine->tlb[i].virtualPage == outPage->virtualPage && &(pageTable)[outPage->virtualPage] == outPage) {
			machine->tlb[i].valid = false;
		}
	}

	int physicalPage = outPage->physicalPage;
	bzero(&(machine->mainMemory[PageSize * physicalPage]), PageSize);	// Initialization of the reserved physical page
	ipt[physicalPage] = 0;
}
#endif