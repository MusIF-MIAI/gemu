#ifndef CONSOLE_SOCKET_H
#define CONSOLE_SOCKET_H

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

struct ge;

int console_socket_init(void);
int console_socket_check(struct ge *ge);


#endif
