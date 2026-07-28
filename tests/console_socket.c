/*
 * tests/console_socket.c - the operator panel actually reaches the machine.
 *
 * The ncurses client (console/curses/console.py) has always packed CLEAR, LOAD
 * and START into its frame; until this was wired, console_socket_check received
 * the datagram and threw it away, so pressing a key on the panel did nothing at
 * all. These tests drive the real socket and assert the machine responds.
 *
 * Frame layout (console_socket.c): 12 bytes of lamps, then the switch flags,
 * the AM register, the momentary buttons, and a one-byte rotary position.
 */

#include "utest.h"
#include "../ge.h"
#include "../console.h"
#include "../console_socket.h"
#include "../log.h"

#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define BTN_SWITCH_1     4
#define BTN_SWITCH_2     5
#define BTN_LOAD_1_2     7
#define BTN_CLEAR        12
#define BTN_LOAD         13
#define BTN_HALT_START   14

#define FRAME_BYTES   19
#define OFF_SWITCHES  12
#define OFF_BUTTONS   16
#define OFF_ROTARY    18

/* A stand-in for the panel client: bind our own address, send one frame, read
 * the lamps back. Returns 0 on success. */
struct panel {
    int fd;
    char path[128];
};

static int panel_open(struct panel *p, const char *tag)
{
    struct sockaddr_un a;

    snprintf(p->path, sizeof(p->path), "/tmp/gemu.console.test.%s", tag);
    unlink(p->path);

    p->fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (p->fd < 0)
        return -1;

    memset(&a, 0, sizeof(a));
    a.sun_family = AF_UNIX;
    snprintf(a.sun_path, sizeof(a.sun_path), "%s", p->path);
    if (bind(p->fd, (struct sockaddr *)&a, sizeof(a)) != 0) {
        close(p->fd);
        p->fd = -1;
        return -1;
    }

    memset(&a, 0, sizeof(a));
    a.sun_family = AF_UNIX;
    snprintf(a.sun_path, sizeof(a.sun_path), "/tmp/gemu.console");
    if (connect(p->fd, (struct sockaddr *)&a, sizeof(a)) != 0) {
        close(p->fd);
        p->fd = -1;
        return -1;
    }
    return 0;
}

static void panel_close(struct panel *p)
{
    if (p->fd >= 0)
        close(p->fd);
    unlink(p->path);
}

/* Send one frame verbatim, then let the emulator service it. */
static void panel_frame(struct panel *p, struct ge *g, uint16_t buttons,
                        uint16_t switches, unsigned char rotary)
{
    unsigned char frame[FRAME_BYTES];
    char reply[256];

    memset(frame, 0, sizeof(frame));
    memcpy(frame + OFF_SWITCHES, &switches, sizeof(switches));
    memcpy(frame + OFF_BUTTONS, &buttons, sizeof(buttons));
    frame[OFF_ROTARY] = rotary;

    (void)send(p->fd, frame, sizeof(frame), 0);
    ge_run_cycle(g);                 /* on_pulse services the socket */
    (void)recv(p->fd, reply, sizeof(reply), MSG_DONTWAIT);
}

/* Press a key and release it, the way a finger does. */
static void panel_press(struct panel *p, struct ge *g, uint16_t buttons)
{
    panel_frame(p, g, buttons, 0, RS_NORM);
    panel_frame(p, g, 0, 0, RS_NORM);
}

UTEST(console_socket, buttons_reach_the_machine)
{
    struct ge g;
    struct panel p;

    ge_init(&g);
    ge_log_set_active_types(0);
    ASSERT_EQ(console_socket_register(&g), 0);
    ge_clear(&g);

    if (panel_open(&p, "buttons") != 0) {
        printf("  [SKIP] cannot open a client socket\n");
        ge_deinit(&g);
        return;
    }

    /* After CLEAR the machine is stopped and nothing is armed. */
    ASSERT_EQ((int)g.ALTO, 1);
    ASSERT_EQ((int)g.AINI, 0);

    /* LOAD1 is the default; one press of the selector key moves to LOAD2. */
    ASSERT_EQ((int)g.ALOI, 0);
    panel_press(&p, &g, 1u << BTN_LOAD_1_2);
    ASSERT_EQ((int)g.ALOI, 1);            /* LOAD1 = connector 2 */
    panel_press(&p, &g, 1u << BTN_LOAD_1_2);
    ASSERT_EQ((int)g.ALOI, 0);            /* LOAD2 */
    panel_press(&p, &g, 1u << BTN_LOAD_1_2);
    ASSERT_EQ((int)g.ALOI, 1);

    /* LOAD arms the bootstrap and does nothing else: the machine stays
     * stopped, and no card has moved. */
    panel_press(&p, &g, 1u << BTN_LOAD);
    ASSERT_EQ((int)g.AINI, 1);
    ASSERT_EQ((int)g.ALTO, 1);

    /* START releases it. */
    panel_press(&p, &g, 1u << BTN_HALT_START);
    ASSERT_EQ((int)g.ALTO, 0);

    /* The same key stops it again (the panel legend is "START (HALT)"). */
    panel_press(&p, &g, 1u << BTN_HALT_START);
    ASSERT_EQ((int)g.ALTO, 1);

    /* CLEAR disarms AINI. */
    panel_press(&p, &g, 1u << BTN_CLEAR);
    ASSERT_EQ((int)g.AINI, 0);

    /* The two program-readable switches. (STEP-BY-STEP is not a momentary key:
     * it is the PAPA switch, and rides the switch word -- see the switches test
     * below and console_socket.c.) */
    ASSERT_EQ((int)g.JS1, 0);
    panel_press(&p, &g, 1u << BTN_SWITCH_1);
    ASSERT_EQ((int)g.JS1, 1);
    panel_press(&p, &g, 1u << BTN_SWITCH_2);
    ASSERT_EQ((int)g.JS2, 1);

    panel_close(&p);
    ge_deinit(&g);
}

UTEST(console_socket, a_held_key_presses_once)
{
    struct ge g;
    struct panel p;

    ge_init(&g);
    ge_log_set_active_types(0);
    ASSERT_EQ(console_socket_register(&g), 0);
    ge_clear(&g);

    if (panel_open(&p, "held") != 0) {
        printf("  [SKIP] cannot open a client socket\n");
        ge_deinit(&g);
        return;
    }

    /* A client that keeps reporting the selector key held must not make the
     * load unit flap back and forth once per frame. */
    panel_frame(&p, &g, 1u << BTN_LOAD_1_2, 0, RS_NORM);
    ASSERT_EQ((int)g.ALOI, 1);
    panel_frame(&p, &g, 1u << BTN_LOAD_1_2, 0, RS_NORM);
    ASSERT_EQ((int)g.ALOI, 1);
    panel_frame(&p, &g, 1u << BTN_LOAD_1_2, 0, RS_NORM);
    ASSERT_EQ((int)g.ALOI, 1);

    /* Released, then pressed again: that is a second press. */
    panel_frame(&p, &g, 0, 0, RS_NORM);
    panel_frame(&p, &g, 1u << BTN_LOAD_1_2, 0, RS_NORM);
    ASSERT_EQ((int)g.ALOI, 0);

    panel_close(&p);
    ge_deinit(&g);
}

UTEST(console_socket, switches_and_rotary_follow_the_frame)
{
    struct ge g;
    struct panel p;
    struct ge_console_switches sw;
    uint16_t packed;

    ge_init(&g);
    ge_log_set_active_types(0);
    ASSERT_EQ(console_socket_register(&g), 0);
    ge_clear(&g);

    if (panel_open(&p, "switches") != 0) {
        printf("  [SKIP] cannot open a client socket\n");
        ge_deinit(&g);
        return;
    }

    memset(&sw, 0, sizeof(sw));
    sw.PAPA = 1;
    sw.INAR = 1;
    memcpy(&packed, &sw, sizeof(packed));

    panel_frame(&p, &g, 0, packed, RS_V1_SCR);
    ASSERT_EQ((int)g.console_switches.PAPA, 1);
    ASSERT_EQ((int)g.console_switches.INAR, 1);
    ASSERT_EQ((int)g.console_switches.STOC, 0);
    ASSERT_EQ((int)g.register_selector, (int)RS_V1_SCR);

    panel_close(&p);
    ge_deinit(&g);
}
