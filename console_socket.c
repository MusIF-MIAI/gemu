#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "console_socket.h"
#include "ge.h"
#include "console.h"
#include "log.h"

static const char socket_path[] = "/tmp/gemu.console";
static int console_socket_fd = -1;

/* Momentary keys are edge-triggered; this remembers the last frame's word. */
static uint16_t console_prev_buttons;

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
    console_prev_buttons = 0;   /* a fresh session starts with no key down */
    return 0;
}

static int console_socket_deinit(struct ge *ge, void *ctx)
{
    (void)ge;
    (void)ctx;
    if (console_socket_fd >= 0) {
        close(console_socket_fd);
        console_socket_fd = -1;
    }
    unlink(socket_path);
    return 0;
}


/*
 * The wire format of an inbound console frame is `struct ge_console` as far as
 * the client fills it in: the lamp block is write-only from the machine's side
 * and arrives zeroed, then the switches, the momentary buttons, and the rotary
 * position. Only the last three are read here.
 *
 *   bytes  0..11  lamps            (ignored on input)
 *   bytes 12..13  switch flags     (PAPA, PATE, RICI, ACOV, ACON, INAR, STOC,
 *                                   INCE, SITE, lamps_on)
 *   bytes 14..15  AM forcing register
 *   bytes 16..17  momentary buttons (see console.h ge_console_buttons)
 *   byte     18   rotary register selector
 *
 * The client packs the rotary as a single byte, not as the C enum's four, so
 * the frame is 19 bytes and the rotary is read as a byte.
 */
#define CONSOLE_FRAME_BYTES 19
#define CONSOLE_OFF_SWITCHES 12
#define CONSOLE_OFF_BUTTONS  16
#define CONSOLE_OFF_ROTARY   18

/* Bit positions within the buttons word, in ge_console_buttons field order. */
enum {
    BTN_AC_ON = 0, BTN_DC_ALERT, BTN_POWER_ON, BTN_MAINTENANCE_ON,
    BTN_SWITCH_1, BTN_SWITCH_2, BTN_STEP_BY_STEP, BTN_LOAD_1_2,
    BTN_EMERGEN_OFF, BTN_STANDBY, BTN_PAD, BTN_MEM_CHECK,
    BTN_CLEAR, BTN_LOAD, BTN_HALT_START, BTN_OPER_CALL,
};

/*
 * Momentary keys are edge-triggered: a key does something when it goes down,
 * not for as long as a client keeps reporting it held. (A client that clears
 * its button word after each frame gets the same behaviour either way.)
 */
static void console_press_buttons(struct ge *ge, uint16_t buttons)
{
    uint16_t edge = (uint16_t)(buttons & ~console_prev_buttons);

    console_prev_buttons = buttons;

    /* LAMPS CHECK shares the MAINT ON button and is the one control here that
     * is not edge-triggered: it is a bulb test that lasts exactly as long as
     * the key is held, so it follows the level. */
    ge->lamps_test = (buttons >> BTN_MAINTENANCE_ON) & 1u;

    if (!edge)
        return;

    /* CLEAR: stop the subsystem and preset it. Note this does NOT clear core --
     * see ge_clear. */
    if (edge & (1u << BTN_CLEAR)) {
        ge_log(LOG_CONSOLE, "console: CLEAR\n");
        ge_clear(ge);
    }

    /* LOAD1/LOAD2: pick which of the two install-time load units the bootstrap
     * will read. One key on the panel, alternating between the two lamps. */
    if (edge & (1u << BTN_LOAD_1_2)) {
        if (ge->ALOI)
            ge_load_2(ge);
        else
            ge_load_1(ge);
        ge_log(LOG_CONSOLE, "console: LOAD%d selected\n", ge->ALOI ? 1 : 2);
    }

    /* LOAD: arms AINI and NOTHING else. No card moves until START. */
    if (edge & (1u << BTN_LOAD)) {
        ge_log(LOG_CONSOLE, "console: LOAD (AINI armed; START will read one card)\n");
        ge_load(ge);
    }

    /* START (HALT): the same key starts a stopped machine and stops a running
     * one. The white HALT lamp says which state you are in. */
    if (edge & (1u << BTN_HALT_START)) {
        if (ge->ALTO) {
            ge_log(LOG_CONSOLE, "console: START\n");
            /* Off NORM this performs the one maintenance cycle the key is worth
             * and leaves the machine stopped; the caller's run loop must not
             * turn it further (console.c). */
            ge_console_start(ge);
        } else {
            ge_log(LOG_CONSOLE, "console: HALT\n");
            ge->ALTO = 1;
        }
    }

    /* The two program-readable switches (JS1 / JS2). */
    if (edge & (1u << BTN_SWITCH_1))
        ge->JS1 = !ge->JS1;
    if (edge & (1u << BTN_SWITCH_2))
        ge->JS2 = !ge->JS2;

    /* STEP-BY-STEP is the operator panel's own switch (ASIN), a separate circuit
     * from the maintenance PAPA and with its own lamp -- so it is a key here,
     * and it owns its own state. It stops the machine at each instruction and
     * can be inhibited by the program (INS/ENS), which PAPA cannot. */
    if (edge & (1u << BTN_STEP_BY_STEP))
        ge->ASIN = !ge->ASIN;

    if (edge & (1u << BTN_POWER_ON))
        ge->powered = 1;
    if (edge & (1u << BTN_EMERGEN_OFF))
        ge->powered = 0;
}

static int console_socket_check(struct ge *ge, void *ctx)
{
    unsigned char buf[1024];
    struct sockaddr_un dst;
    ssize_t ret;
    socklen_t socket_size = sizeof(struct sockaddr_un);
    struct ge_console console;

    (void)ctx;

    if (console_socket_fd < 0) {
        return -1;
    }

    ret = recvfrom(console_socket_fd, buf, sizeof(buf), 0,
                   (struct sockaddr *)&dst, &socket_size);
    if (ret > 0) {
        if (ret >= CONSOLE_FRAME_BYTES) {
            struct ge_console_switches sw;
            uint16_t buttons;
            unsigned char rotary;

            memcpy(&sw, buf + CONSOLE_OFF_SWITCHES, sizeof(sw));
            memcpy(&buttons, buf + CONSOLE_OFF_BUTTONS, sizeof(buttons));
            rotary = buf[CONSOLE_OFF_ROTARY];

            ge_set_console_switches(ge, &sw);
            if (rotary <= RS_FO)
                ge_set_console_rotary(ge, (enum ge_console_rotary)rotary);
            console_press_buttons(ge, buttons);
        } else {
            ge_log(LOG_CONSOLE,
                   "console: short frame (%zd bytes, want %d); ignoring input\n",
                   ret, CONSOLE_FRAME_BYTES);
        }

        /* Answer with the lamps as they stand AFTER the keys were acted on, so
         * a CLEAR or a START is visible in the very frame that requested it. */
        ge_fill_console_data(ge, &console);
        sendto(console_socket_fd, (unsigned char *)(&console),
               sizeof(struct ge_console), 0, (struct sockaddr *)&dst,
               socket_size);
    }

    return 0;
}

static struct ge_peri console_socket = {
    .init = console_socket_init,
    .on_pulse = console_socket_check,
    .deinit = console_socket_deinit,
};

int console_socket_register(struct ge *ge)
{
    return ge_register_peri(ge, &console_socket);
}
