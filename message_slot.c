#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE


#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/slab.h>
//#include <errno.h>

MODULE_LICENSE("GPL");

#include "message_slot.h"

static message_slot_list channels_list[257]; // Supports 256 beacuse 0 is not a valid channel id
static char the_message[BUF_LEN]; // The message the device will give when asked

typedef struct {
    message_slot_node *node;
    bool found; // true if a new node was created, false if an existing node was found
} channel_result;

channel_result *get_channel(int minor_num, int channel_id);

static int device_open(struct inode* inode, struct file*  file) {
    printk("Device open was called successfully");
	return SUCCESS;
}


static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned long  ioctl_param )
{
    channel_result node_info;
    int minor_num = iminor(file->f_inode);
    message_slot_node * new_channel_node;
    if (ioctl_command_id != MSG_SLOT_CHANNEL){
        printk("Invalid param ioctl_command_id provided: %d \n", ioctl_command_id);
        return -22;
    }
    if (ioctl_param <= 0 || ioctl_param > 256){
        printk("Invalid param ioctl_param provided: %d \n", ioctl_param);
        return -22;
    }

    node_info = *get_channel(minor_num, ioctl_param);
    
    // Did not find node for this channel id - allocating one
    if (node_info.found == false)
    {
        new_channel_node = (message_slot_node *)kmalloc(sizeof(message_slot_node), GFP_KERNEL);
        if (new_channel_node == NULL){
            printk("Failed allocating memory\n");
            return -12;
        }
        
        new_channel_node->channel_id = ioctl_param;
        new_channel_node->next = NULL;

        if (node_info.node == NULL){
            printk("Creating new channel for minor num: %d with channel id: %d", minor_num, ioctl_param);
            channels_list[minor_num].head = new_channel_node;
            printk("Successfully alocated the new channel for head");
        }
        else{
            printk("Creating new channel: %d for existing minor_num: %d", ioctl_param, minor_num);
            node_info.node->next = new_channel_node;
        }   
    }
    else{
        new_channel_node = node_info.node;
    }
    printk("Allocating new channel to file private data");
    file->private_data = new_channel_node;
    printk("Allocated the file private data");

    printk("Freeing node info");
    kfree(node_info);
    printk("Freed node info");
    //printk("Ioctl of channel %d of minor %d\n", new_channel_node->channel_id, minor_num);// TODO change this 
    return SUCCESS;
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read( struct file* file,
                            char __user* buffer,
                            size_t       length,
                            loff_t*      offset )
{
    message_slot_node * channel = (message_slot_node *)file->private_data;

    // No valid channel
    if (channel == NULL){
        printk("Provided fd does not contain valid channel");
        return -22;
    }
    int channel_id = channel->channel_id;
    int minor_num = iminor(file->f_inode);
    // No message exists on the channel
    if (channel->msg_length == 0){
        printk("Given minor: %d, provided channel %d does not contain any message %d\n", minor_num, channel->channel_id);
        return -35;
    }
    if (length < channel->msg_length){
        printk("Given minor: %d, provided channel %d last message is too large for provided buffer %d\n", minor_num, channel->channel_id);
        return -28;
    }
    if (buffer == NULL){
        printk("Null pointer provided for buffer");
    }

    if (copy_to_user(buffer, channel->message, length) != 0) {
        printk("copy_to_user failed\n");
        return -55;
    }
    return length;
}

//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write( struct file*       file,
                             const char __user* buffer,
                             size_t             length,
                             loff_t*            offset)
{
    message_slot_node * channel = (message_slot_node *)file->private_data;

    // No valid channel
    if (channel == NULL){
        printk("Provided fd does not contain valid channel");
        return -22;
    }
    if (length == EMPTY_BUF || length > BUF_LEN){
        printk("Given length: %d is invalid should be between: 1-%d", length, BUF_LEN);
        return 40;
    }

    ssize_t i, j;
    printk("Invoking device_write(%p,%ld)\n", file, length);
    for( i = 0; i < length && i < BUF_LEN; ++i ) {
        if (get_user(the_message[i], &buffer[i]) != 0){
            printk("get_user failed\n");
            return -55;
        }
    }
    printk("Number of written bytes is: %zd", i);
    channel->msg_length = i;
    for (j = 0; j < i; j++){
        channel->message[i] = the_message[i];
    }
    printk("Message wrote to channel: %d with minor: %d, length of message is: %d", channel->channel_id, iminor(file->f_inode), 
    channel->msg_length);

    printk("Write message is: %s", channel->message);
    return i;
}


channel_result *get_channel(int minor_num, int channel_id){
    message_slot_node *cur_node = channels_list[minor_num].head;
    channel_result *res = kmalloc(sizeof(channel_result), GFP_KERNEL);
    res->found = false;
    if (cur_node != NULL){

        // Run until we have the currect node
        while (cur_node != NULL){
            if (cur_node->channel_id == channel_id){
                res->found = true;
                break;
            }
            if (cur_node->next == NULL) {
                break;
            }
            cur_node = cur_node->next;
        }
    }
    res->node = cur_node;
    return res;
}

void clean_list(message_slot_node *curr){
    message_slot_node *prev;
    while (curr != NULL){
        prev = curr;
        curr = curr->next;
        kfree(prev);
    }
    return;
}

static int device_release(struct inode* inode, struct file*  file) {
	kfree(file->private_data);
    printk("Freed the file private data");
	return SUCCESS;
}


//==================== DEVICE SETUP =============================

// This structure will hold the functions to be called
// when a process does something to the device we created
struct file_operations Fops = {
  .owner	  = THIS_MODULE, 
  .read           = device_read,
  .write          = device_write,
  .open           = device_open,
  .unlocked_ioctl = device_ioctl,
  .release        = device_release,
};

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init simple_init(void)
{
    int rc = -1;
    int i;
    
    // Register driver capabilities. Obtain major num
    rc = register_chrdev( MAJOR_NUM, DEVICE_NAME, &Fops);

    // Negative values signify an error
    if( rc < 0 ) {
    printk( KERN_ALERT "%s registraion failed for  %d\n",
                        DEVICE_FILE_NAME, MAJOR_NUM );
    return rc;
    }
    // initiate list for every minor number possible
    for(i=0;i<257;i++) {
        channels_list[i].head = NULL;
    }

    printk( "Registeration is successful. ");
    printk( "If you want to talk to the device driver,\n" );
    printk( "you have to create a device file:\n" );
    printk( "mknod /dev/%s c %d 0\n", DEVICE_FILE_NAME, MAJOR_NUM );
    printk( "You can echo/cat to/from the device file.\n" );
    printk( "Dont forget to rm the device file and "
            "rmmod when you're done\n" );

    return 0;
}

//---------------------------------------------------------------
static void __exit simple_cleanup(void)
{   
    int i;

    for (i = 0; i < 257; i++){
        clean_list(channels_list[i].head);
    }
    printk( "Unregisteration is successful. ");
    // Unregister the device
    // Should always succeed
    unregister_chrdev(MAJOR_NUM, DEVICE_NAME);
}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);

//========================= END OF FILE =========================
