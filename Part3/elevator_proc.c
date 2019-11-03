#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/list.h>

#include "elevator.h"

#define start 335
#define issue 336
#define stop 337

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple module featuring proc read");

#define ENTRY_NAME "elevator"
#define ENTRY_SIZE 1000
#define PERMS 0644
#define PARENT NULL
static struct file_operations fops;

static char *message;
static int read_p;

extern struct Elevator elevator;
extern struct Queue passQueue;

extern struct mutex elevatorMutex;
extern struct mutex queueMutex;

/**********************************************************************************************/

/*
Function that returns the total weight of a floor
*/
int getFloorWeight(const int i)
{
	struct list_head * temp;
	struct list_head * dummy;
	Passenger * passenger;

	int wU = 0;

	list_for_each_safe(temp, dummy, &passQueue.list[i])
	{
		passenger = list_entry(temp, Passenger, list);

		wU += passenger->weightUnit;
	}

	return wU;
}

/*
Function that returns the total passenger unit of a floor
*/
int getFloorPassenger(const int i)
{
        struct list_head * temp;
        struct list_head * dummy;
        Passenger * passenger;

        int pU = 0;

        list_for_each_safe(temp, dummy, &passQueue.list[i])
        {
                passenger = list_entry(temp, Passenger, list);

                pU += passenger->passUnit;
        }

        return pU;
}

/*
Function that returns a character string of the elevator statistics
*/
char * printElevator(void)
{
	char *buffer = kmalloc(sizeof(char) * ENTRY_SIZE, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	int i;
	int integer, decimal;

	switch (elevator.state)		// Switch statement for elevator state
	{
		case 0:
			sprintf(buffer, "Elevator state: OFFLINE\n");
			break;
		case 1:
                        sprintf(buffer, "Elevator state: IDLE\n");
			break;
		case 2:
                        sprintf(buffer, "Elevator state: LOADING\n");
			break;
		case 3:
                        sprintf(buffer, "Elevator state: UP\n");
			break;
		case 4:
                        sprintf(buffer, "Elevator state: DOWN\n");
			break;
		default:
			break;
	}

	sprintf(buffer + strlen(buffer), "Current floor: %d\n", elevator.currFloor);	// Prints current floor
	sprintf(buffer + strlen(buffer), "Destination floor: %d\n", elevator.destFloor);	// Prints next floor
	sprintf(buffer + strlen(buffer), "Current passenger load: %d\n", elevator.passUnit);	// Prints current passenger load of the elevator

	integer = elevator.weightUnit / 10;
	decimal = elevator.weightUnit % 10;

	sprintf(buffer + strlen(buffer), "Current weight load: %d.%d\n", integer, decimal);	// Prints current weight load of elevator

	sprintf(buffer + strlen(buffer), "*********************************************\n");

	for (i = 10; i > 0; i--)	// For loop to print statistics of each floor
	{
		sprintf(buffer + strlen(buffer), "Floor %d:\n", i);	// Floor number
		sprintf(buffer + strlen(buffer), "\tPassenger load: %d\n", getFloorPassenger(i - 1));	// Prints passenger unit of floor

	        integer = getFloorWeight(i - 1) / 10;
	        decimal = getFloorWeight(i - 1) % 10;

		sprintf(buffer + strlen(buffer), "\tWeight load: %d.%d\n", integer, decimal);	// Prints weight unit of floor
		sprintf(buffer + strlen(buffer), "\tPassengers serviced: %d\n", elevator.passServiced[i - 1]);	// Prints number of people serviced for that floor
	}

	return buffer;	// Return full summary
}

/***************************************************************************************************/

int elevator_proc_open(struct inode *sp_inode, struct file *sp_file) {
	printk(KERN_INFO "proc called open\n");

	read_p = 1;

        message = kmalloc(sizeof(char) * ENTRY_SIZE, __GFP_RECLAIM | __GFP_IO | __GFP_FS);	// Allocate space for message
        if (message == NULL) {
                printk(KERN_WARNING "elevator_proc_open");
                return -ENOMEM;
        }

	mutex_lock(&elevatorMutex);	// Lock mutexes
	mutex_lock(&queueMutex);

	sprintf(message, printElevator());	// Copy elevator summary to message to be printed

	mutex_unlock(&elevatorMutex);	// Unlock mutex
	mutex_unlock(&queueMutex);

	return 0;
}

ssize_t elevator_proc_read(struct file *sp_file, char __user *buf, size_t size, loff_t *offset) {
	int len = strlen(message);

	read_p = !read_p;
	if (read_p)
		return 0;

	printk(KERN_INFO "proc called read\n");
	copy_to_user(buf, message, len);
	return len;
}

int elevator_proc_release(struct inode *sp_inode, struct file *sp_file) {
	printk(KERN_NOTICE "proc called release\n");
	kfree(message);
	return 0;
}

/**************************************************************************/

static int elevator_init(void) {
	printk(KERN_NOTICE "/proc/%s create\n",ENTRY_NAME);
	fops.open = elevator_proc_open;
	fops.read = elevator_proc_read;
	fops.release = elevator_proc_release;
	
	if (!proc_create(ENTRY_NAME, PERMS, NULL, &fops)) {
		printk(KERN_WARNING "proc create\n");
		remove_proc_entry(ENTRY_NAME, NULL);
		return -ENOMEM;
	}
	
	return 0;
}
module_init(elevator_init);

static void elevator_exit(void) {
	remove_proc_entry(ENTRY_NAME, NULL);
	printk(KERN_NOTICE "Removing /proc/%s\n", ENTRY_NAME);
}
module_exit(elevator_exit);
