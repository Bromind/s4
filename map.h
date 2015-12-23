#include <sys/types.h>

struct map
{
	struct mapNode** set;
	size_t nb_elem;
	size_t set_size;
};
int initMap(struct map * map);
char* get(struct map* map, const char* key);
void put(struct map* map, const char* key, const char* value);
