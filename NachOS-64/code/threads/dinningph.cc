#include "dinningph.h"

DinningPh::DinningPh()
{

    dp = new Lock("dp");

    for (int i = 0; i < 5; i++)
    {
        self[i] = new Condition("philo");
        state[i] = Thinking;
    }
}

DinningPh::~DinningPh()
{
}

void DinningPh::pickup(long who)
{

    dp->Acquire();

    state[who] = Hungry;
    test(who);
    if (state[who] == Hungry)
        self[who]->Wait(dp);

    dp->Release();
}

void DinningPh::putdown(long who)
{

    dp->Acquire();

    state[who] = Thinking;
    test((who + 4) % 5);
    test((who + 1) % 5);

    dp->Release();
}

void DinningPh::test(long i)
{

    if ((state[(i + 4) % 5] != Eating) &&
        (state[i] == Hungry) &&
        (state[(i + 1) % 5] != Eating))
    {
        state[i] = Eating;
        self[i]->Signal(dp);
    }
}

void DinningPh::print()
{

    for (int i = 0; i < 5; i++)
    {
        printf("Philosopher %d is %s \n", i + 1, (state[i] == Hungry) ? "Hungry" : (state[i] == Thinking) ? "Thinking" : "Eating");
    }
}

/// --------------------------------

NorthPole::NorthPole()
{
    ws = new Lock("ws");

    for (int i = 0; i < 4; i++)
    {
        self[i] = new Condition("elf");
        state[i] = states::Resting;
    }
    workshop1[0] = -1;
    workshop1[1] = -1;
    workshop2[0] = -1;
    workshop2[1] = -1;
}

NorthPole::~NorthPole() {}

//Sincronization for workshop entrance
void NorthPole::enter(long elf)
{
    ws->Acquire();
    state[elf] = Resting;
    test(elf);
    if (state[elf] == Resting)
    {
        self[elf]->Wait(ws);
    }
    ws->Release();
}

//Sincronization for workshop exit
void NorthPole::exit(long elf)
{
    ws->Acquire();
    state[elf] = Resting;
    removeFromWS(elf, workshop1);
    removeFromWS(elf, workshop2);
    for(int i=0; i <4; i++){
        if(i != elf){
            test(i);
        }
    }
    ws->Release();
}

//This method is in charge of checking in which workshop an elf can be placed at
void NorthPole::test(long elf)
{
    removeFromWS(elf, workshop1);
    removeFromWS(elf, workshop2);
    //0: Gruñon, 1: Miedoso, 2: Tom, 3: Jerry
    switch (elf)
    {
    case 0: //Gruñon can only work by himself
        if (isEmpty(workshop1))
        {
            enterWS(elf, workshop1);
        }
        else if (isEmpty(workshop2))
        {
            enterWS(elf, workshop2);
        }
        break;
    case 1: //Miedoso can only work with Tom or Jerry. Never by himself
        if (theresRoom(workshop1) && (whosThere(workshop1) != 0))
        {
            enterWS(elf, workshop1);
        }
        else if (theresRoom(workshop2) && (whosThere(workshop2) != 0))
        {
            enterWS(elf, workshop2);
        }
        break;
    case 2: //Tom can only work by himself or with Miedoso
        if (isEmpty(workshop1))
        {
            enterWS(elf, workshop1);
        }
        else if (isEmpty(workshop2))
        {
            enterWS(elf, workshop2);
        }
        else if (theresRoom(workshop1) && (whosThere(workshop1) != 0) && (whosThere(workshop1) != 3))
        {
            enterWS(elf, workshop1);
        }
        else if (theresRoom(workshop2) && (whosThere(workshop2) != 0) && (whosThere(workshop1) != 3))
        {
            enterWS(elf, workshop2);
        }
        break;
    case 3: //Jerry can only work by himself or with Miedoso
        if (isEmpty(workshop1))
        {
            enterWS(elf, workshop1);
        }
        else if (isEmpty(workshop2))
        {
            enterWS(elf, workshop2);
        }
        else if (theresRoom(workshop1) && (whosThere(workshop1) != 0) && (whosThere(workshop1) != 2))
        {
            enterWS(elf, workshop1);
        }
        else if (theresRoom(workshop2) && (whosThere(workshop2) != 0) && (whosThere(workshop1) != 2))
        {
            enterWS(elf, workshop2);
        }
        break;
    default:
        std::cout << "Error in test" << endl;
        break;
    }
}

//Checks if workshop is empty
bool NorthPole::isEmpty(int *workshop)
{
    bool empty = false;
    if ((workshop[0] == -1) && (workshop[1] == -1))
    {
        empty = true;
    }
    return empty;
}

//Checks if workshop is full
bool NorthPole::isFull(int *workshop)
{
    bool full = false;
    if ((workshop[0] != -1) && (workshop[1] != -1))
    {
        full = true;
    }
    return full;
}

//Determines when there is room for one in workshop
//This means workshop must be neither full nor empty
bool NorthPole::theresRoom(int *workshop)
{ 
    bool indeed = false;
    if (!isEmpty(workshop) && !isFull(workshop))
    {
        indeed = true;
    }
    return indeed;
}

//Checks who is in currently working in workshop
int NorthPole::whosThere(int *workshop)
{ 
    if (theresRoom(workshop))
    {
        if (workshop[0] == -1)
        {
            return workshop[1];
        }
        else
        {
            return workshop[0];
        }
    }
}

//Determines position new elf needs to go to
//Assumes there's room for one more in workshop
int NorthPole::whereTo(int *workshop)
{ 
    if (workshop[0] == -1)
    {
        return 0;
    }
    else if (workshop[1] == -1)
    {
        return 1;
    }
}

//Removes elf from workshop
void NorthPole::removeFromWS(long elf, int *workshop)
{ 
    if (workshop[0] == elf)
    {
        workshop[0] = -1;
    }
    else if (workshop[1] == elf)
    {
        workshop[1] = -1;
    }
}

//Puts elf in its place
void NorthPole::enterWS(long elf, int *workshop)
{ 
    if (!isEmpty(workshop))//if there's another elf on the workshop
    {
        workshop[whereTo(workshop)] = elf;
        state[elf] = Working;
        self[elf]->Signal(ws);
    }
    else//if workshop's empty
    {
        workshop[0] = elf;
        state[elf] = Working;
        self[elf]->Signal(ws);
    }
}

