#include "semtable.h"

SemaphoreTable::SemaphoreTable(){
    semaphores = new long[25];
    semaphore_map = new BitMap(25);
    semaphore_map->Mark(0);
    usage = 0;
}

SemaphoreTable::~SemaphoreTable(){
    delete semaphores;
    delete semaphore_map;
}

int SemaphoreTable::Create(long sem){
    int pos = semaphore_map->Find();
    if(pos != -1){
        semaphores[pos] = sem;
        semaphore_map->Mark(pos);
    }
    return pos;
}

int SemaphoreTable::Destroy(int id){
    bool test = semaphore_map->Test(id);
    if(test){
        semaphore_map->Clear(id);
    }
    else{
        return -1;
    }
    return 1; 
}

void SemaphoreTable::addThread(){
    usage++;
}

void SemaphoreTable::delThread(){
    usage--;
}

int SemaphoreTable::find(){
    int sem = semaphore_map->Find();
    if(sem != -1){
        semaphore_map->Clear(sem);
    }
    return sem;
}