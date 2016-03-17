
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

pthread_cond_t checkCondition;
pthread_mutex_t matchedMutex;
int8_t finished = -1;
char *match;

struct workerThreadData {
	int increment;
	int wordLength;
	char* startWord;
	char* endWord;
	char* match;
	int threadID;
};

void wordIncrement(char* theWord, int increment, int index) {
	if ((int)theWord[index] <= (126 - increment)) {
		theWord[index] += increment;
		return;
	}
	else {
		wordIncrement(theWord, 1, index - 1);
		theWord[index] = (theWord[index] + increment) % 126;
		theWord[index] += 32;
	}
}

// The method run in each thread to loop through the words and check hash results.
void* workerThread(void* data) {
	// initialize data
	//pthread_mutex_lock(&matchedMutex);
	struct workerThreadData *inputs;
	inputs = (struct workerThreadData*) data;

	printf("in%d\n", inputs->threadID);
	fflush(stdout);

	char* currentWord = (char*)malloc(sizeof(char) * strlen(inputs->startWord + 1));
	strcpy(currentWord, inputs->startWord);
	char* currentHash;
	
	int counter = 0;

	wordIncrement(currentWord, inputs->threadID, strlen(currentWord) - 1);	// pre-increment by thread # to prevent all threads running same words.
	while (true) {
		if (finished < 0) {
			//printf("..%d..\n", inputs->threadID);
			//pthread_mutex_unlock(&matchedMutex);
			MD5_CTX context;
			unsigned char digest[16];
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
				printf("worker '%d' found match.\tword is: \"%s\"\n", inputs->threadID, currentWord);
				printf("wait lock %d\n", inputs->threadID);
				pthread_mutex_lock(&matchedMutex);
				printf("got lock %d\n", inputs->threadID);
				finished = inputs->threadID;
				pthread_cond_signal(&checkCondition);
				strcpy(match, currentWord);
				free(currentWord);
				free(currentHash);
				pthread_mutex_unlock(&matchedMutex);
				printf("release lock %d\n", inputs->threadID);
				pthread_exit(NULL);
				// return match somehow
			}
			int comp = strcmp(currentWord, inputs->endWord);
			if (comp >= 0) {		// return failure to find match, and done.
				printf("failed to match? .%d.\n", inputs->threadID);
				printf("current word: %s\tend Word: %s\n", currentWord, inputs->endWord);
				fflush(stdout);
				printf("wait lock %d\n", inputs->threadID);
				pthread_mutex_lock(&matchedMutex);
				printf("got lock %d\n", inputs->threadID);
				free(currentWord);
				free(currentHash);
				finished = inputs->threadID;
				pthread_cond_signal(&checkCondition);
				pthread_mutex_unlock(&matchedMutex);
				printf("release lock %d\n", inputs->threadID);
				pthread_exit(NULL);
			}
			else {					// increment to next word.
				comp = 0;
				wordIncrement(currentWord, inputs->increment, strlen(currentWord) - 1);
				counter++;
			}
			//pthread_mutex_lock(&matchedMutex);
			currentHash[0] = '\0';
			continue;
		}
		else printf("..%d.fini is..%d.\n", inputs->threadID, finished);
		break;
	}
	printf("wait lock %d\n", inputs->threadID);
	pthread_mutex_lock(&matchedMutex);
	printf("got lock %d\n", inputs->threadID);
	printf("exit%d\t%d\n", inputs->threadID, finished);
	finished = inputs->threadID;
	pthread_cond_signal(&checkCondition);
	pthread_mutex_unlock(&matchedMutex);
	printf("release lock %d\n", inputs->threadID);
	pthread_exit(NULL);
}

// 
int errCheck(int result) {
	if (result == -1) {
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
	

	pthread_cond_init(&checkCondition, NULL);
	pthread_mutex_init(&matchedMutex, NULL);

	struct workerThreadData threadData[numThreads];
	pthread_t workers[numThreads];
	for (int i = 0; i < numThreads; i++) {
		threadData[i].increment = numThreads;
		threadData[i].wordLength = wordLength;
		threadData[i].startWord = startWord;
		threadData[i].endWord = endWord;
		threadData[i].match = theHash;
		threadData[i].threadID = i;
		printf("pr%d\n", threadData[i].threadID);
		pthread_create(&workers[i], NULL, workerThread, (void *)&threadData[i]);
	}

	match = (char*)malloc(sizeof(char) * wordLength);
	// returning results from threads...
	for (int threads = numThreads-1; threads > 0; threads--) {
		pthread_cond_wait(&checkCondition, &matchedMutex);
		printf("wait lock main\n");
		pthread_mutex_lock(&matchedMutex);
		pthread_join(workers[finished], NULL);
		if (match != NULL) {
			finished = 10;
		}
		else {
			printf("match!=null\n");
			finished = -1;
		}
		printf("waiting on: %d\n", threads);
		pthread_mutex_unlock(&matchedMutex);
	}
	
	
	// sending results to master...
	zmq_send(sender, "test", 5, 0);

	zmq_close(receiver);
	zmq_close(sender);
	zmq_ctx_destroy(context);
	return 0;
}