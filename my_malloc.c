#include "my_malloc.h"

/* You *MUST* use this macro when calling my_sbrk to allocate the
 * appropriate size. Failure to do so may result in an incorrect
 * grading!
 */
#define SBRK_SIZE 2048

/* Please use this value as your canary! */
#define CANARY 0x2110CAFE

/* If you want to use debugging printouts, it is HIGHLY recommended
 * to use this macro or something similar. If you produce output from
 * your code then you may receive a 20 point deduction. You have been
 * warned.
 */
#ifdef DEBUG
#define DEBUG_PRINT(x) printf x
#else
#define DEBUG_PRINT(x)
#endif

/* Our freelist structure - This is where the current freelist of
 * blocks will be maintained. Failure to maintain the list inside
 * of this structure will result in no credit, as the grader will
 * expect it to be maintained here.
 *
 * Technically this should be declared static for the same reasons
 * as above, but DO NOT CHANGE the way this structure is declared
 * or it will break the autograder.
 */
metadata_t* freelist;
#define METADATASIZE (sizeof(metadata_t)+2*sizeof(int))		//2 canaries and metadata

void* my_malloc(size_t size){
	size_t trueSize = size + METADATASIZE;	// true amt of space needed

	if(trueSize > SBRK_SIZE){		//asked for too much
		ERRNO = SINGLE_REQUEST_TOO_LARGE;
		return NULL;
	}else{
		//no free data
		if(NULL == freelist){
			void* newSBRK = my_sbrk(SBRK_SIZE);
			if (NULL == newSBRK){
				ERRNO = OUT_OF_MEMORY;
				return NULL;
			}else{
				metadata_t mData = {SBRK_SIZE, 0, NULL, NULL};
				*((metadata_t*)newSBRK) = mData;
				freelist = newSBRK;
			}
		}

		//find smallest usable block
		metadata_t* active = freelist;
		metadata_t* prev = NULL;
		metadata_t* min = NULL;
		while(active != NULL) {
			if(active->block_size > trueSize){
				if(min == NULL || min->block_size > active->block_size)
					min = active;
			}
			prev = active;
			active = active->next;
		}


		if(NULL == min){
			void* newSBRK = my_sbrk(SBRK_SIZE);
			if (NULL == newSBRK){
				ERRNO = OUT_OF_MEMORY;

				return NULL;
			}else{
				metadata_t mData = {trueSize, size, NULL, NULL};
				if(trueSize + METADATASIZE + 1 < SBRK_SIZE) {
					metadata_t pAlloc = {SBRK_SIZE - trueSize, 0, NULL, prev};
					*((metadata_t*)(((char*) newSBRK) + METADATASIZE + size)) = pAlloc;
					if(prev != NULL)
						prev->next = ((metadata_t*)(((char*) newSBRK) + METADATASIZE + size));
				}else{
					mData.block_size = SBRK_SIZE;
				}

				*((metadata_t*)newSBRK) = mData;
				*((int*)(((char*) newSBRK) + sizeof(metadata_t))) = CANARY;
				*((int*)(((char*) newSBRK) + sizeof(metadata_t) + sizeof(int) + size)) = CANARY;

				ERRNO = NO_ERROR;

				return ((void*)(((char*) newSBRK) + METADATASIZE - sizeof(int)));
			}
		}else{
			if(trueSize + METADATASIZE + 1 < min->block_size){

				metadata_t pAlloc = {min->block_size - trueSize, 0, min->next, min->prev};
				*((metadata_t*)(((char*) min) + METADATASIZE + size)) = pAlloc;

				if(min->prev != NULL)
					min->prev->next = ((metadata_t*)(((char*) min) + METADATASIZE + size));

				if(min->next != NULL)
					min->next->prev = ((metadata_t*)(((char*) min) + METADATASIZE + size));

				if(freelist == min)
					freelist = ((metadata_t*)(((char*) min) + METADATASIZE + size));

				min->block_size = trueSize;

			}else{
				if(min->prev != NULL) 
					min->prev->next = NULL;
			}
			if(freelist == min) 
				freelist = min->next;

			//Set the canaries
			*((int*)(((char*) min) + sizeof(metadata_t))) = CANARY;
			*((int*)(((char*) min) + (sizeof(metadata_t) + sizeof(int) + size))) = CANARY;
			min->request_size = size;
			ERRNO = NO_ERROR;

			return ((void*)(((char*) min) + METADATASIZE - sizeof(int)));
		}
	}

  return NULL;
}

void my_free(void* ptr){
	metadata_t* metaDataPtr = ((metadata_t*)(((char*) ptr) - sizeof(int) - sizeof(metadata_t)));		//grab the metadata

	//check first canary
	int* canary1 = ((int*)(((char*) metaDataPtr) + sizeof(metadata_t)));
	if(*canary1 != CANARY) {
		ERRNO = CANARY_CORRUPTED;

		return;
	}else{
		//check second canary
		int* canary2 = ((int*)(((char*) metaDataPtr) + metaDataPtr->block_size - sizeof(int)));
		if(*canary2 != CANARY) {
			ERRNO = CANARY_CORRUPTED;
			
			return;
		}
	}

	//see if we can merge stuff
	metadata_t* active = freelist;
	metadata_t* prev = NULL;
	metadata_t* postmData = ((metadata_t*)(((char*) metaDataPtr) + metaDataPtr->block_size));
	int merged = 0;
	while(active != NULL){
		metadata_t* nextmData = ((metadata_t*)(((char*) active) + active->block_size));

		if(nextmData == metaDataPtr){			//active is right before freed memory
			if(nextmData == freelist){
				freelist = active;
				freelist->prev->next = NULL;
				freelist->prev = NULL;
			}

			//merge stuff together
			merged = 1;
			active->block_size += metaDataPtr->block_size;
			metaDataPtr->block_size = 0;
			metaDataPtr->request_size = 0;
			metaDataPtr->prev = NULL;
			metaDataPtr->next = NULL;
			prev = active;
			active = active->next;

		}else if(postmData == active){			//active is right after freed memory
			
			metadata_t* next = active->next;
			//merge if not merged before
			if(!merged){
				merged = 1;
				metaDataPtr->block_size += active->block_size;
				metaDataPtr->prev = active->prev;

				if(active->prev != NULL)
					active->prev->next = metaDataPtr;

				metaDataPtr->next = active->next;

				if(active->next != NULL)
					active->next->prev = metaDataPtr;

				if(freelist == active)
					freelist = metaDataPtr;

				active->block_size = 0;
				metaDataPtr->request_size = 0;
				active->next = NULL;
				active->prev = NULL;
			}else{
				//merge with block before freed memory
				active->prev->block_size += active->block_size;
				active->prev->next = active->next;

				if(active->next != NULL)
					active->next->prev = active->prev;
				
				active->block_size = 0;
				active->next = NULL;
				active->prev = NULL;
			}

			if(next != NULL)
				prev = next->prev;
			active = next;

		}else{
			prev = active;
			active = active->next;
		}
	}
	//no merges, add memory to freelist
	if(!merged) {
		metaDataPtr->request_size = 0;
		metaDataPtr->next = NULL;
		if(prev != NULL) {
			prev->next = metaDataPtr;
			metaDataPtr->prev = prev;
		}else{
			freelist = metaDataPtr;
		}
	} 
}

// /* MAYBE ADD SOME HELPER FUNCTIONS HERE? */

