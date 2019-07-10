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
//Crea un nuevo puntero a un semaforo en el arreglo de semaforos
int SemaphoreTable::Create(long sem){
    int pos = semaphore_map->Find();
    if(pos != -1){
        semaphores[pos] = sem;
        semaphore_map->Mark(pos);
    }
    return pos;
}
//Destruye el semaforo en el array y limpia bitmap
int SemaphoreTable::Destroy(int id){
    bool test = semaphore_map->Test(id);
    if(test){
        delete  (Semaphore*)&semaphores[id];
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
//Encuentra la primer posición válida en el bitmap para el semaforo y lo limpia antes de retornarlo
int SemaphoreTable::find(){
    int sem = semaphore_map->Find();
    if(sem != -1){
        semaphore_map->Clear(sem);
    }
    return sem;
}

Semaphore* SemaphoreTable::getSemaphore(int semId)
{
  bool test = semaphore_map->Test(semId);
  Semaphore* semPointer;
  if(test)
  {
    semPointer= (Semaphore*)& semaphores[semId];
  }
  return semPointer;
}
