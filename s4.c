#include "coB.h"
#include "parser.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "map.h"
#include <errno.h>
#include <signal.h>

struct coB broadcaster;
FILE* outputFile;
int localIndex;
volatile int finished = 0;
volatile int start = 0;
struct map map;

void starter(int signo);

void writer(char* mess)
{
	int index;
	char key[11];
	char value[11];
	char* stored;
	switch (mess[0])
	{
		case 'P' : 
			//printf("Received PUT:\t%s\n",mess);
			strcpy(key, &mess[2]);
			strcpy(value, &mess[13]);
			put(&map, key, value);
			break;
		case 'G' : 
			index = (int) mess[2];
			strcpy(key, &mess[3+sizeof(int)]);
			if(index == localIndex)
			{
				stored = get(&map, key);
				if(stored == NULL)
				{
					stored = "undefined";
				}
				fprintf(outputFile,"%s,%s\n",key,stored);
			}	
			break;
		case 'E' : 
//			printf("End of file\n");
			index = (int) mess[2];
			if(index == localIndex)
			finished = 1;
			break;
	}
}

/*
	message format : 
	P\000KEY.......\000VALUE.....\000\000
	G\000\IDX\IDX\IDX\IDX\000KEY.......\000\000\000\000\000\000\000\000
	E\000\IDX\IDX\IDX\IDX\000...
*/

void broadcast(const struct statement* cmd)
{
	char msg[25];
	char* indexPos;
	switch (cmd->type){ 
		case PUT:
			strcpy(&msg[2], cmd->key);
			strcpy(&msg[13], cmd->value);
			msg[0] = 'P';
			coBSend(msg, &broadcaster, 0);
			break;
		case GET:
			msg[0] = 'G';
			indexPos=&msg[2];
			*(int*)indexPos = localIndex;
			sprintf(&msg[3+sizeof(int)], "%s", cmd->key);
			coBSend(msg, &broadcaster, 0);
			break;
		case EOC:
			msg[0] = 'E';
			indexPos=&msg[2];
			*(int*)indexPos = localIndex;
			coBSend(msg, &broadcaster, 0);
			break;
	}
}

int main(int argc, char** argv)
{
	if(argc%2 !=1 || argc < 5)
	{
		fprintf(stderr, "Usage : ./s4 ip1 port1 [ip2 port2 ...] index inputFile\n");
		fprintf(stderr, "Minimun 1 node, indexes increasing 1 by 1, starting at 0.");
		return EARGS;
	}

	int nbNode = (argc - 3)/2;
	char** addrs = malloc(nbNode*sizeof(char*));
	char** ports = malloc(nbNode*sizeof(char*));

	int i;
	for(i = 0 ; i < nbNode ; i++)
	{
		addrs[i] = argv[1+2*i];
		ports[i] = argv[2+2*i];
	}
	localIndex = atoi(argv[1+2*nbNode]);
	char* inputFile = argv[2+2*nbNode];

	char fileName[10];
	sprintf(fileName, "%i.output", localIndex);
	outputFile = fopen(fileName, "w");
	if(outputFile == NULL)
	{
		fprintf(stderr, "Can't open file %s : %s\n", fileName, strerror(errno));
		fprintf(stderr, "Will output to stdout instead\n");
		outputFile = stdout;
	}

	initMap(&map);
	coBInit(addrs, ports, nbNode, addrs[localIndex], ports[localIndex], (void*(*)(void*)) &writer, &broadcaster, localIndex);
	size_t fileSize = 0;
	struct file* file = parse(inputFile, &fileSize);

#ifndef DONT_WAIT_SIGNAL /* Wait signal option */
	/* Signal catching. Use sa_handler. */
	struct sigaction action, oldAction; 
	memset(&action, '\0', sizeof(struct sigaction));
	action.sa_handler = &starter;
	
	if(sigaction(SIGINT, &action, &oldAction) != 0)
	{
		fprintf(stderr, "sigaction() failed : %s\n", strerror(errno));
	}


	for(;start != 1;)
		sleep(1); /* Wait for start flag */ 

	if(sigaction(SIGINT, &oldAction, NULL) != 0)
	{
		fprintf(stderr, "sigaction() failed : %s\n", strerror(errno));
	}
#endif

	constMap(file, broadcast);

	for(;finished != 1;)
		sched_yield();
	closeBroadcaster(&broadcaster);
	deleteFileTree(file);
	
	return SUCCESS;
}

void starter(int signo)
{
	switch(signo) 
	{
		case SIGINT:
			fprintf(stderr, "\nEn route vers l'infini et l'au-delà...\n");
			start = 1;
			break;
	}
}
