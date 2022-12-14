#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#include "console_socket.h"
#include "ge.h"
#include "log.h"

static const char socket_path[] = "/tmp/gemu.console";
static int console_socket_fd = -1;

static int console_socket_init(struct ge *ge, void *ctx)
{
    int sd;
    struct sockaddr_un sock;
    (void)ge;
    (void)ctx;
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

    console_socket_fd = sd;
    return 0;
}


static int console_socket_check(struct ge *ge, void *ctx)
{
    char buf[1024];
    struct sockaddr_un dst;
    int ret;
    socklen_t ssz = sizeof(struct sockaddr_un);
    (void)ctx;
    if (console_socket_fd < 0) {
        return -1;
    }
    ge->console.lamps.RO = ge->rRO;
    ge->console.lamps.SO = ge->rSO;
    ge->console.lamps.SA = ge->rSA;
    ge->console.lamps.FA = ge->rFA & 0x0F;

    ret = recvfrom(console_socket_fd, buf, 1024, 0,
                (struct sockaddr *)&dst, &ssz);
    if (ret > 0) {
        ge_log(LOG_DEBUG, "doing check\n");
        sendto(console_socket_fd, (unsigned char *)(&ge->console), sizeof(struct ge_console), 0,
               (struct sockaddr *)&dst, ssz);
    }

    return 0;
}

static struct ge_peri console_socket = {
    .init = console_socket_init,
    .on_pulse = console_socket_check,
};

int console_socket_register(struct ge *ge)
{
    return ge_register_peri(ge, &console_socket);
}
