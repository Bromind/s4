#include "error.h"
#include "map.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_KEY 11
#define MAX_VALUE 11
#define MAP_INIT_SIZE 1

int compare(const char* s1, const char* s2);
int indexOf(struct map* map, const char* key);

struct mapNode
{
	char key[MAX_KEY];
	char value[MAX_VALUE];
};


int initMap(struct map * map)
{
	map->set = malloc(MAP_INIT_SIZE*sizeof(struct mapNode));
	if(map->set == NULL)
	{
		fprintf(stderr, "Can't allocate");
		return EALLOC;
	}
	map->nb_elem = 0;
	map->set_size = MAP_INIT_SIZE;
	return SUCCESS;
}

char* get(struct map* map, const char* key)
{
	int i = indexOf(map, key);
	if(i == -1)
	{
		return NULL;
	}
	return map->set[i]->key;
}

void put(struct map* map, const char* key, const char* value)
{
	int i = indexOf(map, key);
	if(i == -1)
	{
		if(map->nb_elem == map->set_size)
		{
			size_t newSize = 2*map->set_size;
			struct mapNode** newSet = realloc(map->set, newSize*sizeof(struct mapNode*));
			if(newSet == NULL)
			{
				fprintf(stderr, "Realloc failure");
				return;
			}
			map->set_size = newSize;
			map->set = newSet;

		} 
		struct mapNode* node = malloc(sizeof(struct mapNode));
		strcpy(node->key, key);
		strcpy(node->value, value);
		map->set[map->nb_elem] = node;
		map->nb_elem++;
	} else {
		strcpy(map->set[i]->value, value);
	}
}

int indexOf(struct map* map, const char* key)
{
	int i = 0;
	for(;i < map->nb_elem ; i++)
	{
		if(compare(map->set[i]->key, key) == 0)
		{
			return i;
		}
	}
	return -1;
}


int compare(const char* s1, const char* s2)
{
int i;
	for(i = 0 ; i < MAX_KEY ; i++)
	{
		if(s1[i] == '\0' && s2[i] == '\0')
		{
			return 0; 
		}
		if(s1[i] != s2[i]) 
		{
			return 1;
		}
	}
	return 0;
}
