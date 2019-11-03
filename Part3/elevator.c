#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include "elevator.h"

MODULE_LICENSE("GPL");

// DEFINITIONS FOR ELEVATOR CONSTRAINTS
#define MAX_PASS 10
#define MAX_WEIGHT 150
#define MAX_FLOOR 10
#define MIN_FLOOR 1

// DEFINITIONS FOR PASSENGER TYPES
#define ADULTS 1
#define CHILD 2
#define ROOM_SERVICE 3
#define BELLHOP 4

// ENUMERATIONS FOR ELEVATOR STATES
#define OFFLINE 0
#define IDLE 1
#define LOADING 2
#define UP 3
#define DOWN 4

// Declaring Mutexes
struct mutex elevatorMutex;
EXPORT_SYMBOL(elevatorMutex);
struct mutex queueMutex;
EXPORT_SYMBOL(queueMutex);

// Declaring Thread
struct task_struct * elevator_thread;

// Declaring Global Variables
Elevator elevator;
EXPORT_SYMBOL(elevator);
Queue passQueue;
EXPORT_SYMBOL(passQueue);

/**************************************************************************************************/

/*
This function takes the elevator to the next floor in the direction in which it is
going. If the elevator is at the top and going up, then the state is changed to down;
and if the elevator is at the bottom floor going down, the state changes to up.
*/

static void nextFloor(void)
{
	if(elevator.state == DOWN)
	{
		if (elevator.currFloor > 1)
		{
			elevator.destFloor--;
		}
		else
		{
			elevator.state = UP;
			elevator.destFloor++;
		}
	}
	else if (elevator.state == UP)
	{
		if (elevator.currFloor < 10)
		{
			elevator.destFloor++;
		}
		else
		{
			elevator.state = DOWN;
			elevator.destFloor--;
		}
	}
	else if ((elevator.state == IDLE) && (passQueue.size != 0))
	{
		elevator.state = UP;
	}
	else if (elevator.state == LOADING)
	{
		elevator.state = elevator.prevState;
	}
}

/*
If the elevator has reached max weight or if it has reached the maximum number of
passengers, the the function returns true. Otherwise, it returns false.
*/

static int atMax(void)
{
	if ((elevator.passUnit == MAX_PASS) || (elevator.weightUnit == MAX_WEIGHT))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/*
If the elevator is able to load a passenger, and if the passenger's start floor is the same as the
elevator's current location, this function returns true. Otherwise, it return false.
*/

static int Loadable(Passenger * passenger)
{
	if (passenger->weightUnit <= MAX_WEIGHT - elevator.weightUnit)
	{
		if (passenger->passUnit <= MAX_PASS - elevator.passUnit)
		{
			if (passenger->start == elevator.currFloor)
			{
				if (((elevator.state == UP) || (elevator.currFloor == 1)) && (passenger->dest > elevator.currFloor))
				{
					return 1;
				}
				else if (((elevator.state == DOWN) || (elevator.currFloor == 10)) && (passenger->dest < elevator.currFloor))
				{
					return 1;
				}
				else
				{
					return 0;
				}
			}
			else
			{
				return 0;
			}
		}
		else
		{
			return 0;
		}
	}
	else
	{
		return 0;
	}
}

/*
This function loads the passenger onto the elevator by passing over the waiting queue and loading
every passenger that is Loadable until the elevator is either at max weight, at max passenger capacity,
or until all passengers at that floor have been loaded. The number of passengers loaded is returned 
by the counter variable.
*/

static int Load(void)
{
	struct list_head * dummy = NULL;
	struct list_head * temp = NULL;

	struct Passenger * passenger = NULL;

	int counter = 0;

	list_for_each_safe(temp, dummy, &passQueue.list[elevator.currFloor - 1])
	{
		passenger = list_entry(temp, Passenger, list);

		if (Loadable(passenger))
		{
			list_del(&passenger->list);
			list_add(&passenger->list, &elevator.list[passenger->dest - 1]);

			elevator.size += 1;
			elevator.passUnit += passenger->passUnit;
			elevator.weightUnit += passenger->weightUnit;

			passQueue.floorSize[elevator.currFloor - 1] -= 1;
			passQueue.size -= 1;

			counter++;
		}

		if (atMax())
		{
			return counter;
		}
	}

	return counter;
}

/*
This function removes passengers from the elevator's queue until all passengers
whose destination is the current floor are cleared from the queue. The number of
passengers unloaded is returned by the counter variable.
*/

static int Unload(void)
{
	struct list_head * temp = NULL;
	struct list_head * dummy = NULL;

	struct Passenger * passenger = NULL;

	int counter = 0;

	list_for_each_safe(temp, dummy, &elevator.list[elevator.currFloor - 1])
	{
		passenger = list_entry(temp, Passenger, list);

		elevator.size -= 1;
                elevator.passUnit -= passenger->passUnit;
                elevator.weightUnit -= passenger->weightUnit;

		list_del(&passenger->list);
		kfree(passenger);

		counter++;
	}

	return counter;
}

/*
Process for running the elevator. Scheduling algorithm is SCAN
*/

int Elevator_Process(void * data)
{
	int loadPass = 0;
	int unloadPass = 0;
	int finished = 0;
	int cF, dF;

	while(!elevator.stop_call)	// While loop for when elevator is in normal operation
	{
		loadPass = unloadPass = 0;	// Reset local variables

		mutex_lock(&elevatorMutex);	// Lock mutexes
		mutex_lock(&queueMutex);

		unloadPass = Unload();	// Unload applicable passengers
		loadPass = Load();	// Load applicable passengers

		elevator.passServiced[elevator.currFloor - 1] += unloadPass;	// Update number of passengers serviced

		if (loadPass + unloadPass > 0)	// If anybody loaded or unloaded then change state to LOADING
		{
			elevator.prevState = elevator.state;
			elevator.state = LOADING;
		}

		mutex_unlock(&elevatorMutex);	// Unlock mutexes
		mutex_unlock(&queueMutex);

		if (loadPass + unloadPass > 0)	// Sleeps for one second if anybody got off or on
		{
			ssleep(1);
		}

		mutex_lock(&elevatorMutex);	// Lock elevator mutex
		mutex_lock(&queueMutex);

		if ((passQueue.size != 0) || (elevator.passUnit != 0))
		{
			nextFloor();	// Update destination floor
		}
		else
		{
			elevator.state = IDLE;
		}

		cF = elevator.currFloor;
		dF = elevator.destFloor;

		mutex_unlock(&queueMutex);
		mutex_unlock(&elevatorMutex);	// Unlock elevator mutex

		if (cF != dF)
		{
			ssleep(2);
		}

		mutex_lock(&elevatorMutex);

                if (elevator.currFloor != elevator.destFloor)	// Update elevators current floor before starting loop again
		{
			elevator.currFloor = elevator.destFloor;
		}

		mutex_unlock(&elevatorMutex);
	}

	while((elevator.passUnit > 0) && (!kthread_should_stop()))	// While loop for unloading rest of passengers
	{							// on elevator before shutting down
		unloadPass = 0;

		mutex_lock(&elevatorMutex);	// Lock mutexes
		mutex_lock(&queueMutex);

		unloadPass = Unload();	// Unload passengers if applicable

		elevator.passServiced[elevator.currFloor - 1] += unloadPass;	// Update number of passengers serviced

		if (unloadPass > 0)	// If elevator unloads anyone then change state to LOADING
		{
			elevator.state = LOADING;
		}

		mutex_unlock(&elevatorMutex);	// Unlock mutexes
		mutex_unlock(&queueMutex);

		if (unloadPass > 0)	// If elevator unloads anyone then wait 1 second
		{
			ssleep(1);
		}

		mutex_lock(&elevatorMutex);	// Lock elevator mutex

		if (elevator.passUnit != 0)	// If there are still passengers aboard
		{				// then update destination floor
			nextFloor();
		}
		else				// Else change state to OFFLINE and dont move
		{
			finished = 1;
		}

                cF = elevator.currFloor;
                dF = elevator.destFloor;

		mutex_unlock(&elevatorMutex);	// Unlock elevator mutex

		if (!finished)	// If elevator is not finished unloading everyone
		{		// then wait 2 seconds for floor change
			ssleep(2);
		}

		mutex_lock(&elevatorMutex);	// Lock elevator mutex;

		if (elevator.currFloor != elevator.destFloor)	// Update current floor
		{
			elevator.currFloor = elevator.destFloor;
		}

		mutex_unlock(&elevatorMutex);	// Unlock elevatorMutex
	}

	mutex_lock(&elevatorMutex);

	elevator.state = OFFLINE;

	mutex_unlock(&elevatorMutex);

	return 0;
}

/**************************************************************************************************/

/*
System call function to start the elevator process
*/
extern int (*STUB_start_elevator)(void);
int my_start_elevator(void)
{
	int temp;
	int i;

	mutex_lock(&elevatorMutex);	// Lock Elevator mutex

	if (elevator.state == OFFLINE)	// Initialize elevator variables
	{
		elevator.state = IDLE;
		elevator.currFloor = 1;
		elevator.destFloor = 1;
		elevator.passUnit = 0;
		elevator.weightUnit = 0;
		elevator.stop_call = 0;
		for (i = 0; i < 10; i++)
		{
			INIT_LIST_HEAD(&elevator.list[i]);
		}

		elevator_thread = kthread_run(Elevator_Process, NULL, "elevator");	// Create a new thread to run the elevator process

		if (IS_ERR(elevator_thread) != 0)	// Error checking for creating the thread
		{
			printk(KERN_ERR "Elevator Process failed: thread error\n");
			temp = -1;
		}
		else
		{
			temp = 0;
		}
	}
	else
	{
		temp = 1;
	}

	mutex_unlock(&elevatorMutex);	// Unlock mutex

	return temp;
}

/*
System call that adds a new passenger to the waiting queue
*/
extern int (*STUB_issue_request)(int,int,int);
int my_issue_request(int type, int start, int dest)
{
        int pU = 0;
	int wU = 0;

	Passenger * p = NULL;

	switch (type)	// Switch statement to determine the new passenger and weight units
	{
		case ADULTS:
			pU = 1;
			wU = 10;
			break;
		case CHILD:
			pU = 1;
			wU = 5;
			break;
		case ROOM_SERVICE:
			pU = 2;
			wU = 20;
			break;
		case BELLHOP:
			pU = 2;
			wU = 40;
			break;
		default:
			printk("Fail on passenger type\n");
			return 1;
	}

	if ((start >= 1) && (start <= 10) && (dest >= 1) && (dest <= 10) && (start != dest))	// Conditional statement to make sure the floor
	{											// levels are within specifications
		p = kmalloc(sizeof(Passenger) * 1, __GFP_RECLAIM);

		if (p != NULL)	// Initializes new Passenger with parameters if details are valid
		{
			p->passUnit = pU;
			p->weightUnit = wU;
			p->start = start;
			p->dest = dest;
			INIT_LIST_HEAD(&p->list);

			mutex_lock(&queueMutex);	// Lock mutex

			list_add_tail(&p->list, &passQueue.list[p->start - 1]);	// Adds new passenger to floor queue
			passQueue.floorSize[p->start - 1] += 1;			// Update queue variables
			passQueue.size += 1;

			mutex_unlock(&queueMutex);	// Unlock mutex

			return 0;
		}
		else
		{
			printk("Fail in malloc\n");
			return 1;
		}
	}
	else
	{
		printk("Fail in floor\n");
		return 1;
	}
}

/*
System call to stop elevator
*/
extern int (*STUB_stop_elevator)(void);
int my_stop_elevator(void)
{
	int temp;

	mutex_lock(&elevatorMutex);	// Lock mutex

	if (elevator.stop_call == 0)	// Turn on stop variable if not already on
	{
		elevator.stop_call = 1;
		temp = 0;
	}
	else
	{
		temp = 1;
	}

	mutex_unlock(&elevatorMutex);	// Unlock mutex

	return temp;
}


/****************************************************************************************/

/*
Module initialization
*/
static int elevator_init(void)
{
	STUB_start_elevator = my_start_elevator;	// Assign system call stubs
	STUB_issue_request = my_issue_request;
	STUB_stop_elevator = my_stop_elevator;

	mutex_init(&elevatorMutex);	// Initialize mutexes
	mutex_init(&queueMutex);

	mutex_lock(&elevatorMutex);	// Lock elevator mutex

	int i;

	elevator.state = 0;		// Initialize elevator variables
	elevator.currFloor = 1;
	elevator.destFloor = 1;
	elevator.passUnit = 0;
	elevator.weightUnit = 0;
	elevator.stop_call = 0;
	for (i = 0; i < 10; i++)
	{
		INIT_LIST_HEAD(&elevator.list[i]);
		elevator.passServiced[i] = 0;
	}

	mutex_unlock(&elevatorMutex);	// Unlock elevator mutex

	mutex_lock(&queueMutex);	// lock queue mutex

	for(i = 0; i < 10; i++)		// Initialize queue variables
	{
		INIT_LIST_HEAD(&passQueue.list[i]);
		passQueue.floorSize[i] = 0;
	}

	passQueue.size = 0;

	mutex_unlock(&queueMutex);	// Unlock mutex

	printk(KERN_ALERT "Elevator Initialized!\n");

	return 0;
}

module_init(elevator_init);

/*
Module exit function
*/
static void elevator_exit(void)
{
	STUB_start_elevator = NULL;
	STUB_issue_request = NULL;
	STUB_stop_elevator = NULL;

	mutex_destroy(&elevatorMutex);
	mutex_destroy(&queueMutex);

	printk(KERN_ALERT "Elevator Stopping!\n");
}

module_exit(elevator_exit);
