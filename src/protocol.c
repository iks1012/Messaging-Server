#include "debug.h"
#include "protocol.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/*
 * Send a packet with a specified header and payload.
 *   fd - file descriptor on which packet is to be sent
 *   hdr - the packet header, with multi-byte fields in host byte order
 *   payload - pointer to packet payload, or NULL, if none.
 *
 * Multi-byte fields in the header are converted to network byte order
 * before sending.  The header structure passed to this function may
 * be modified as a result.
 *
 * On success, 0 is returned.
 * On error, -1 is returned and errno is set.
 */
int proto_send_packet(int fd, bvd_packet_header *hdr, void *payload){
	//write payload to fd
	//get length of payload from header
	if(hdr == NULL){

	debug("HI");
		return -1;
	}

	//get size;
	uint32_t size = hdr-> payload_length;

	hdr->payload_length = htonl(hdr->payload_length);
	hdr->timestamp_sec 	= htonl(hdr->timestamp_sec);
	hdr->timestamp_nsec = htonl(hdr->timestamp_nsec);

	if(hdr->msgid == 0)
		hdr->msgid 			= (hdr->timestamp_nsec); //Unique ID

	//Write header
	if(write(fd, (void*)hdr, sizeof(bvd_packet_header)) != sizeof(bvd_packet_header)){
		return -1;
	}

	//write payload if the size is non 0
	if(size > 0){
		if(write(fd, payload, size) != size){
			return -1;
		}
	}
	return 0;
}

/*
 * Receive a packet, blocking until one is available.
 *  fd - file descriptor from which packet is to be received
 *  hdr - pointer to caller-supplied storage for packet header
 *  payload - variable into which to store payload pointer
 *
 * The returned header has its multi-byte fields in host byte order.
 *
 * If the returned payload pointer is non-NULL, then the caller
 * is responsible for freeing the storage.
 *
 * On success, 0 is returned.
 * On error, -1 is returned, payload and length are left unchanged,
 * and errno is set.
 */
int proto_recv_packet(int fd, bvd_packet_header *hdr, void **payload){	

	//recv Header
	
	int t = -1;
	if((t = read(fd, (void*)hdr, sizeof(bvd_packet_header))) < 0){
		return -1;
	}
	


	hdr->payload_length = ntohl(hdr->payload_length);
	hdr->timestamp_nsec = ntohl(hdr->timestamp_nsec);
	hdr->timestamp_sec 	= ntohl(hdr->timestamp_sec);
	hdr->msgid			= (hdr->timestamp_nsec);
	//Add Payload if needed
	uint32_t size = hdr->payload_length;

	if(size > 0){

		
		char* input = malloc(size*sizeof(char));

		*payload = malloc(size*sizeof(char));
		*(((char**)payload)+1) = malloc(size*sizeof(char));


		if((t = read(fd, input, size)) != size){
			return -1;
		}
		int rFound = 0;
		int new_line = 0;
		char* ptr = input;
		int i = 0;
		for(; i < size && !new_line; i++){
			if(rFound && *ptr == '\n'){
				new_line = 1;
			}
			else{
				rFound = 0;
			}
			if(*ptr == '\r'){
				rFound = 1;
			}
			ptr++;
		}

		memcpy( *payload , input , i*sizeof(char));
		if(i+1 < size){
			memcpy( *(((char**)payload)+1), input+i+1, (size-i-1) * sizeof(char));
		}
		

		free(input);
	}

	debug("Payload: %s", (char*)*payload);
	return 0;
}




