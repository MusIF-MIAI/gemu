#!/usr/bin/python3
import curses
import socket
import struct
from curses import wrapper
import time
import os

led_off='⚆'
led_on='⚈'
led_spacing = 3

sw_up = '┸'
sw_down = '┰'

MS_VAL = 0
AM_VAL = 0

LP_RO = 0
LP_SO = 0
LP_SA = 0
LP_ADD_REG = 0
LP_OP_REG = 0
LP_ALERTS = 0
SCREEN = 'top'
CPU_sock = None
CPU_sock_connected = False


scr = curses.initscr()
curses.noecho()
curses.cbreak()
curses.nocbreak()
curses.mousemask(1)
scr.keypad(True)
curses.curs_set(0)
curses.start_color()
led_colorpair = curses.init_pair(1, curses.COLOR_YELLOW, curses.COLOR_BLACK)
chassis_colorpair = curses.init_pair(2, curses.COLOR_WHITE, curses.COLOR_BLUE)
button_red_colorpair = curses.init_pair(3, curses.COLOR_WHITE, curses.COLOR_RED)
button_white_colorpair = curses.init_pair(4, curses.COLOR_BLACK, curses.COLOR_WHITE)
MAX_Y,MAX_X = scr.getmaxyx()

def statusbar(st):
    global SCREEN
    if SCREEN == 'front':
        cp = 1
    else:
        cp = 2

    for x in range(0, MAX_X):
        scr.addch(MAX_Y - 2, x,  ' ', curses.color_pair(cp))

    scr.addstr(MAX_Y - 2, 0, st, curses.color_pair(cp))
    x = len(st) + 2
    if (SCREEN == 'front'):
        scr.addstr(MAX_Y - 2, x, "Operator panel. Press [TAB] to switch.", curses.color_pair(cp))
    else:
        scr.addstr(MAX_Y - 2, x, "Diagnostic panel. Press [TAB] to switch.", curses.color_pair(cp))

def CPU_read_status(buf):
    global LP_RO, LP_SO, LP_SA
    global LP_ADD_REG, LP_OP_REG, LP_ALERTS
    statusbar('msg')
    if (len(buf) != 19):
        statusbar('protocol error')
        return
    LP_RO, LP_SO, LP_SA = struct.unpack("HHH", buf[0:6])
    LP_ADD_REG, LP_OP_REG, LP_ALERTS = struct.unpack("HHH", buf[6:12])
    statusbar('CPU Synchronized')
     


def switch_screen():
    global SCREEN
    scr.clear()
    if SCREEN == 'top':
        SCREEN = 'front'
    else:
        SCREEN = 'top'


def draw_led_labels():
    y = 8
    # fuse
    scr.addch(y, 20, '⬢')
    scr.addstr(y + 1, 19, 'Fuse')
    y = 16
    # first line
    scr.addstr(y - 1, 9, "┏━━┯━━┯━━┯━━┯━━┯━━┯━━┯━━┓ ┏━━┯━━┯━━┯━━┯━━┯━━┯━━┯━━┓")
    scr.addstr(y + 1, 32, '08   07 06 05 04 03 02 01 00')
    scr.addstr(y + 2, 32, '\__________________________/')
    scr.addstr(y + 3, 45, 'RO')
    scr.addstr(y + 1, 20, 'UR')

    y += 6

    # second line
    scr.addstr(y - 1, 9, "┏━━┯━━┯━━┯━━┯━━┯━━┯━━┯━━┓ ┏━━┯━━┯━━┯━━┯━━┯━━┯━━┯━━┓")
    scr.addstr(y + 1, 37, '07 06 05 04 03 02 01 00')
    scr.addstr(y + 2, 37, '\_____________________/')
    scr.addstr(y + 3, 48, 'SO')
    scr.addstr(y + 1, 11, '03 02 01 00')
    scr.addstr(y + 2, 11, '\_________/')
    scr.addstr(y + 3, 15, 'FA')

    y += 6

    #third line
    scr.addstr(y - 1, 9, "┏━━┯━━┯━━┯━━┯━━┯━━┯━━┯━━┓ ┏━━┯━━┯━━┯━━┯━━┯━━┯━━┯━━┓")
    scr.addstr(y + 1, 37, '07 06 05 04 03 02 01 00')
    scr.addstr(y + 2, 37, '\_____________________/')
    scr.addstr(y + 3, 48, 'SA')
    scr.addstr(y + 1, 11, '04 03 02 01')

def draw_switch_labels():

    scr.addstr(1, 118, "LAMPS ON")
    scr.addstr(5, 117, "LAMPS OFF")

    sl = [ 'SITE', 'INCE', 'INAR', 'STOC', 'ACON', 'ACOV', 'RICI', 'PATE', 'PAPA' ]
    y = 8
    x = 85
    for i,lbl in enumerate(sl):
        scr.addstr(y, x, sl[i])
        x += 5

    y = 16
    x = 90
    scr.addstr(y - 3, x + 17, 'AM')
    scr.addstr(y - 2, x, ' ___________________________________')
    scr.addstr(y - 1, x, '/                                   \\')
    for i in range(0, 8):
        scr.addstr(y, x + i * 5, "%02d" % (15 - i))

    y = 22
    scr.addstr(y - 2, x, ' ___________________________________')
    scr.addstr(y - 1, x, '/                                   \\')
    for i in range(0, 8):
        scr.addstr(y, x + i * 5, "%02d" % (7 - i))


def draw_dial_labels():
    dl = [ 'V4', 'L3', 'V3', 'R1-L2', 'V2', 'L1', 'V1', 'V1 SCR', 'V1 LETT', 'NORM', 'PO', 'FI-UR', 'SO', 'FO' ]
    # TODO

def draw_leds(pos_y, pos_x, value):
    global SCREEN
    n = 16
    xval = pos_x + ((led_spacing * n) - 6) // 2
    for i, x in enumerate(range(pos_x, pos_x + (led_spacing * n), led_spacing)):
        off = 1

        if SCREEN == 'top':
            if (i > 7):
                off = 3
        else:
            if (i > 11):
                off = 7
            elif (i > 7):
                off = 5
            elif (i > 3):
                off = 3

        if (value >> (n - (i + 1))) % 2:
            val = led_on
            scr.addch(pos_y, x + off, led_on, curses.color_pair(1))
        else:
            scr.addch(pos_y, x + off, led_off, curses.color_pair(1))

def draw_switch(pos_y, pos_x, val=False):
    if not val:
        scr.addch(pos_y, pos_x, sw_down)
        scr.addch(pos_y + 1, pos_x, '◯')
    else:
        scr.addch(pos_y - 1, pos_x, '◯')
        scr.addch(pos_y, pos_x, sw_up)

def draw_switch_row(pos_y, pos_x, n, value):
    sw_spacing = 5
    for i, x in enumerate(range(pos_x, pos_x + (sw_spacing * n), sw_spacing)):
        if (value >> (n - (i + 1))) % 2:
            val = True
        else:
            val = False
        draw_switch(pos_y, pos_x + (i * sw_spacing) + 1, val)

def draw_top_panel():
    global MS_VAL, AM_VAL, LP_RO, LP_SO, LP_SA
    draw_led_labels()
    draw_switch_labels()

    # Lights
    draw_leds(16, 10, LP_RO)
    draw_leds(22, 10, LP_SO)
    draw_leds(28, 10, LP_SA)

    # Switches
    draw_switch_row(10, 85, 9, MS_VAL)
    draw_switch_row(18, 90, 8, AM_VAL & 0xFF)
    draw_switch_row(24, 90, 8, (AM_VAL & 0xFF00) >> 8)
    draw_switch(3,122,True)

def draw_front_labels():
    scr.addstr(15, 98, "ADD REG")
    for x in range(76, MAX_X - 10, 14):
        scr.addstr(17,x,"8  4  2  1")

    scr.addstr(19, 113, "OP.REG")
    for x in range(104, MAX_X - 10, 14):
        scr.addstr(21,x,"8  4  2  1")
    scr.addstr(21, 76, "OF NZ IM JE   I  C1 C2 C3")

    button_labels = [
            ['AC ON',''],
            ['',''],
            ['DC ALRT','POW OFF'],
            ['POWER', 'ON'],
            ['MAINT.', 'ON'],
            ['SW 1',''],
            ['SW 2',''],
            ['STEP BY','STEP'],
            ['LOAD 1', 'LOAD 2'],
            ['EMERGEN', 'OFF'],
            ['',''],
            ['STANDBY',''],
            ['',''],
            ['MEM CHK', 'INV ADD'],
            ['CLEAR',''],
            ['LOAD',''],
            ['HALT','START'],
            ['OPER.','CALL']
            ]
    for i,x in enumerate(range(3, 69, 8)):
        scr.addstr(15, x, button_labels[i][0], curses.color_pair(4))
        scr.addstr(16, x, button_labels[i][1], curses.color_pair(4))
    for i,x in enumerate(range(3, 69, 8)):
        if (i == 0):
            scr.addstr(19, x, button_labels[i + 9][0], curses.color_pair(3))
            scr.addstr(20, x, button_labels[i + 9][1], curses.color_pair(3))
        else:
            scr.addstr(19, x, button_labels[i + 9][0], curses.color_pair(4))
            scr.addstr(20, x, button_labels[i + 9][1], curses.color_pair(4))

def draw_front_panel():
    global MS_VAL, AM_VAL, LP_RO, LP_SO, LP_SA
    for y in range(0, 14):
        for x in range(0, MAX_X):
            scr.addch(y,x,' ', curses.color_pair(2))
    for y in range(14, 23):
        for x in range(0, 2):
            scr.addch(y,x,' ', curses.color_pair(2))
        for x in range(MAX_X - 2, MAX_X):
            scr.addch(y,x,' ', curses.color_pair(2))

    for y in range(23, MAX_Y - 3):
        for x in range(0, MAX_X):
            scr.addch(y,x,' ', curses.color_pair(2))
    draw_leds(16, 75, LP_ADD_REG)
    draw_leds(20, 75, LP_OP_REG)

    # TODO: buttons change colors based on LP_ALERTS
    # Buttons: top row
    for y in range (15, 18):
        for x in range (3, 75):
            if (x - 2) % 8 == 0:
                scr.addch(y,x,' ')
            else:
                scr.addch(y,x,' ', curses.color_pair(4))
    # Buttons: bottom row
    for y in range (19, 22):
        for x in range (3, 75):
            if (x - 2) % 8 == 0:
                scr.addch(y,x,' ')
            else:
                if (x < 10):
                    scr.addch(y,x,' ', curses.color_pair(3))
                else:
                    scr.addch(y,x,' ', curses.color_pair(4))
    draw_front_labels()


def parse_mouse(i, y, x):
    global MS_VAL
    global AM_VAL
    global SCREEN
    if (SCREEN == 'top'):
        x0 = 90
        if y < 8:
            return
        if y < 16:
            x0 = 85
            row = 0
        elif y < 21:
            row = 1
        elif y < 28:
            row = 2
        else:
            return
        if (x < x0):
            return
        pos = (x - x0) // 5
        if (row == 1) and pos > 8:
            return
        if (row != 1) and pos > 7:
            return
        if (row == 1):
            pos += 8
        if (row > 0):
            bp = 15 -pos
            if (AM_VAL & (1 << bp)) == 0:
                AM_VAL |= (1 << bp)
            else:
                AM_VAL &= ~(1 << bp)
            return
        bp = 8 - pos
        if (MS_VAL & (1 << bp)) == 0:
            MS_VAL |= (1 << bp)
        else:
            MS_VAL &= ~(1 << bp)
    elif SCREEN == 'front':
        if (y < 14) or (y > 22):
            return
        if (x < 5) or (x > 75):
            return
        # TODO: Clicking buttons in the front panel
        pass


def comm_cpu():
    global CPU_sock;
    global CPU_sock_connected;
    if CPU_sock is None:
        sockname = "/tmp/gemu.console.client"
        CPU_sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
        CPU_sock.setblocking(False)
        try:
            os.unlink(sockname)
        except:
            pass
        CPU_sock.bind(sockname)
        statusbar("CPU socket created.")
    if CPU_sock_connected == False:
        statusbar("Connecting to CPU...")
        try:
            CPU_sock.connect("/tmp/gemu.console")
        except:
            return False
    CPU_sock_connected == True
    statusbar("Connected to CPU.")
    try:
        CPU_sock.send(struct.pack("HHHHHHHHHb", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
    except:
        pass
    try:
        r = CPU_sock.recv(1024)
        if r != None:
            CPU_read_status(r)
            return True
    except:
        return False
    return False



# Curses wrapped function
def main(stdscr):
    global SCREEN
    global LP_SO, LP_SA
    scr.timeout(100)
    while True:
        if SCREEN == 'top':
            draw_top_panel()
        elif SCREEN == 'front':
            draw_front_panel()

        scr.refresh()

        # Blocking point
        k = scr.getch()
        if k == -1:
            comm_cpu()
            continue
            #if (False == comm_cpu()):
            #    continue
        if k == ord('q') or k == ord('Q'):
            break
        if k == ord('\t'):
            switch_screen()
            scr.refresh()
            continue
        if k == curses.KEY_MOUSE:
            (m_id, mx, my, mz, bstat) = curses.getmouse()
            parse_mouse(m_id, my, mx)
            LP_SA += 1;
        scr.clear()


wrapper(main)

curses.echo()
curses.endwin()
