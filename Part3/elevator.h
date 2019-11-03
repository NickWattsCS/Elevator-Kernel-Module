#ifndef __ELEVATOR
#define __ELEVATOR

#include <linux/list.h>

struct Elevator
{
        int state;
	int prevState;
        int currFloor;
        int destFloor;
        int passUnit;
        int weightUnit;
        int stop_call;
	int passServiced[10];
	int size;
        struct list_head list[10];
};

typedef struct Elevator Elevator;

struct Passenger
{
        int passUnit;
        int weightUnit;
        int start;
        int dest;
        struct list_head list;
};

typedef struct Passenger Passenger;

struct Queue
{
        struct list_head list[10];
        int size;
	int floorSize[10];
};

typedef struct Queue Queue;

#endif
