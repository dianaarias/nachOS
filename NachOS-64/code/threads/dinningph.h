#include "synch.h"
#include <iostream>

using namespace std;

class DinningPh
{

public:
  DinningPh();
  ~DinningPh();
  void pickup(long who);
  void putdown(long who);
  void test(long who);
  void print();

private:
  enum
  {
    Thinking,
    Hungry,
    Eating
  } state[5];
  Lock *dp; // Locks for each method in class
  Condition *self[5];
};

class NorthPole
{

public:
  NorthPole();
  ~NorthPole();
  void enter(long elf);
  void exit(long elf);
  void test(long elf);
  bool isEmpty(int *workshop);
  bool isFull(int *workshop);
  bool theresRoom(int *workshop);
  int whosThere(int *workshop);
  int whereTo(int *workshop);
  void removeFromWS(long elf, int *workshop);
  void enterWS(long elf, int *workshop);

private:
  //0: working, 1: resting
  //0: Gruñon, 1: Miedoso, 2: Tom, 3: Jerry
  enum states
  {
    Working = 0,
    Resting
  } state[4];
  int workshop1[2];
  int workshop2[2];
  Lock *ws;
  Condition *self[4];
};

