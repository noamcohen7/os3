#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <threads.h>
#include <stdatomic.h>

typedef struct queue_object {
    void *obj;
    struct queue_object* next;
} queue_object;

typedef struct thread_queue_object {
    cnd_t cond;
    struct thread_queue_object* next;
} thread_queue_object;

typedef struct queue {
    queue_object *head;
    queue_object *tail;
    size_t size;
    size_t visited;
    atomic_size_t waiting_threads;
    mtx_t mutex;
    cnd_t cond;
    thread_queue_object *thread_head;
    thread_queue_object *thread_tail;
} queue;

static queue* main_queue = NULL;

void initQueue(void) {
    main_queue = (queue*)malloc(sizeof(queue));
    main_queue->head = NULL;
    main_queue->tail = NULL;
    main_queue->size = 0;
    main_queue->visited = 0;
    atomic_store(&main_queue->waiting_threads, 0);
    mtx_init(&main_queue->mutex, mtx_plain);
    cnd_init(&main_queue->cond);
    main_queue->thread_head = NULL;
    main_queue->thread_tail = NULL;
}

void destroyQueue(void) {
    if (main_queue != NULL) {
        free_elements_in_queue();
        cnd_destroy(&main_queue->cond);
        mtx_destroy(&main_queue->mutex);
        free(main_queue);
        main_queue = NULL;
    }
}

void enqueue(void* obj) {
    queue_object *queue_obj = (queue_object*)malloc(sizeof(queue_object));
    queue_obj->obj = obj;
    queue_obj->next = NULL;

    mtx_lock(&main_queue->mutex);

    if (main_queue->tail != NULL) {
        main_queue->tail->next = queue_obj;
    } else {
        main_queue->head = queue_obj;
    }

    if (main_queue->thread_head != NULL) {
        thread_queue_object* thread_obj = main_queue->thread_head;
        main_queue->thread_head = thread_obj->next;
        if (main_queue->thread_head == NULL) {
            main_queue->thread_tail = NULL;
        }
        cnd_signal(&thread_obj->cond);
        free(thread_obj); // Free the thread_obj after signaling
    }

    main_queue->tail = queue_obj;
    main_queue->size++;
    atomic_fetch_add(&main_queue->visited, 1);

    mtx_unlock(&main_queue->mutex);
}

void* dequeue(void) {
    queue_object* temp;
    void* obj;
    
    mtx_lock(&main_queue->mutex);
    atomic_fetch_add(&main_queue->waiting_threads, 1);

    while (main_queue->head == NULL) {
        thread_queue_object* thread_obj = (thread_queue_object*)malloc(sizeof(thread_queue_object));
        cnd_init(&thread_obj->cond);
        thread_obj->next = NULL;
        
        if (main_queue->thread_tail == NULL) {
            main_queue->thread_head = thread_obj;
        } else {
            main_queue->thread_tail->next = thread_obj;
        }
        main_queue->thread_tail = thread_obj;

        cnd_wait(&thread_obj->cond, &main_queue->mutex);
        cnd_destroy(&thread_obj->cond);
        free(thread_obj);
    }

    atomic_fetch_sub(&main_queue->waiting_threads, 1);

    temp = main_queue->head;
    obj = temp->obj;
    main_queue->head = main_queue->head->next;
    main_queue->size--;

    if (main_queue->head == NULL) {
        main_queue->tail = NULL;
    }

    free(temp);
    mtx_unlock(&main_queue->mutex);
    return obj;
}

bool tryDequeue(void** obj) {
    bool success = false;

    mtx_lock(&main_queue->mutex);

    if (main_queue->head != NULL) {
        queue_object* temp = main_queue->head;
        *obj = temp->obj;
        main_queue->head = main_queue->head->next;
        main_queue->size--;

        if (main_queue->head == NULL) {
            main_queue->tail = NULL;
        }

        free(temp);
        success = true;
    }

    mtx_unlock(&main_queue->mutex);
    return success;
}

size_t size(void) {
    return main_queue->size;
}

size_t waiting(void) {
    return atomic_load(&main_queue->waiting_threads);
}

size_t visited(void) {
    return atomic_load(&main_queue->visited);
}

void free_elements_in_queue(void) {
    queue_object* curr = main_queue->head;
    queue_object* next;
    while (curr != NULL) {
        next = curr->next;
        free(curr);
        curr = next;
    }
}
