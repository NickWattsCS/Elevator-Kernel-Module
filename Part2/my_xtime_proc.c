#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/time.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple module featuring proc read");

#define ENTRY_NAME "timed"
#define ENTRY_SIZE 100
#define PERMS 0644
#define PARENT NULL
static struct file_operations fops;

static char *message;
static int read_p;
static struct timespec timed;
static struct timespec previous;
static struct timespec difference;
static int counter;

int time_proc_open(struct inode *sp_inode, struct file *sp_file) {
	printk(KERN_INFO "proc called open\n");
	
	read_p = 1;
	message = kmalloc(sizeof(char) * ENTRY_SIZE, __GFP_RECLAIM | __GFP_IO | __GFP_FS);
	if (message == NULL) {
		printk(KERN_WARNING "time_proc_open");
		return -ENOMEM;
	}
	
	counter += 1;
	timed = current_kernel_time();

	if (counter > 1)
	{
		difference.tv_nsec = timed.tv_nsec - previous.tv_nsec;
		if (difference.tv_nsec < 0)
		{
			difference.tv_nsec += 1000000000;
			difference.tv_sec = timed.tv_sec - previous.tv_sec - 1;
		}
		else
		{
			difference.tv_sec = timed.tv_sec - previous.tv_sec;
		}
		sprintf(message, "Current Time: %ld.%ld\nElapsed Time: %ld.%ld\n", timed.tv_sec, timed.tv_nsec, difference.tv_sec, difference.tv_nsec);
	}
	else
	{
		sprintf(message,"Current Time: %ld.%ld\n", timed.tv_sec, timed.tv_nsec);
	}

	previous = timed;
	return 0;
}

ssize_t time_proc_read(struct file *sp_file, char __user *buf, size_t size, loff_t *offset) {
	int len = strlen(message);
	
	read_p = !read_p;
	if (read_p)
		return 0;
		
	printk(KERN_INFO "proc called read\n");
	copy_to_user(buf, message, len);
	return len;
}

int time_proc_release(struct inode *sp_inode, struct file *sp_file) {
	printk(KERN_NOTICE "proc called release\n");
	kfree(message);
	return 0;
}

static int time_init(void) {
	printk(KERN_NOTICE "/proc/%s create\n",ENTRY_NAME);
	fops.open = time_proc_open;
	fops.read = time_proc_read;
	fops.release = time_proc_release;
	
	timed = current_kernel_time();
	previous = current_kernel_time();
	counter = 0;

	if (!proc_create(ENTRY_NAME, PERMS, NULL, &fops)) {
		printk(KERN_WARNING "proc create\n");
		remove_proc_entry(ENTRY_NAME, NULL);
		return -ENOMEM;
	}
	
	return 0;
}
module_init(time_init);

static void time_exit(void) {
	remove_proc_entry(ENTRY_NAME, NULL);
	printk(KERN_NOTICE "Removing /proc/%s\n", ENTRY_NAME);
}
module_exit(time_exit);
