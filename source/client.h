#pragma once

enum client_type
{
	TYPE_GDB,
	TYPE_SHELL
};

typedef struct client_ctx
{
	enum client_type type;
} client_ctx;