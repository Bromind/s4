#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "error.h"
#include "parser.h"

void parsePut(const char line[], struct statement *statement);
void parseGet(const char line[], struct statement *statement);
FILE * openFile(const char *name);
struct file * parse(const char* name)
{
	FILE* fd = openFile(name);
	struct file* file = malloc(sizeof(struct file));
	struct file* first = file;
	/* Each line is at most 26 chars (we take 27 to be sure) */
	char line[27];
	while(fgets(line, 27, fd) != NULL)
	{
		file->next = malloc(sizeof(struct file));
		if(line[0] == 'p') 
		{
			parsePut(line, &file->command);
		}
		if(line[0] == 'g')
		{
			parseGet(line, &file->command);
		}
		file = file->next;
	}
	file->command.type = EOC;
	return first;
}

void parsePut(const char line[], struct statement *statement)
{
	int i;
	int offset = 4;
	statement->type = PUT;
	for(i = 0; i<10 && line[offset+i] != ',' ; i++)
	{
		statement->key[i] = line[offset+i];
	}
	offset += i+1;
	for(i = 0 ; i<10 && line[offset+i] != '\n' ; i++)
	{
		statement->value[i] = line[offset+i];
	}
}



void parseGet(const char line[], struct statement *statement)
{
	int i;
	int offset = 4;
	statement->type = GET;
	for(i = 0; i<10 && line[offset+i] != '\n' ; i++)
	{
		statement->key[i] = line[offset+i];
	}
}

FILE * openFile(const char *name)
{
	FILE *file;
	file = fopen(name, "r");
	if(file == NULL)
	{
		fprintf(stderr, "Can't open file %s : %s", name, strerror(errno));
	}
	return file;
}

void deleteFileTree(struct file* file)
{
	struct file* tmp;
	while(file != NULL)
	{ 
		tmp = file->next;
		free(file);
		file = tmp;
	}
}

void constMap(const struct file *file, void(*f)(const struct statement*))
{
	while(file !=NULL)
	{
		f(&file->command);
		file = file->next;
	}
}

void printCommand(const struct statement* cmd)
{
	switch (cmd->type){ 
		case PUT:
			printf("put %s,%s\n",(char*) &cmd->key, (char*)&cmd->value);
			break;
		case GET:
			printf("get %s\n", (char*)&cmd->key);
			break;
		case EOC:
			printf("End of content\n");
			break;
	}
}

void printFileTree(const struct file *file)
{
	constMap(file, printCommand);
}
