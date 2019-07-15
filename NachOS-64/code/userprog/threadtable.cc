#include "threadtable.h"
#define MAX_THREAD 32

//----------------------------------------------------------------------
// Constructor for the table
//----------------------------------------------------------------------
NachosThreadTable::NachosThreadTable() {
	DEBUG('N', "Init the Nachos Thread table\n");
	threadMap = new BitMap(MAX_THREAD);
	threads = std::vector<Thread*>(MAX_THREAD, (Thread*)0);
}

//----------------------------------------------------------------------
// Destructor for the table
//----------------------------------------------------------------------

NachosThreadTable::~NachosThreadTable() {
	DEBUG('N', "Delete the Thread table\n");
	delete threadMap;
	DEBUG('N', "Thread Table succesfully deleted\n");
}

//----------------------------------------------------------------------
// Save a space in the array for a new Process
//
// 		Thread *newT -> Thread to save
//----------------------------------------------------------------------
int NachosThreadTable::Create(Thread* newTread) {
	int i = threadMap -> Find();

	if (i != -1) {
		// If not full
		threads[i] = newTread;
	} else {
		// If full
		printf ("Unable to open the Thread : THREAD\n");
	}
	return i;
}

//----------------------------------------------------------------------
// Close the entry in the table for the respective process id
//
//		int pid -> id used to close an entry
//----------------------------------------------------------------------
int NachosThreadTable::Destroy(int threadID) {
	if (threadID < MAX_THREAD && threadID >= 0 && threadMap -> Test(threadID) == true) {
		// If opened clear it
		threadMap -> Clear(threadID);
	} else {
		// If not opened
		DEBUG ('N', "The thread %d doesn't exists\n", threadID);
		return -1;
	}
	return 1;
}

//----------------------------------------------------------------------
// Verify if the respective process id is registered
//
//		int pid -> Id used to verify if the thread is registered
//----------------------------------------------------------------------
bool NachosThreadTable::exists(int threadID) {

	// Verify the handle is valid
	if (threadID < MAX_THREAD && threadID >= 0 && threadMap -> Test(threadID) == true) {
		return true;
	} else {
		return false;
	}
}

//----------------------------------------------------------------------
// Get the respective Thread for a process id
//
//		int pid -> Look the thread for this id
//----------------------------------------------------------------------
Thread* NachosThreadTable::getThread (int threadID) {
	// Verify the handle is valid
	if (threadID < MAX_THREAD && threadID >= 0 && threadMap -> Test(threadID) == true) {
		return threads[threadID];
	}
	else {
		return 0;
	}
}
