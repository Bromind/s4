
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


struct file * parse(const char* name, size_t *fileSize);
void deleteFileTree(struct file* file);
void constMap(const struct file *file, void(*f)(const struct statement*));
