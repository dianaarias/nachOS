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

Semaphore* Console = new Semaphore("Console", 1);

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
    if(joinAvailable[currentThread->execID])
    {
       int semid = semaphoreIDVec[currentThread->execID];
       semaphoreJoin->getSemaphore(semid)->V();
    }
    currentThread->Finish();
}       // Nachos_Exit

/* Run the executable, stored in the Nachos file "name", and return the
 * address space identifier
 */
void NachosExec(void *file)
{
  currentThread->space = new AddrSpace((OpenFile *) file);

  //Initialize registers and then load the pages table
  currentThread->space->InitRegisters();
  currentThread->space->RestoreState();

  //Return adress is the same as calling thread
  machine->WriteRegister(RetAddrReg, 4);
  // run the program
  machine->Run();
  ASSERT(false);
}


void Nachos_Exec() {           // System call 2
  ASSERT(execSemaphoreMap->NumClear() > 0); //Checks for space to joinAvailable
  //Loads the content
  int registerContent =machine->ReadRegister(4);
  //Creates variables to read the path
  char* path=new char[OPEN_FILE_TABLE_SIZE];
  int charTaken =-1;
  int pathIndex =0;
  //Loads the path onto array
  while(0!= charTaken)
  {
    machine->ReadMem(registerContent,1,&charTaken);
    path[pathIndex]=(char)charTaken;
    registerContent++;
    pathIndex++;
  }
  //Uses function NachosExec in a new child thread with info obtained
  Thread* childExec = new Thread("Child for EXEC");
  childExec->execID = execSemaphoreMap->Find();
  int savedChildID =childExec->execID;
  childExec->Fork(NachosExec, (void*)fileSystem->Open(path));
  machine->WriteRegister(2,savedChildID);
}       // Nachos_Exec

/* Only return once the the user program "id" has finished.
 * Return the exit status.
 */
void Nachos_Join() {           // System call 3
    int id = machine->ReadRegister(4);
    if (execSemaphoreMap->Test(id))
    {
      joinAvailable[id] = true;
      int semaphoreid = semaphoreJoin->Create(0);
      semaphoreIDVec[id] = semaphoreid;
      semaphoreJoin->getSemaphore(semaphoreid)->P();
      semaphoreJoin->Destroy(semaphoreid);
      joinAvailable[id] = false;
      execSemaphoreMap->Clear(id);
  }
  else
  {
      ASSERT(false);
  }
}       // Nachos_Join

/* Create a Nachos file, with "name" */
void Nachos_Create() {           // System call 4
    int reg4 = machine->ReadRegister(4);
    char file_name[100] = {};
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
    char file_name[100] = {};
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
    int reg4 = machine->ReadRegister(4);  // Read address
    int size = machine->ReadRegister(5);  // Read size to read
    OpenFileId id = machine->ReadRegister(6);
    char buffer[size] = {};
    ssize_t read_result = 0;

    Console->P();
    switch (id) {
        case ConsoleInput:
            read_result = read(id, buffer, size);
            machine->WriteRegister(2, read_result);
        break;
        case ConsoleOutput:
            machine->WriteRegister( 2, -1 );
        break;
        case ConsoleError:
            printf( "%d\n", machine->ReadRegister( 4 ) );
        break;
        default:
            if(currentThread->nachostable->isOpened(id)){
                int UnixHandle = currentThread->nachostable->getUnixHandle(id);
                read_result = read(UnixHandle, buffer, size);
                machine->WriteRegister(2, read_result);
            }
            else{
                machine->WriteRegister(2, -1);
            }
        break;
    }
    Console->V();

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
	Console->P();
	switch (id) {
		case  ConsoleInput:	// User could not write to standard input
			machine->WriteRegister( 2, -1 );
			break;

    case  ConsoleOutput:
			buffer[ size ] = {};
			printf( "%s", buffer );
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
      }
      else{
            machine->WriteRegister(2, -1);
          }
		break;
	}
	// Update simulation stats, see details in Statistics class in machine/stats.cc
	Console->V();

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
  newSpace->InitRegisters();
  newSpace->RestoreState();
  //Actualiza los registros para el Fork
  machine->WriteRegister(RetAddrReg,4);
  machine->WriteRegister(PCReg,(long)registerPointer);
  machine->WriteRegister(NextPCReg,(long)registerPointer+4);
  //Se ejecuta el nuevo fork en userProg.
  machine->Run();
}

void Nachos_Fork() {            // System call 9
    //Se crea el nuevo hilo por el fork
    Thread* forkingT = new Thread("Forking Thread");
    //Le provee al nuevo hilo el copia del espacio del padre
    forkingT->space = new AddrSpace(currentThread->space);

    forkingT->Fork(ForkVoidFunction,(void*)machine->ReadRegister(4));
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
    sprintf(name, "Semaphore %d", semtable->find());
    Semaphore *semaphore = new Semaphore("New Sem", value);
    int handle = semtable->Create((long)new Semaphore(name, value));
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
    int handle = semtable->Destroy(sem_value);
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
    Semaphore* sem;
    //Encontrar el semaforo en la tabla de semaforos
    sem = semtable->getSemaphore(sem_value);
    //Hacer Signal en semaforo
    sem->V();
    machine->WriteRegister(2, 1);
}       // Nachos_SemSignal

/* SemWait waits a semaphore, some other thread may awake if one blocked */
void Nachos_SemWait() {          // System call 14
    Semaphore* sem;
    int sem_value = machine->ReadRegister(4);
    //Encontrar el semaforo en tabla de semaforos
    sem = semtable->getSemaphore(sem_value);
    //Hacer wait en el semaforo
    sem->P();
    machine->WriteRegister(2, 1);
}       // Nachos_SemWait

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

void
ExceptionHandler(ExceptionType which)
{
    // int type = machine->ReadRegister(2);

    // if ((which == SyscallException) && (type == SC_Halt)) {
	// DEBUG('a', "Shutdown, initiated by user program.\n");
   	// interrupt->Halt();
    // } else {
	// printf("Unexpected user mode exception %d %d\n", which, type);
	// ASSERT(false);
    // }

    int type = machine->ReadRegister(2);

    switch ( which ) {

       case SyscallException:
          switch ( type ) {
             case SC_Halt:
                Nachos_Halt();             // System call # 0
                break;
             case SC_Exit:
                Nachos_Exit();
                break;
             case SC_Exec:
                Nachos_Exec();
                break;
             case SC_Join:
                Nachos_Join();
                break;
             case SC_Open:
                Nachos_Open();             // System call # 5
                break;
             case SC_Write:
                Nachos_Write();             // System call # 7
                break;
             case SC_Read:
                Nachos_Read();
                break;
             case SC_Create:
                Nachos_Create();
                break;
             case SC_Fork:
                Nachos_Fork();
                break;
             case SC_Yield:
                Nachos_Yield();
                break;
             case SC_SemCreate:
                Nachos_SemCreate();
                break;
             case SC_SemDestroy:
                Nachos_SemDestroy();
                break;
             case SC_SemSignal:
                Nachos_SemSignal();
                break;
             case SC_SemWait:
                Nachos_SemWait();
                break;
             default:
                printf("Unexpected syscall exception %d\n", type );
                ASSERT(false);
                break;
          }
       break;
       default:
          printf( "Unexpected exception %d\n", which );
          ASSERT(false);
          break;
    }
}
