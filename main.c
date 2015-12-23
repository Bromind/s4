#include "coB.h"
#include "color.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct coB b1, b2, b3;
int counter;

void printCallback(char *mess)
{
	printf(ANSI_BLUE"Receiving message : \n");
	printf("Data :\t%s\n"ANSI_RESET, mess);
	counter++;
	if(counter >= 45)
	{
		closeBroadcaster(&b1);
		closeBroadcaster(&b2);
		closeBroadcaster(&b3);
	}
}

int main(void)
{
	char *b1Addr = "127.0.0.1";
	char *b1Port = "9999";
	char *b2Addr = "127.0.0.1";
	char *b2Port = "9998";
	char *b3Addr = "127.0.0.1";
	char *b3Port = "9997";

	char* addrs[3] = {b1Addr, b2Addr, b3Addr};
	char* ports[3] = {b1Port, b2Port, b3Port};
	coBInit(addrs, ports, 3, b1Addr, b1Port, (void*(*)(void*)) &printCallback, &b1, 0);
	coBInit(addrs, ports, 3, b2Addr, b2Port, (void*(*)(void*)) &printCallback, &b2, 1);
	coBInit(addrs, ports, 3, b3Addr, b3Port, (void*(*)(void*)) &printCallback, &b3, 2);
	int i = 0;
	for(; i < 15 ; i++)
	{
		char *mess = malloc(25 * sizeof(char));
		sprintf(mess, "Hello, World ! (%i)", i);
		coBSend(mess, &b1, 0);
		coBSend(mess, &b2, 0);
		coBSend(mess, &b3, 0);
	}
	fprintf(stderr, "ZZZzzz\n");
	/* for(;;){} */
	printf("\n\n");
	printf("Node 1 : \n");
	printTree(&b1);
	printf("Node 2 : \n");
	printTree(&b2);
	printf("Node 3 : \n");
	printTree(&b3);
	return SUCCESS;
}
