#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H

#include <linux/ioctl.h>
#include <stdbool.h>


// The major device number.
#define MAJOR_NUM 235

// Set the message of the device driver
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned int)

#define DEVICE_NAME "message_slot"
#define DEVICE_FILE_NAME "simple_message_slot"
#define BUF_LEN 128
#define EMPTY_BUF 0
#define SUCCESS 0

// Struct used to describe a node of a given channel id
typedef struct message_slot_node
{
    unsigned int channel_id;
    char message[BUF_LEN];
    int msg_length;
    struct message_slot_node *next;
}message_slot_node;

// Struct used to describe the list of nodes
typedef struct 
{
    message_slot_node *head;
}message_slot_list;

#endif
