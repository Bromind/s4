#include "coB.h"
#include "color.h"
#include <stdio.h>

void printCallback(struct p2pMessage *buf)
{
	struct coMessage *coMesg = (struct coMessage*) buf->message;
	void *args = buf->upperLayerArgs;
	printf(ANSI_BLUE"Receiving message : \n");
	printf("Sender :\t%i\tSSN :\t%i\nPredecessor :\t%i\tPSSN :\t%i\n", coMesg->sender, coMesg->ssn, coMesg->pred_sender, coMesg->pred_ssn);
	printf("Args :\t%p\n", args);
	printf("Data :\t%s\n"ANSI_RESET, coMesg->buffer);
}

int main(void)
{
	char *addrReceiver = "127.0.0.1";
	char *portReceiver = "9999";
	struct p2pChannel chan;
	int res = init_p2p("9999", "127.0.0.1", &chan, 
			(void* (*)(void*))&printCallback, NULL);
	struct coB broadcast;
	coBInit(&addrReceiver, &portReceiver, 1, addrReceiver, "9998", NULL, &broadcast, 1);
	coBSend("Hello, World !", &broadcast);
	fprintf(stderr, "ZZZzzzz\n");
	for(;;){}
	return res;	
}
