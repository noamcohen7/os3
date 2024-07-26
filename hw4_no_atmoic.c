#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <threads.h>

// Function declarations
void initQueue(void);
void destroyQueue(void);
void enqueue(void*);
void* dequeue(void);
bool tryDequeue(void**);
size_t size(void);
size_t waiting(void);
size_t visited(void);

typedef struct queue_object {
    void *obj;
    struct queue_object* next;
} queue_object;

typedef struct thread_queue_object {
    cnd_t cond;
    struct thread_queue_object* next;
} thread_queue_object;

typedef struct queue {
    queue_object *head; // head is used to indicate the first object in the queue
    queue_object *tail; // tail is used to indicate the last object inserted in the queue
    size_t size;
    mtx_t mutex;
    thread_queue_object *thread_head; // head of the waiting threads queue
    thread_queue_object *thread_tail; // tail of the waiting threads queue
} queue;

queue* main_queue = NULL;
static size_t waiting_threads = 0;
static size_t inserted_elements = 0;

void initQueue(void) {
    /* This function is used to initialize the queue */
    main_queue = (queue*)malloc(sizeof(queue));
    main_queue->head = NULL;
    main_queue->tail = NULL;
    main_queue->size = 0;
    main_queue->thread_head = NULL;
    main_queue->thread_tail = NULL;
    waiting_threads = 0;
    inserted_elements = 0;
    mtx_init(&main_queue->mutex, mtx_plain);
}

void destroyQueue(void) {
    if (main_queue->size != 0) {
        free_elements_in_queue();    
    }
    mtx_destroy(&main_queue->mutex);
    free(main_queue);
}

void enqueue(void* obj) {
    queue_object *queue_obj = (queue_object*)malloc(sizeof(queue_object));
    queue_obj->obj = obj;
    queue_obj->next = NULL;

    // Lock the mutex
    mtx_lock(&main_queue->mutex);

    // If there is an item so the last item will point to the newly added last item
    if (main_queue->tail != NULL) {
        main_queue->tail->next = queue_obj;
    } else {
        // In case there are no items
        main_queue->head = queue_obj;
    }

    // If there is a thread waiting then we want to wake up the first thread
    if (main_queue->thread_head != NULL) {
        // Wake up the first waiting thread
        thread_queue_object* thread_obj = main_queue->thread_head;
        main_queue->thread_head = main_queue->thread_head->next;
        if (main_queue->thread_head == NULL) {
            main_queue->thread_tail = NULL;
        }
        cnd_signal(&thread_obj->cond);
    }

    main_queue->tail = queue_obj;
    main_queue->size++;
    inserted_elements++;

    mtx_unlock(&main_queue->mutex);
}

void* dequeue(void) {
    thread_queue_object thread_obj;
    cnd_init(&thread_obj.cond);
    thread_obj.next = NULL;

    mtx_lock(&main_queue->mutex);

    // If the queue is empty, add this thread to the waiting queue and wait
    if (main_queue->head == NULL) {
        // Add this thread to the waiting queue
        if (main_queue->thread_tail != NULL) {
            main_queue->thread_tail->next = &thread_obj;
        } else {
            main_queue->thread_head = &thread_obj;
        }
        main_queue->thread_tail = &thread_obj;
        waiting_threads++;

        // Wait for a signal
        cnd_wait(&thread_obj.cond, &main_queue->mutex);

        // Remove this thread from the waiting queue
        waiting_threads--;
    }

    // Get the first inserted object
    queue_object* temp = main_queue->head;
    void* obj = temp->obj;
    main_queue->head = main_queue->head->next;
    main_queue->size--;

    // If there was only one item then tail should be NULL as well
    if (main_queue->head == NULL) {
        main_queue->tail = NULL;
    }

    free(temp);
    mtx_unlock(&main_queue->mutex);
    cnd_destroy(&thread_obj.cond);
    return obj;
}

bool tryDequeue(void** obj) {
    mtx_lock(&main_queue->mutex);

    if (main_queue->head == NULL) { // Check if the queue is empty
        mtx_unlock(&main_queue->mutex);
        return false;
    }

    // Get the first inserted object
    queue_object* temp = main_queue->head;
    *obj = temp->obj;
    main_queue->head = main_queue->head->next;
    main_queue->size--;

    // If there was only one item then tail should be NULL as well
    if (main_queue->head == NULL) {
        main_queue->tail = NULL;
    }

    free(temp);
    mtx_unlock(&main_queue->mutex);
    return true;
}

size_t size(void) {
    return main_queue->size;
}

size_t waiting(void) {
    mtx_lock(&main_queue->mutex);
    int res = waiting_threads;
    mtx_unlock(&main_queue->mutex);
    return res;
}

size_t visited(void) {
    mtx_lock(&main_queue->mutex);
    int res = inserted_elements;
    mtx_unlock(&main_queue->mutex);
    return res;
}

void free_elements_in_queue() {
    /* Function is used to clear the elements inside the queue */
    queue_object *curr = main_queue->head;
    queue_object *next;
    while (curr != NULL) {
        next = curr->next;
        free(curr);
        curr = next;
    }
}
