
#include <zmq.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include "global.h"
#include "md5.h"

#define MD 5
#define MD5_CTX MD5_CTX
#define MDInit MD5Init
#define MDUpdate MD5Update
#define MDFinal MD5Final

typedef enum { false, true } bool;

struct workerThreadData {
	int increment;
	int wordLength;
	char* startWord;
	char* endWord;
	char* match;
	int threadID;
};

void wordIncrement(char* theWord, int increment, int index) {
	if (theWord[index] < 126 + increment) {
		theWord[index] += increment;
		return;
	}
	else {
		theWord[index] = (theWord[index] + increment) % 126;
		wordIncrement(theWord, 1, index + 1);
	}
}

// The method run in each thread to loop through the words and check hash results.
void* workerThread(void* data) {
					// initialize data
	struct workerThreadData *inputs;
	inputs = (struct workerThreadData*) data;
	char* currentWord = (char*)malloc(sizeof(char) * strlen(inputs->startWord + 1));
	strcpy(currentWord, inputs->startWord);
	char* currentHash;
	MD5_CTX context;
	unsigned char digest[16];

	wordIncrement(currentWord, inputs->threadID, strlen(currentWord));	// pre-increment by thread # to prevent all threads running same words.
	// printf(".. %d\n", inputs->threadID);
	while (true) {
		currentHash = (char*)malloc(sizeof(char) * strlen(inputs->match + 1));
		unsigned int len = strlen(currentWord);
		MDInit(&context);
		MDUpdate(&context, currentWord, len);
		MDFinal(digest, &context);
		for (int i = 0; i < 16; i++) {
			char twoHex[2];
			sprintf(twoHex, "%02x", digest[i]);
			strcat(currentHash, twoHex);
		}
		if (strcmp(currentHash, inputs->match) == 0) {					// if match has been found.
			//printf("worker found match.\tword is: %s\n", currentWord);
			//printf("orig hash = %s\t new hash = %s\n", inputs->match, currentHash);
			char* result = (char*)malloc(sizeof(char) * (strlen(currentWord) + 1));
			strcpy(result, currentWord);
			//strcat(result, '/0');
			free(currentWord);
			free(currentHash);
			pthread_exit(result);
			// return match somehow
		}
		int comp = strcmp(currentWord, inputs->endWord);
		if (comp >= 0) {		// return failure to find match, and done.
			free(currentWord);
			free(currentHash);
			pthread_exit(NULL);
		}
		else {					// increment to next word.
			wordIncrement(currentWord, inputs->increment, 0);
		}
		currentHash[0] = '\0';
		free(currentHash);
	}
	pthread_exit(NULL);
}

// 
int errCheck(int result) {
	if (result != 0) {
		fprintf(stderr, "error reading from file.\n");
		return EXIT_FAILURE;
	}
}

int main(void)
{
	char buff[255];
	printf("worker started.\n");

	void *context = zmq_ctx_new();
	void *receiver = zmq_socket(context, ZMQ_PULL);		//  Socket to receive messages on
	zmq_bind(receiver, "tcp://*:6534");
	void *sender = zmq_socket(context, ZMQ_PUSH);		//  Socket to send results to
	zmq_connect(sender, "tcp://localhost:6535");

	printf("worker waiting...\n");
	int result = zmq_recv(receiver, buff, 255, 0);		// receive # of threads to use
	errCheck(result);
	int numThreads = atoi(buff);
	printf("Recieved numthr: %d\n", numThreads);

	result = zmq_recv(receiver, buff, 255, 0);			// receive password length
	errCheck(result);
	int wordLength = atoi(buff);
	printf("Recieved word length: %d\n", wordLength);

	char* startWord = (char*)malloc(sizeof(char) * (wordLength + 3));
	char* endWord = (char*)malloc(sizeof(char) * (wordLength + 3));
	char* theHash = (char*)malloc(sizeof(char) * (35));

	result = zmq_recv(receiver, buff, 255, 0);			// receive starting word
	errCheck(result);
	strcpy(startWord, buff);
	printf("Recieved start: \n%s\n", startWord);

	result = zmq_recv(receiver, buff, 255, 0);			// receive ending word
	errCheck(result);
	strcpy(endWord, buff);
	printf("Recieved end: \n%s\n", endWord);
	
	result = zmq_recv(receiver, buff, 255, 0);			// receive hash to match
	errCheck(result);
	strcpy(theHash, buff);
	printf("Recieved hash: \n%s\n", theHash);

	struct workerThreadData threadData[numThreads];
	pthread_t workers[numThreads];
	for (int i = 0; i < numThreads; i++) {
		threadData[i].increment = numThreads;
		threadData[i].wordLength = wordLength;
		threadData[i].startWord = startWord;
		threadData[i].endWord = endWord;
		threadData[i].match = theHash;
		threadData[i].threadID = i;
		printf("pr%d\n", i);
		pthread_create(&workers[i], NULL, workerThread, (void *)&threadData[i]);
	}
	
	// returning results from threads...


	// sending results to master...
	zmq_send(sender, "test", 5, 0);

	zmq_close(receiver);
	zmq_close(sender);
	zmq_ctx_destroy(context);
	return 0;
}