
#include "mailbox.h"
#include "debug.h"



#include <semaphore.h>
#include <string.h>
#include <sys/socket.h>


//STRUCTS
typedef struct mailbox_node {
	MAILBOX_ENTRY* mb_entry;
	int msgid;
	struct mailbox_node* next;
} MB_NODE;


typedef struct mailbox {
	MB_NODE* head;
	MB_NODE* tail;
	sem_t mutex;

	sem_t ref_mutex;
	char* handle;
	int ref_cnt;
	int is_defunct;
} MAILBOX;


//FUNCTION DECLARATIONS
MB_NODE* make_new_node(int msgid, void* body, int length);




//GLOBAL VARIABLES
sem_t mutex2;
sem_t mutex3;



/*
 * Create a new mailbox for a given handle.
 * The mailbox is returned with a reference count of 1.
 */
MAILBOX *mb_init(char *handle){
	sem_wait(&mutex2);
	MAILBOX* mb = malloc(sizeof(MAILBOX));
	mb->head = NULL;
	mb->tail = NULL;
	mb->handle = handle;

	sem_init(&mb->mutex , 0, 1);
	sem_wait(&mb->mutex);	

	sem_init(&mb->ref_mutex , 0, 1);
	sem_wait(&mb->ref_mutex);


	mb->ref_cnt = 1;
	mb->is_defunct = 0;
	sem_post(&mutex2);
	return mb;
}

/*
 * Set the discard hook for a mailbox.
 */
void mb_set_discard_hook(MAILBOX *mb, MAILBOX_DISCARD_HOOK * mb_dh){

}

/*
 * Increase the reference count on a mailbox.
 * This must be called whenever a pointer to a mailbox is copied,
 * so that the reference count always matches the number of pointers
 * that exist to the mailbox.
 */
void mb_ref(MAILBOX *mb){
	sem_wait(&mutex2);

	if(mb->ref_cnt == 0){
		sem_wait(&mb->ref_mutex);
	}
	mb->ref_cnt += 1;
	sem_post(&mutex2);
}

/*
 * Decrease the reference count on a mailbox.
 * This must be called whenever a pointer to a mailbox is discarded,
 * so that the reference count always matches the number of pointers
 * that exist to the mailbox.  When the reference count reaches zero,
 * the mailbox will be finalized.
 */
void mb_unref(MAILBOX *mb){
	sem_wait(&mutex2);
	mb->ref_cnt -= 1;
	if(mb->ref_cnt == 0){
		sem_post(&mb->ref_mutex);
	}
	sem_post(&mutex2);
}

/*
 * Shut down this mailbox.
 * The mailbox is set to the "defunct" state.  A defunct mailbox should
 * not be used to send any more messages or notices, but it continues
 * to exist until the last outstanding reference to it has been
 * discarded.  At that point, the mailbox will be finalized, and any
 * entries that remain in it will be discarded.
 */
void mb_shutdown(MAILBOX *mb){
	mb->is_defunct = 1;
	sem_wait(&mb->ref_mutex);
	sem_post(&mb->ref_mutex);
	//discard all the entries

	MB_NODE* ptr = mb->head;

	while(ptr!=NULL){
		free(ptr->mb_entry->body);
		ptr = ptr->next;
		mb->head = mb->head->next;
	}

	free(mb->handle);
	sem_destroy(&mb->ref_mutex);
	sem_destroy(&mb->mutex);
}

/*
 * Get the handle associated with a mailbox.
 */
char *mb_get_handle(MAILBOX *mb){
	sem_wait(&mutex2);
	char* return_this = NULL;
	if(mb != NULL){
		return_this = mb -> handle;
	}
	sem_post(&mutex2);
	return return_this;
}

/*
 * Add a message to the end of the mailbox queue.
 *   msgid - the message ID
 *   from - the sender's mailbox
 *   body - the body of the message, which can be arbitrary data, or NULL
 *   length - number of bytes of data in the body
 *
 * The message body must have been allocated on the heap,
 * but the caller is relieved of the responsibility of ultimately
 * freeing this storage, as it will become the responsibility of
 * whomever removes this message from the mailbox.
 *
 * The reference to the sender's mailbox ("from") is conceptually
 * "transferred" from the caller to the new message, so no increase in
 * the reference count is performed.  However, after the call the
 * caller must discard this pointer which it no longer "owns".
 */
void mb_add_message(MAILBOX *mb, int msgid, MAILBOX *from, void *body, int length){
	sem_wait(&mutex2);
	if(!mb->is_defunct){
		//adding the info to an appropraitely existing node.
		MB_NODE* ptr = mb->head;
		int id_match = 0;
		while(ptr != NULL && !id_match){
			if(ptr->mb_entry->content.notice.msgid == msgid){
				ptr->mb_entry->body = body;
				ptr->mb_entry->length = length;
				MESSAGE msg = {msgid, from};
				memcpy(&(ptr->mb_entry->content.message), &msg, sizeof(MESSAGE));
				id_match = 1;
			}
			ptr = ptr->next;
		}

		debug("id_match: %d \n", id_match);

		if(!id_match){
			//append the message node to the head

			//allocate the new node
			MB_NODE* new_node = make_new_node(msgid, body, length);

			//copy the message into the entry
			MESSAGE msg ;
			msg.msgid = msgid;
			msg.from = from;
			new_node->mb_entry->content.message = msg;


		
		debug("id_match: %d \n", id_match);
		
			
			//Append node to tail
			if(mb->head == NULL){
				new_node->next = mb->head;

				mb->head = new_node;
				mb->tail = new_node;
			}
			else{
				mb->tail->next = mb->head;
				mb->tail = new_node;
			}
			sem_post(&mb->mutex);
		}


		debug("id_match: %d \n", id_match);
	}
	
	sem_post(&mutex2);
}

/*
 * Add a notice to the end of the mailbox queue.
 *   ntype - the notice type
 *   msgid - the ID of the message to which the notice pertains
 *   body - the body of the notice, which can be arbitrary data, or NULL
 *   length - number of bytes of data in the body
 *
 * The notice body must have been allocated on the heap, but the
 * caller is relieved of the responsibility of ultimately freeing this
 * storage, as it will become the responsibility of whomever removes
 * this notice from the mailbox.
 */
void mb_add_notice(MAILBOX *mb, NOTICE_TYPE ntype, int msgid, void *body, int length){
	sem_wait(&mutex2);
	if(!mb->is_defunct){
		//adding the info to an appropraitely existing node.
		MB_NODE* ptr = mb->head;
		int id_match = 0;

		while(ptr != NULL && !id_match){
			if(ptr->mb_entry->content.message.msgid == msgid){
				ptr->mb_entry->body = body;
				ptr->mb_entry->length = length;
				NOTICE notice = {ntype, msgid};
				memcpy(&(ptr->mb_entry->content.notice), &notice, sizeof(NOTICE));
				id_match = 1;
			}
			ptr = ptr->next;
		}


		
		debug("id_match: %d \n", id_match);

		if(!id_match){
			MB_NODE* new_node = make_new_node(msgid, body, length);


			
			NOTICE notice;
			notice.type = ntype;
			notice.msgid = msgid;

			new_node->mb_entry->content.notice = notice;

			//Append Node to tail
			if(mb->head == NULL){
				new_node->next = mb->head;
				mb->head = new_node;
				mb->tail = new_node;
			}
			else{
				mb->tail->next = mb->head;
				mb->tail = new_node;
			}



			//sem_post(&mb->mutex);
		}
	}

	

	sem_post(&mutex2);
}

MB_NODE* make_new_node(int msgid, void* body, int length){
	sem_wait(&mutex3);
	MB_NODE* new_node = malloc(sizeof(MB_NODE));
	new_node->msgid = msgid;

	//allocate and assign the entry's info
	new_node->mb_entry = malloc(sizeof(MAILBOX_ENTRY));
	new_node->mb_entry->body = body;
	new_node->mb_entry->length = length;
	sem_post(&mutex3);
	return new_node;
}

/*
 * Remove the first entry from the mailbox, blocking until there is
 * one.  The caller assumes the responsibility of freeing the entry
 * and its body, if present.  In addition, if it is a message entry,
 * the caller must decrease the reference count on the "from" mailbox
 * to account for the destruction of the pointer.
 *
 * This function will return NULL in case the mailbox is defunct.
 * The thread servicing the mailbox should use this as an indication
 * that service should be terminated.
 */
MAILBOX_ENTRY *mb_next_entry(MAILBOX *mb){
	sem_wait(&mb->mutex);
	//dequeue
	MAILBOX_ENTRY* return_this;
	if(mb->is_defunct){
		return_this = NULL;
	}
	else{
		return_this = mb->head->mb_entry;
		mb->head = mb->head->next;
	}
	sem_post(&mb->mutex);
	return return_this;
}









