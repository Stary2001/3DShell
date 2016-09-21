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

int process_cmd(int fd, struct client_ctx *ctx);