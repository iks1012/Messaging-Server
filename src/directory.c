#include "directory.h"
#include "mailbox.h"
#include "debug.h"
#include <semaphore.h>
#include <string.h>
#include <sys/socket.h>



//STRUCTS
//The head will always be empty
typedef struct directory_node {
	char* handle;
	int sockfd;
	MAILBOX* mailbox;
	int isDefunct;
	struct directory_node* next;
} directory_node;


//HELPER FUNCTION DECLARATIONS
void dir_free(directory_node*);


//GLOBAL VARIABLES
sem_t mutex;
struct directory_node * directory_head;
int num_nodes;

extern sem_t mutex2;
extern sem_t mutex3;


/*
 * Initialize the directory.
 */
void dir_init(void){
	directory_head = malloc(sizeof(directory_node));
	sem_init(&mutex , 0, 1);
	sem_init(&mutex2 , 0, 1);
	sem_init(&mutex3 , 0, 1);
	sem_wait(&mutex);
	num_nodes = 0;
	directory_head->mailbox = NULL;
	directory_head->sockfd = -1;
	directory_head->handle = NULL;
	directory_head->next = NULL;
	directory_head->isDefunct = 0;
	sem_post(&mutex);
}



/*
 * Shut down the directory.
 * This marks the directory as "defunct" and shuts down all the client sockets,
 * which triggers the eventual termination of all the server threads.
 */
void dir_shutdown(void){
	sem_wait(&mutex);
	directory_node* ptr = directory_head;
	while(ptr != NULL){
		shutdown(ptr->sockfd, SHUT_RDWR); //as per the spec;
		ptr->isDefunct = 1;
		ptr = ptr->next;
	}

	sem_post(&mutex);

}

/*
 * Finalize the directory.
 *
 * Precondition: the directory must previously have been shut down
 * by a call to dir_shutdown().
 */
void dir_fini(void){
	dir_free(directory_head);
	sem_destroy(&mutex);
	sem_destroy(&mutex2);
	sem_destroy(&mutex3);
}

/*
	
	recursively shuts down all the directories
		Have to do it this way because in order to
		most efficiently (one-pass) free a singly linked list 
		is to do it recursively
	dont need to make this thread safe as this 
	will only be called within a semaphore 
	-> I made it so I know it is not being called from your library ;)
*/
void dir_free(directory_node* ptr){
	if(ptr->next != NULL){
		dir_free(ptr->next);
	}

	num_nodes -= 1;
	mb_shutdown(ptr->mailbox);
	ptr->next = NULL;
	free(ptr);
	return;
}

/*
 * Register a handle in the directory.
 *   handle - the handle to register
 *   sockfd - file descriptor of client socket
 *
 * Returns a new mailbox, if handle was not previously registered.
 * Returns NULL if handle was already registered or if the directory is defunct.
 */
MAILBOX *dir_register(char *handle, int sockfd){
	MAILBOX* returnThis = dir_lookup(handle);//this doesnt need mutex dir_lookup has it.
	sem_wait(&mutex);
	if(returnThis != NULL){
		mb_unref(returnThis);
		returnThis = NULL;
	}
	else{
		directory_node* ptr = directory_head;
		ptr->mailbox = mb_init(handle);
		mb_ref(ptr->mailbox);
		ptr->handle = malloc(sizeof(char) * strlen(handle));
		strcpy(ptr->handle, handle);
		ptr->sockfd = sockfd;
		returnThis = ptr->mailbox;
		//Create new Node and make the new head
		directory_node* new_node = malloc(sizeof(directory_node));
		new_node->mailbox = NULL;
		new_node->sockfd = -1;
		new_node->handle = NULL;
		new_node->next = ptr;
		new_node->isDefunct = 0;
		num_nodes += 1;
	}
	sem_post(&mutex);
	return returnThis;	
}

/*
 * Unregister a handle in the directory, only one specific handle.
 * The associated mailbox is removed from the directory and shut down.
 * -- This only unregisters it if it is even there
 */
void dir_unregister(char *handle){
	sem_wait(&mutex);
	directory_node* ptr = directory_head;
	int node_found = 0;
	while(ptr->next != NULL && !node_found){
		if(!strcmp(handle, ptr->next->handle)){ //strcmp returns 0 if the strings are equal
			mb_unref(ptr->next->mailbox);//as per the spec
			mb_shutdown(ptr->next->mailbox);
			free(ptr->next);
			ptr->next = ptr->next->next;
			node_found = 1;
			num_nodes -= 1;
		}
		else{
			ptr = ptr->next;
		}
	}
	sem_post(&mutex);
}

/*
 * Query the directory for a specified handle.
 * If the handle is not registered, NULL is returned.
 * If the handle is registered, the corresponding mailbox is returned.
 * The reference count of the mailbox is increased to account for the
 * pointer that is being returned.  It is the caller's responsibility
 * to decrease the reference count when the pointer is ultimately discarded.
 */
MAILBOX *dir_lookup(char *handle){
	sem_wait(&mutex);
	directory_node* ptr = directory_head->next;

	while(ptr != NULL){
		if(!strcmp(handle, ptr->handle)){ //strcmp returns 0 if the strings are equal
			mb_ref(ptr->mailbox); //calls it as per the spec
			return ptr->mailbox;
		}
		ptr = ptr->next;
	}
	sem_post(&mutex);
	return NULL;
}

/*
 * Obtain a list of all handles currently registered in the directory.
 * Returns a NULL-terminated array of strings.
 * It is the caller's responsibility to free the array and all the strings
 * that it contains.
 */
char **dir_all_handles(void){
	sem_wait(&mutex);
	directory_node* ptr = directory_head->next;
	char** handles = malloc(sizeof(char*) * num_nodes);
	for(int i = 0; i < num_nodes; i++){
		handles[i] = ptr->handle;
		ptr = ptr->next;
	}
	sem_post(&mutex);
	return handles;
}



