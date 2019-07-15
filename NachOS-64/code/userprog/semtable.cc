#include "semtable.h"
#define MAX_SEM 32
SemaphoreTable::SemaphoreTable(){
    semaphores = new long[25];
    semaphore_map = new BitMap(25);
    semaphore_map->Mark(0);
    usage = 1;
    waitingThreads = 0;
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

int SemaphoreTable::getUsage() {
	return usage;
}

long SemaphoreTable::getSemaphore (int semId) {
	// Verify the handle is valid
	if (semId < MAX_SEM && semId >= 0 && semaphore_map -> Test(semId) == true) {
		return semaphores[semId];
	}
	else {
		return 0;
	}
}

bool SemaphoreTable::exists(int semId) {
	// Verify the handle is valid
  bool exists= (semId < MAX_SEM) && (semId >= 0) && (semaphore_map -> Test(semId) == true);
	return exists;
}

void SemaphoreTable::beginSemaphoreTable(long semId) {
	semaphores[0] = semId;
}
