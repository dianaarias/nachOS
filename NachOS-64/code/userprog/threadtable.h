#include "bitmap.h"
#include "syscall.h"
#include "thread.h"
#include <vector>


class NachosThreadTable {
  public:
    NachosThreadTable();       // Initialize
    ~NachosThreadTable();      // De-allocate

    int Create( Thread* newT ); // Register the file handle
    int Destroy( int pid );      // Unregister the file handle
    bool exists( int pid );
	  Thread* getThread (int pid);

  private:
    std::vector<Thread*> threads;		// A vector with user opened files
    BitMap * threadMap;	// A bitmap to control our vector
};
