
/* PUT, GET, EncOfContent */
enum type {EOC,PUT,GET};


struct statement {
	enum type type;
	char key[11];
	char value[11];
};

struct file {
	struct statement command;
	struct file* next;
};

void parsePut(const char line[], struct statement *statement);
void parseGet(const char line[], struct statement *statement);
FILE * openFile(const char *name);

