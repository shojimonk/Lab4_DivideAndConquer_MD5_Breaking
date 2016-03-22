
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zmq.h>

/*  unimplemented struct for eventual use in zmq send/recv
struct workerDataSend {
	int numThreads;
	int wordLength;
	char startWord[20];
	char endWord[20];
	char hash[32];
};

struct workerDataSend* newDataSender() {
	struct workerDataSend *newSender = malloc(sizeof(struct workerDataSend));
	if (newSender == NULL) {
		return NULL;
	}

	return newSender;
}

void delWorkerData(struct workerDataSend *wDS) {
	if (wDS != NULL) {
		free(wDS);
	}
}
*/

/* If the last character of the passed string is a newline, removes the newline. */
void chomp(char *toChomp) {

	int length = strlen(toChomp);
	if (toChomp[length - 1] == '\n') {
		toChomp[length - 1] = '\0';
	}
	return;
}

/********************************************************************************	
*	Builds word to be sent to workers based on inputs							*
**	Inputs **																	*
*	increment: which increment of the total is currently being assigned			*
*	numSlaves: total number of increments to be divided amongst					*
*	wordLength: number of characters in the word								*
*	divided[3]: 3 ints that specify how much to increment the 3 most sig chars	*
*	wordResult:	char array used to store the built word							*
*********************************************************************************/
void wordBuilder(int increment, int numSlaves, int wordLength, int divided[3], char *wordResult) {

	uint16_t tempChar;
	wordResult[0] = (increment * divided[0]) + 32;
	tempChar = increment * divided[1];
	if (tempChar >= 94) {
		wordResult[0] += tempChar / 94;
		wordResult[1] = (tempChar % 95) + 32;
	}
	else {
		wordResult[1] = tempChar + 32;
	}
	if (increment == numSlaves - 1) {
		wordResult[2] = divided[2] + 32;
	}
	else {
		wordResult[2] = 32;
	}
	for (int j = 3; j < wordLength; j++) {
		wordResult[j] = 32;
	}
	wordResult[wordLength] = '\0';
	return;
}


/****************************************************************************************
*	Divides the workload up based on the number of workers and threads per worker.		*
*	If not an even division, the last worker is also given the remainder, which			*
*	is never over 1% more than the rest of the workers.									*
*****************************************************************************************/
int* divisionOfLabour(uint8_t numSlaves, uint8_t numThreads, uint8_t passLength) {

	int slaveThreads = numSlaves * numThreads;
	uint16_t numChars = 9025;	// 95 * 95, or the # of combos of 2 characters.
	static int divided[3];

	divided[0] = (numChars / slaveThreads);
	divided[1] = divided[0] % 95;			// amount to increment the 2nd most significant char
	divided[0] /= 95;						// amount to increment the most significant char
	divided[2] = numChars % slaveThreads;	// the remainder, which is extra work for one worker.
	return divided;
}


int main(void)
{
	uint8_t numSlaves = 0;
	char **slaveIPs;
	FILE *fp;
	char buff[255];
	char *verify;

	// open config file
	fp = fopen("./config.txt", "r");
	if (fp == NULL) {
		fprintf(stderr, "error reading from file.\n");
		return EXIT_FAILURE;
	}
	
	verify = fgets(buff, 255, fp);		// read in number of workers
	if (verify == NULL) {
		fprintf(stderr, "error reading from file.\n");
		return EXIT_FAILURE;
	}
	numSlaves = atoi(buff);
	printf("numSlaves = %d\n", numSlaves);
	
	slaveIPs = (char**)malloc(sizeof(char*) * numSlaves);
	for (int i = 0; i < numSlaves; i++) {		// read in worker IPs
		verify = fgets(buff, 30, fp);
		if (verify == NULL) {
			fprintf(stderr, "error reading from file.\n");
			return EXIT_FAILURE;
		}
		slaveIPs[i] = (char*)malloc(sizeof(char) * strlen(buff));
		strcpy(slaveIPs[i], buff);
		chomp(slaveIPs[i]);
//		printf("initial slave IP %d = %s\n", i, slaveIPs[i]);
	}

	verify = fgets(buff, 255, fp);		// read in number of threads per worker
	if (verify == NULL) {
		fprintf(stderr, "error reading from file.\n");
		return EXIT_FAILURE;
	}
	uint8_t slaveThreads = atoi(buff);
	printf("numThreads = %d\n", slaveThreads);

	verify = fgets(buff, 255, fp);			// read in length of word to hash
	if (verify == NULL) {
		fprintf(stderr, "error reading from file.\n");
		return EXIT_FAILURE;
	}
	uint8_t goalLength = atoi(buff);
	printf("goalLength = %d\n", goalLength);

	verify = fgets(buff, 255, fp);			// read in hash to match
	if (verify == NULL) {
		fprintf(stderr, "error reading from file.\n");
		return EXIT_FAILURE;
	}
	char* theHash;
	theHash = (char*)malloc(sizeof(char) * strlen(buff));
	strcpy(theHash, buff);
	chomp(theHash);
	printf("the hash = %s\n", theHash);

	int *divided = divisionOfLabour(numSlaves, slaveThreads, goalLength);	// calculate contents of work packets
	for (int loop = 0; loop < 3; loop++) {	
		printf("\tdivided %d = %d\n", loop, divided[loop]);
	}

	void *context = zmq_ctx_new();
	void *sender = zmq_socket(context, ZMQ_PUSH);		// Socket to send messages on
	void *receiver = zmq_socket(context, ZMQ_PULL);		// Socket to listen for responses
	zmq_bind(receiver, "tcp://*:6535");

	printf("Press Enter when the workers are ready: ");
	getchar();
	
	for (int i = 0; i < numSlaves; i++) {
		char workerConnect[255] = { '\0' };
		strcat(workerConnect, "tcp://");
		strncat(workerConnect, slaveIPs[i], strlen(slaveIPs[i])-1);
		strcat(workerConnect, ":6534");
		printf("\nconnecting to: %s\n", workerConnect);
		
		zmq_connect(sender, workerConnect);
		
		/*	// non-working code attempting to send data as a struct through zmq. needs data-serialization, outside of scope.
		char startWord[20];
		wordBuilder(i*slaveThreads, numSlaves, goalLength, divided, startWord);
		strcpy(toSend->startWord, startWord);

		if (i != numSlaves - 1)
			wordBuilder((i + 1) * slaveThreads, numSlaves, goalLength, divided, startWord);
		else
			for (int j = 0; j < goalLength; j++)
				startWord[j] = 126;

		strcpy(toSend->endWord, startWord);
		printf("sizeof toSend: %d\n", sizeof(toSend));
		printf("sending end Start numT wordL : ..%s..%s..%d..%d..\n", toSend->endWord, toSend->startWord, toSend->numThreads, toSend->wordLength);
		printf("main connected...\n");
		zmq_send(sender, &toSend, sizeof(&toSend), 0);
		printf("main sent...\n");
		*/
		
		char info[255];
		sprintf(info, "%d", slaveThreads);
		printf("threads = %s\t", info);
		zmq_send(sender, info, strlen(info), 0);	// send # of threads

		sprintf(info, "%d", goalLength);
		printf("wordLen = %s\n", info);
		zmq_send(sender, info, strlen(info), 0);	// send length of word

		char startWord[50];
		wordBuilder(i*slaveThreads, numSlaves, goalLength, divided, startWord);
		printf("startWord = ..%s..\n", startWord);
		zmq_send(sender, startWord, goalLength+1, 0);	// send starting word

		if (i != numSlaves - 1)
			wordBuilder((i + 1) * slaveThreads, numSlaves, goalLength, divided, startWord);
		else
			for (int j = 0; j < goalLength; j++) 
				startWord[j] = 126;
		zmq_send(sender, startWord, goalLength + 1, 0);	// send ending word

		zmq_send(sender, theHash, 33, 0);		// send the hash to be matched
		
	}
	printf("waiting for return...\n");
	int result = zmq_recv(receiver, buff, 255, 0);
	char *wordResult;
	wordResult = (char*)malloc(sizeof(char) * result);
	strncpy(wordResult, buff, result);
	wordResult[result] = '\0';
	printf("received end word of: ..%s..", wordResult);

	zmq_close(sender);
	zmq_close(receiver);
	zmq_ctx_destroy(context);
	return EXIT_SUCCESS;
}