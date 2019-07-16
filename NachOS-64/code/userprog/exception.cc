// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.
#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "synch.h"
#include "nachostable.h"


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#define BUFFERLENGTH 124
int exitStatus = 0;

void returnFromSystemCall(){
    int pc, npc;
    pc = machine->ReadRegister(PCReg);
    npc = machine->ReadRegister(NextPCReg);
    machine->WriteRegister(PrevPCReg, pc); // PrevPC <- PC
    machine->WriteRegister(PCReg, npc); // PC <- NextPC
    machine->WriteRegister(NextPCReg, npc+4); // NextPC <- NextPC + 4
}

void Nachos_Halt() {             // System call 0

        DEBUG('a', "Shutdown, initiated by user program.\n");
        interrupt->Halt();

}       // Nachos_Halt

/* This user program is done (status = 0 means exited normally). */ //REVISAR
void Nachos_Exit() {           // System call 1 //REVISAR
    int status = machine->ReadRegister(4);
    exitStatus = status;
    DEBUG ('s', "Process exited with code %d\n", status);

    Semaphore* waitT = (Semaphore*)(currentThread->semtable->getSemaphore(0));

  	for (int i = 0;  i< currentThread->semtable->waitingThreads; i++) {
  		waitT->V();
  	}
  	DEBUG( 's', "Deleting AddrSpace\n" );
  	delete currentThread->space;
  	currentThread->nachostable->delThread();
  	currentThread->semtable->delThread();
  	if ( currentThread->nachostable->getUsage() == 0) {
  		DEBUG( 's', "Deleting open file table\n" );
  		delete currentThread->nachostable;
  	}
  	if ( currentThread->nachostable->getUsage() == 0) {
  		DEBUG( 's', "Deleting semaphore table\n");
  		delete currentThread->semtable;
  	}
  	if (runningThreadTable->exists( currentThread->threadID ) ) {
  		runningThreadTable->Destroy(currentThread->threadID);
  	}

    currentThread->Finish();
    returnFromSystemCall();
}       // Nachos_Exit

/* Run the executable, stored in the Nachos file "name", and return the
 * address space identifier
 */
void NachosExec(void *file)
{
    AddrSpace *space;
    space = currentThread->space;

    //Initialize registers and then load the pages table
    space->InitRegisters();
    space->RestoreState();

    //Return adress is the same as calling thread
    machine->WriteRegister(RetAddrReg, 4);
    machine->WriteRegister(PCReg, (long)file);
    machine->WriteRegister(NextPCReg, (long)file+4);
    // run the program
    machine->Run(); //Jump to user program
    ASSERT(false);
}


void Nachos_Exec() {           // System call 2
  DEBUG( 's', "Entering Exec System call\n" );
	int addrs = machine->ReadRegister( 4 );
	int buffer;
  int buffIndex;
	char name[BUFFERLENGTH];

	machine->ReadMem(addrs, 1, &buffer);
	/* Reads the name in memory until it finds a null character (the end of the string) */
	for (buffIndex = 0; buffer != '\0'; buffIndex++) {
		name[buffIndex] = (char) buffer;
		addrs++;
		machine->ReadMem(addrs, 1, &buffer);
	}

	/* Fills the rest of name buffer with null */
	for (; buffIndex < BUFFERLENGTH; buffIndex++) {
		name[buffIndex] = '\0';
	}

	DEBUG('s', "The exec buffer is %s", name);
	OpenFile* executableFile = fileSystem->Open(name);

	if ( executableFile == NULL) {
		printf("Invalid file '%s' for execution : EXECUTE \n", name);
		machine->WriteRegister(2, -1);
	} else {
		Thread * newExecutingThread = new Thread( "Exec Process" );
		int threadID = runningThreadTable->Create(newExecutingThread);
		if (threadID == -1) {
			printf("No available space for a new Thread to run\n");
			ASSERT(false);
		}
		newExecutingThread->threadID = threadID;
		machine->WriteRegister(2, threadID);

		newExecutingThread->space = new AddrSpace( executableFile );
		newExecutingThread->space->copyName(name);
		DEBUG('s', "Initializing the thread wait semaphore\n");
		newExecutingThread->semtable->beginSemaphoreTable((long)new Semaphore("Waiting Threads Semaphore", 0));
		DEBUG('s', "New process created with pid = %d\n", threadID);
		//Sends the address 0 because any code segment starts at the Virtual Address 0
		newExecutingThread->Fork( NachosExec, 0 );
	}

	returnFromSystemCall();	// This adjust the PrevPC, PC, and NextPC registers

	DEBUG( 's', "Exiting Exec System call\n" );
}       // Nachos_Exec

/* Only return once the the user program "id" has finished.
 * Return the exit status.
 */
void Nachos_Join() {           // System call 3
    int threadID = machine->ReadRegister(4);

	   DEBUG('s', "Entering Join, with ID = %d\n", threadID);
	if (runningThreadTable->exists(threadID)) {
		Thread* newWaitingThread = runningThreadTable->getThread(threadID);
		DEBUG('s', "The thread is now waiting for %d to finish\n", threadID);
		Semaphore* waitSemaphore = (Semaphore*)(newWaitingThread->semtable->getSemaphore(0));
		newWaitingThread->semtable->waitingThreads++;
		waitSemaphore->P();

		DEBUG('s', "The thread received a signal from %d, exit status is %d\n", threadID, exitStatus);
		machine->WriteRegister(2, exitStatus);
	} else {
		printf("Unvalid Thread ID %d : JOIN\n", threadID);
		machine->WriteRegister(2, -1);
	}

  returnFromSystemCall();
}       // Nachos_Join

/* Create a Nachos file, with "name" */
void Nachos_Create() {           // System call 4
    int reg4 = machine->ReadRegister(4);
    char file_name[BUFFERLENGTH] = {};
    int buffer = 0;

    //Reads in memory until end of name
    machine->ReadMem(reg4, 1, &buffer);
    int i = 0;
    while(buffer != '\0'){
        file_name[i] = (char) buffer;
        reg4++;
        i++;
        machine->ReadMem(reg4, 1, &buffer);
    }

    int handle = creat(file_name, S_IRWXG);
    if(handle != -1){
        close(handle);
        machine->WriteRegister(2, handle);
    }
    else{
        machine->WriteRegister(2,-1);
    }

    returnFromSystemCall();
}       // Nachos_Create

/* Open the Nachos file "name", and return an "OpenFileId" that can
 * be used to read and write to the file.
 */
void Nachos_Open() {            // System call 5
    int reg4 = machine->ReadRegister(4);
    char file_name[BUFFERLENGTH] = {};
    int buffer = 0;

    //Reads in memory until end of name
    machine->ReadMem(reg4, 1, &buffer);
    int i = 0;
    while(buffer != '\0'){
        file_name[i] = (char) buffer;
        reg4++;
        i++;
        machine->ReadMem(reg4, 1, &buffer);
    }

    // Use NachosOpenFilesTable class to create a relationship
	// between user file and unix file
    int UnixHandle = open(file_name, O_RDWR);
    if(UnixHandle != -1){
        int NachosHandle = currentThread->nachostable->Open(UnixHandle);
        machine->WriteRegister(2, NachosHandle);
    }
	// Verify for errors
    else{
        printf("Cannot open file\n");
        machine->WriteRegister(2,-1);
    }

    returnFromSystemCall();		// Update the PC registers

}       // Nachos_Open

/* Read "size" bytes from the open file into "buffer".
 * Return the number of bytes actually read -- if the open file isn't
 * long enough, or if it is an I/O device, and there aren't enough
 * characters to read, return whatever is available (for I/O devices,
 * you should always wait until you can return at least one character).
 */
void Nachos_Read() {           // System call 6
    int addrs = machine->ReadRegister(4);  // Read address
    int size = machine->ReadRegister(5);  // Read size to read
    int wBuffer;
    OpenFileId id = machine->ReadRegister(6);
    char* buffer = new char[size];
    ssize_t read_result = 0;

    bool error =false;

    consoleMutex->P();
    switch (id) {
        case ConsoleInput:
          read_result = read(id, buffer, size);
          stats->numConsoleCharsRead++;
          machine->WriteRegister(2, read_result);
        break;
        case ConsoleOutput:
          machine->WriteRegister(2, -1);
          printf( "Error: user can not read from output\n" );
          error = true;
        break;
        case ConsoleError:
        		error = true;
            printf("ReadError");
        break;
        default:
            if(currentThread->nachostable->isOpened(id)){
                int UnixHandle = currentThread->nachostable->getUnixHandle(id);
                read_result = read(UnixHandle, buffer, size);
                buffer[read_result] =  '\0';
                machine->WriteRegister(2, read_result);
                stats->numDiskReads++;
            }
            else{
                machine->WriteRegister(2, -1);
            }
        break;
    }
    if (!error)
    {
  		/* Reads the chars in memory specified by the user */
  		wBuffer = (int)buffer[0];
  		machine->WriteMem(addrs, 1, wBuffer);
  		addrs++;
	  }
    delete buffer;
    consoleMutex->V();

    returnFromSystemCall();

}       // Nachos_Read

/* Write "size" bytes from "buffer" to the open file. */
void Nachos_Write() {           // System call 7

    int reg4 = machine->ReadRegister(4);
    int size = machine->ReadRegister(5);	    // Read  size to write
    OpenFileId id = machine->ReadRegister(6);	// Read file descriptor
    char buffer[size] = {};
    int letter_buffer = 0;

    // buffer = Read data from address given by user;
    machine->ReadMem(reg4, 1, &letter_buffer);
    for(int i=0; i<size; i++){
        buffer[i] = (char) letter_buffer;
        reg4++;
        machine->ReadMem(reg4, 1, &letter_buffer);
    }

	// Need a semaphore to synchronize access to console
	consoleMutex->P();
	switch (id) {
		case  ConsoleInput:	// User could not write to standard input
			machine->WriteRegister( 2, -1 );
			break;

    case  ConsoleOutput:
			buffer[ size ] = {};
			printf( "%s", buffer );
      if (buffer[size-1] != '\n')
      {
        fflush(stdout);
      }
      stats->numConsoleCharsWritten++;
		break;

    case ConsoleError:	// This trick permits to write integers to console
			printf( "%d\n", machine->ReadRegister( 4 ) );
		break;

    default:	// All other opened files
			// Verify if the file is opened, if not return -1 in r2
			// Get the unix handle from our table for open files
			// Do the write to the already opened Unix file
			// Return the number of chars written to user, via r2
      if(currentThread->nachostable->isOpened(id)){
        int UnixHandle = currentThread->nachostable->getUnixHandle(id);
        ssize_t write_result = write(UnixHandle, buffer, size);
        machine->WriteRegister(2, write_result);
        stats->numDiskWrites++;
      }
      else{
            machine->WriteRegister(2, -1);
          }
		break;
	}
	// Update simulation stats, see details in Statistics class in machine/stats.cc
	consoleMutex->V();

  returnFromSystemCall();		// Update the PC registers

}       // Nachos_Write

/* Close the file, we're done reading and writing to it. */
void Nachos_Close() {           // System call 8
    OpenFileId id = machine->ReadRegister(4); //REVISAR

    if(currentThread->nachostable->isOpened(id)){
        int NachosHandle = currentThread->nachostable->Close(id); // NachOS close
        int UnixHandle = currentThread->nachostable->getUnixHandle(id);
        int close_result = close(UnixHandle); // Unix close

        // 0 on success, -1 on error
        if(NachosHandle == -1 && close_result == -1){
            machine->WriteRegister(2, -1);
        }
        else{
            machine->WriteRegister(2, 0);
        }
    }
    else{
        machine->WriteRegister(2, -1);
    }
    returnFromSystemCall();
}       // Nachos_Close

/* Fork a thread to run a procedure ("func") in the *same* address space
  as the current thread.
*/

void ForkVoidFunction(void *registerPointer)
 {
   //Crea un nuevo AddrSpace para el hacer el fork
  AddrSpace* newSpace;
  newSpace = currentThread->space;
  newSpace->InitRegisters();    // set initial reg
  newSpace->RestoreState();     // load page table reg
  //Actualiza los registros para el Fork
  machine->WriteRegister(RetAddrReg,4);
  machine->WriteRegister(PCReg,(long)registerPointer);
  machine->WriteRegister(NextPCReg,(long)registerPointer+4);
  //Se ejecuta el nuevo fork en userProg.
  machine->Run();
  ASSERT(false);
}

void Nachos_Fork() {            // System call 9
    //Se crea el nuevo hilo por el fork
    Thread* forkingT = new Thread("Forking Thread");
    //Le provee al nuevo hilo el copia del espacio del padre
    forkingT->nachostable = currentThread->nachostable;
    forkingT->semtable = currentThread->semtable;
    forkingT->nachostable->addThread();
    forkingT->semtable->addThread();
    //Llamar al override de constructor de AddrSpace: copia de padre a hijo
    forkingT->space = new AddrSpace(currentThread->space, currentThread->space->codeSize, currentThread->space->addrSpaceUsage);
    forkingT->Fork(ForkVoidFunction, (void *)((long) machine->ReadRegister(4)));
    returnFromSystemCall();
}       // Nachos_Fork

/* Yield the CPU to another runnable thread, whether in this address space
 * or not.
 */
void Nachos_Yield() {           // System call 10
    currentThread->Yield();
    returnFromSystemCall();
}       // Nachos_Yield

/* SemCreate creates a semaphore initialized to initval value
 * return the semaphore id
 */
void Nachos_SemCreate() {        // System call 11
    int value = machine->ReadRegister(4);
    char * name = new char[50];
    sprintf(name, "Semaphore %d", currentThread->semtable->find());
    int handle = currentThread->semtable->Create((long)new Semaphore(name, value));
    if(handle == -1){
        machine->WriteRegister(2,-1);
    }
    else{
        machine->WriteRegister(2, handle);
    }
    returnFromSystemCall();
}       // Nachos_SemCreate

/* SemDestroy destroys a semaphore identified by id */
void Nachos_SemDestroy() {       // System call 12
    int sem_value = machine->ReadRegister(4);
    int handle = currentThread->semtable->Destroy(sem_value);
    if(handle == -1){
        machine->WriteRegister(2,-1);
    }
    else{
        machine->WriteRegister(2,handle);
    }

    returnFromSystemCall();
}       // Nachos_SemDestroy

/* SemSignal signals a semaphore, awakening some other thread if necessary */
void Nachos_SemSignal() {        // System call 13
    int sem_value = machine->ReadRegister(4);
    int returnVal=-1;
    //Encuentra el semaforo en la tabla de semaforos
    if(currentThread->semtable->exists(sem_value))
    {
      Semaphore* sem= (Semaphore*)currentThread->semtable->getSemaphore(sem_value);
      //Hace Signal en semaforo
      sem->V();
      returnVal=0;
    }
    //Devuelve valor dependiendo de si fue exitoso(0) o no(-1)
    machine->WriteRegister(2, returnVal);
    returnFromSystemCall();
}       // Nachos_SemSignal

/* SemWait waits a semaphore, some other thread may awake if one blocked */
void Nachos_SemWait() {          // System call 14

    int sem_value = machine->ReadRegister(4);
    int returnVal=-1;
    //Encuentra el semaforo en la tabla de semaforos
    if (currentThread->semtable->exists(sem_value))
    {
      Semaphore* sem=(Semaphore*)currentThread->semtable->getSemaphore(sem_value);
      //Hace Wait en semaforo
      sem->P();
      returnVal=0;
    }
    //Devuelve valor dependiendo de si fue exitoso(0) o no(-1)
    machine->WriteRegister(2, returnVal);
    returnFromSystemCall();
}       // Nachos_SemWait


void PageFaultCatcher() {
#ifdef VM
	unsigned int faultAddrs = machine->ReadRegister(39);
	DEBUG('s',"Page Fault at #%d. FaultAddrs = 0x%x Swapped Pages: %d\n", stats->numPageFaults,faultAddrs,currentThread->space->swapCounter);
	unsigned int faultPage = (unsigned) faultAddrs / PageSize;
	int faultOffset = faultAddrs % PageSize;
	DEBUG('s',"BadPage = %d, BadOffset = %d \n", faultPage, faultOffset);
	currentThread->space->load( faultPage );
	stats->numPageFaults++;
#else
	ASSERT(false);
#endif
}
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions
//	are in machine.h.
//----------------------------------------------------------------------

void ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    // if ((which == SyscallException) && (type == SC_Halt)) {
	// DEBUG('a', "Shutdown, initiated by user program.\n");
   	// interrupt->Halt();
    // } else {
	printf("SYSCALL: User mode exception %d %d\n", which, type);
	//ASSERT(false);
    //}

    switch ( which ) {

       case SyscallException:
          switch ( type ) {
             case SC_Halt:
                Nachos_Halt();              // System call # 0
                break;
             case SC_Exit:
                Nachos_Exit();              // System call # 1
                break;
             case SC_Exec:
                Nachos_Exec();              // System call # 2
                break;
             case SC_Join:
                Nachos_Join();              // System call # 3
                break;
             case SC_Create:
                Nachos_Create();            // System call # 4
                break;
             case SC_Open:
                Nachos_Open();              // System call # 5
                break;
             case SC_Read:
                Nachos_Read();              // System call # 6
                break;
             case SC_Write:
                Nachos_Write();             // System call # 7
                break;
             case SC_Close:
                Nachos_Close();             // System call # 8
                break;
             case SC_Fork:
                Nachos_Fork();              // System call # 9
                break;
             case SC_Yield:
                Nachos_Yield();             // System call # 10
                break;
             case SC_SemCreate:
                Nachos_SemCreate();         // System call # 11
                break;
             case SC_SemDestroy:
                Nachos_SemDestroy();        // System call # 12
                break;
             case SC_SemSignal:
                Nachos_SemSignal();         // System call # 13
                break;
             case SC_SemWait:
                Nachos_SemWait();           // System call # 14
                break;
             default:
                printf("Unexpected syscall exception %d\n", type );
                ASSERT(false);
                break;
          }
          //returnFromSystemCall();
       break;
       case PageFaultException:
			    PageFaultCatcher();
			    break;
       default:
          printf( "Unexpected exception %d\n", which );
          ASSERT(false);
          break;
    }
}
