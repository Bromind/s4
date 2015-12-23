#include "coB.h"
#include "parser.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "map.h"

struct coB broadcaster;
char fileName[10];
int localIndex;
volatile int finished = 0;
volatile int start = 0;
struct map map;

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
				printf("%s,%s\n",key,stored);
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
	sprintf(fileName, "%i.output", localIndex);

	initMap(&map);

	coBInit(addrs, ports, nbNode, addrs[localIndex], ports[localIndex], (void*(*)(void*)) &writer, &broadcaster, localIndex);
	

	struct file* file = parse(inputFile);

	constMap(file, broadcast);

	for(;finished != 1;);
	closeBroadcaster(&broadcaster);
	deleteFileTree(file);
	
	return SUCCESS;
}