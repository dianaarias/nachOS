#ifndef SEM_TABLE_
#define SEM_TABLE_

#include "bitmap.h"
#include "syscall.h"


class SemaphoreTable {
  public:
    SemaphoreTable();       // Initialize
    ~SemaphoreTable();      // De-allocate

    int Create( long sem ); // Register semaphore
    int Destroy( int id );  // Unregister semaphore
    void addThread();		// If a user thread is using this table, add it
    void delThread();		// If a user thread is using this table, delete it
    int find();
    int getUsage(); //Gets usage from sem table
    bool exists( int semId ); //Returns 1 if ID is registered, else 0
    long getSemaphore(int semId);
    int waitingThreads;
    void beginSemaphoreTable(long semId); //Starts a new semaphore table


  private:
    long * semaphores;		// A vector with semaphores
    BitMap * semaphore_map;	// A bitmap to control our vector
    int usage;            // How many threads are using this table

};
#endif //SEM_TABLE_
