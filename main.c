#include "coB.h"
#include "color.h"
#include <stdio.h>
#include <stdlib.h>

void printCallback(char *mess)
{
	printf(ANSI_BLUE"Receiving message : \n");
	printf("Data :\t%s\n"ANSI_RESET, mess);
}

int main(void)
{
	char *b1Addr = "127.0.0.1";
	char *b1Port = "9999";
	char *b2Addr = "127.0.0.1";
	char *b2Port = "9998";
	struct coB b1, b2;
	coBInit(&b2Addr, &b2Port, 1, b1Addr, b1Port, (void*(*)(void*)) &printCallback, &b1, 1);
	coBInit(&b1Addr, &b1Port, 1, b2Addr, b2Port, (void*(*)(void*)) &printCallback, &b2, 2);
	int i = 0;
	for(; i < 20 ; i++)
	{
	char *mess = malloc(25 * sizeof(char));
		sprintf(mess, "Hello, World ! (%i)", i);
		coBSend(mess, &b1);
	}
	fprintf(stderr, "ZZZzzz\n");
	for(;;){}
	return SUCCESS;
}
