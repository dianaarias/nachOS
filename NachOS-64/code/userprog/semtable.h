#ifndef SEM_TABLE_
#define SEM_TABLE_

#include "bitmap.h"
#include "syscall.h"
#include "synch.h"

class SemaphoreTable {
  public:
    SemaphoreTable();       // Initialize
    ~SemaphoreTable();      // De-allocate

    int Create( long sem ); // Register semaphore
    int Destroy( int id );  // Unregister semaphore
    void addThread();		// If a user thread is using this table, add it
    void delThread();		// If a user thread is using this table, delete it
    int find();
    Semaphore* getSemaphore(int semId);


  private:
    long * semaphores;		// A vector with semaphores
    BitMap * semaphore_map;	// A bitmap to control our vector
    int usage;            // How many threads are using this table

};
#endif //SEM_TABLE_
