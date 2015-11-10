#include "p2p.h"
#include <stdio.h>

void printCallback(struct p2pMessage *buf)
{
	printf("Receiving callback : \n");
	printf("%s\n", &(buf->message));
}

int main(void)
{
	struct p2pChannel chan;
	int res = init_p2p("9999", "127.0.0.1", &chan, 
			(void* (*)(void*))&printCallback);
	if(res == SUCCESS) {
		res = init_p2p_sender("9999", "127.0.0.01", &chan);
	}
	if(res == SUCCESS) {
		char* message = "Hello, world!";
		res = p2pSend(&chan, 0, message, 14);
	}
	fprintf(stderr, "ZZZzzzz\n");
	for(;;){}
	return res;	
}
