#define SCRATCH_LENGTH 256

typedef struct client_ctx
{
	char scratch[SCRATCH_LENGTH];
	int scratch_off;
	int scratch_len;
} client_ctx;

enum TokenType
{
	Null,
	Int,
	Str,
	Cmd
};

enum CmdType
{
	Gdb
};

typedef struct token
{
	enum TokenType t;
	union
	{
		int i;
		unsigned int ui;
		char *str;
		enum CmdType cmd;
	}
} token;

int process_cmd(int fd, struct client_ctx *ctx);