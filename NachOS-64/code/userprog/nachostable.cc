#include "nachostable.h"

NachosOpenFilesTable::NachosOpenFilesTable(){
    openFiles = new int[256];
    openFilesMap = new BitMap(256);
    usage = 0;

    for(int i=0; i<3; i++){
        openFiles[i] = i;
        openFilesMap->Mark(i);
    }
    for(int i=3; i < 256; i++){
        openFiles[i] = -1;
    }
}

NachosOpenFilesTable::~NachosOpenFilesTable(){
    delete openFiles;
    delete openFilesMap;
}

int NachosOpenFilesTable::Open(int UnixHandle){
    int pos = openFilesMap->Find();
    if(pos != -1){
        openFiles[pos] = UnixHandle;
    } 
    return pos;
}

int NachosOpenFilesTable::Close(int NachosHandle){
    bool test = openFilesMap->Test(NachosHandle);
    if(test){
        openFilesMap->Clear(NachosHandle);
    }
    return getUnixHandle(NachosHandle);
}

bool NachosOpenFilesTable::isOpened(int NachosHandle){
    bool test = openFilesMap->Test(NachosHandle);
    return test;
}

int NachosOpenFilesTable::getUnixHandle(int NachosHandle){
    if(openFilesMap->Test(NachosHandle)){
        return NachosHandle;
    }
    else{
        return -1;
    }
}

void NachosOpenFilesTable::addThread(){
    usage++;
}

void NachosOpenFilesTable::delThread(){
    usage--;
}

void NachosOpenFilesTable::Print(){
    printf("Pos    UnixHandle    IsOpen");
    for(int i=0; i<256; i++){
        if(openFilesMap->Test(i)){
            printf("%-12d : %-14d : %d", i, getUnixHandle(i), openFilesMap->Test(i));
        }
    }
}