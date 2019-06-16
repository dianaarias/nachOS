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
#include "nachostable.h"
#include "synch.h"

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

/* This user program is done (status = 0 means exited normally). */
void Nachos_Exit() {           // System call 1
    int status = machine->ReadRegister(4);
    
}       // Nachos_Exit

/* Run the executable, stored in the Nachos file "name", and return the 
 * address space identifier
 */
void Nachos_Exec() {           // System call 2

}       // Nachos_Exec

/* Only return once the the user program "id" has finished.  
 * Return the exit status.
 */
void Nachos_Join() {           // System call 3

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
    OpenFileId id = machine->ReadRegister(4);
    
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
 * as the current thread.
 */
void Nachos_Fork() {            // System call 9

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

}       // Nachos_SemCreate

/* SemDestroy destroys a semaphore identified by id */ 
void Nachos_SemDestroy() {       // System call 12

}       // Nachos_SemDestroy

/* SemSignal signals a semaphore, awakening some other thread if necessary */
void Nachos_SemSignal() {        // System call 13

}       // Nachos_SemSignal

/* SemWait waits a semaphore, some other thread may awake if one blocked */
void Nachos_SemWait() {          // System call 14

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
             case SC_Open:
                Nachos_Open();             // System call # 5
                break;
             case SC_Write:
                Nachos_Write();             // System call # 7
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

