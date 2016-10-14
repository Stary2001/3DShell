#pragma once
#include "client.h"

enum token_type
{
	Null,
	Int,
	Str,
	Cmd
};

enum cmd_type
{
	Gdb
};

typedef struct token
{
	enum token_type t;
	union
	{
		int i;
		unsigned int ui;
		char *str;
		enum cmd_type cmd;
	}
} token;

typedef void (*output_func)(int fd, void *ctx, const char *s, int len);
typedef int (*shell_cmd_func)(int fd, struct shell_ctx *ctx, int argc, char **argv);

typedef struct shell_cmd
{
	const char *name;
	shell_cmd_func func;
} shell_cmd;

typedef struct shell_ctx
{
	output_func out;
} shell_ctx;

int process_cmd(int fd, struct client_ctx *ctx);
int process_line(int fd, struct shell_ctx *ctx, const char *line);

int shell_printf(int fd, struct shell_ctx *ctx, const char *line, ...);
int sock_shell_out(int fd, void *ctx, const char *s, int len);