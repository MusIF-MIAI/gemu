#include "console_socket.h"
#include "ge.h"
#include <fcntl.h>

static const char socket_path[] = "/tmp/gemu.console";

int console_socket_init(void)
{
    int sd;
    struct sockaddr_un sock;
    unlink(socket_path);
    memset(&sock, 0, sizeof(sock));
    sock.sun_family = AF_UNIX;
    strcpy(sock.sun_path, socket_path);
    sd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sd < 0)
        return sd;
    fcntl(sd, F_SETFL, O_NONBLOCK);
    if (bind(sd, (struct sockaddr *)&sock, sizeof(sock)) != 0) {
        close(sd);
        return -1;
    }
    return sd;
}


int console_socket_check(struct ge *ge)
{
    char buf[1024];
    if (ge->ge_console_socket < 0) {
        return -1;
    }
    if (recv(ge->ge_console_socket, buf, 1024, 0) > 0) {
        send(ge->ge_console_socket, (unsigned char *)(&ge->console), sizeof(struct ge_console), 0);
    }

}

