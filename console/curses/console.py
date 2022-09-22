#!/usr/bin/python3
import curses
import socket
import struct
from curses import wrapper
import time
import os
import sys

lamp_off='⚆'
lamp_on='⚈'
lamp_spacing = 3

sw_up = '┸'
sw_down = '┰'
button_labels = [
        ['', 'AC ON',''],
        ['DC ALRT','', 'POW OFF'],
        ['POWER','ON', ''],
        ['MAINT.', 'ON', ''],
        ['', 'SW 1',''],
        ['', 'SW 2',''],
        ['STEP','BY','STEP',],
        ['LOAD 1','', 'LOAD 2'],

        ['EMERGEN', 'OFF', ''],
        ['','STANDBY',''],
        ['','',''],
        ['MEM CHK', '', 'INV ADD'],
        ['', 'CLEAR',''],
        ['', 'LOAD',''],
        ['HALT','', 'START'],
        ['OPER.','CALL', '']
        ]

button_pins = [
        [ 3, 3, 3 ],
        [ 0, -1, 1],
        [ 3, 3, 3],
        [ 4, 4, 4],
        [ 7, 7, 7],
        [ 8, 8, 8],
        [ 9, 9, 9],
        [11, -1, 12],

        [-2, -2, -2],
        [2,  2,  2],
        [ -1, -1, -1],
        [5, -1, 6],
        [-2, -2, -2],
        [-2, -2, -2],
        [10, -2, -2],
        [13, 13, 13],
        ]


MS_VAL = 0
AM_VAL = 0
BUTTONS_VAL = 0
ROT_VAL = 9 # RS_NORM

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

if not curses.has_colors():
    print("Please use a terminal that can set colors!")
    sys.exit(1)
# Colors
curses.init_color(9, 400, 400, 400)
lamp_off_colorpair = curses.init_pair(5,curses.COLOR_BLACK, 9)
curses.init_pair(1, curses.COLOR_YELLOW, curses.COLOR_BLACK)
curses.init_pair(2, curses.COLOR_WHITE, curses.COLOR_BLUE)
curses.init_pair(3, curses.COLOR_WHITE, curses.COLOR_RED)
curses.init_pair(4, curses.COLOR_BLACK, curses.COLOR_WHITE)
curses.init_pair(6, curses.COLOR_BLACK, curses.COLOR_YELLOW)
curses.init_pair(7, curses.COLOR_BLACK, curses.COLOR_CYAN)
MAX_Y,MAX_X = scr.getmaxyx()
clr_off = curses.color_pair(5)
clr_white = curses.color_pair(4)
clr_red = curses.color_pair(3)
clr_yellow = curses.color_pair(6)
clr_blue = curses.color_pair(7)
button_colors = [
        [ clr_red, clr_red, clr_red ],
        [ clr_red, clr_off, clr_yellow ],
        [ clr_yellow, clr_yellow, clr_yellow ],
        [ clr_red, clr_red, clr_red ],
        [ clr_white, clr_white, clr_white],
        [ clr_white, clr_white, clr_white],
        [ clr_white, clr_white, clr_white],
        [ clr_white, clr_off, clr_white],

        [ clr_red, clr_red, clr_red ],
        [ clr_blue, clr_blue, clr_blue ],
        [ clr_off, clr_off, clr_off ],
        [ clr_red, clr_off, clr_red ],
        [ clr_white, clr_white, clr_white],
        [ clr_white, clr_white, clr_white],
        [ clr_white, clr_off, clr_white],
        [ clr_blue, clr_blue, clr_blue ]
        ]


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

def draw_lamp_labels():
    y = 12
    # fuse
    scr.addch(y, 20, '⬢')
    scr.addstr(y, 22, 'F1')
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
    scr.addstr(y + 1, 11, 'B4 B3 B2 B1')

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
    global ROT_VAL
    dl = [ 'V4', 'L3', 'V3', 'R1-L2', 'V2', 'L1', 'V1', 'V1 SCR', 'V1 LETT', 'NORM', 'PO', 'FI-UR', 'SO', 'FO' ]
    # Temporary rotor ascii
    #
    scr.addstr(30, 110, dl[ROT_VAL])
    scr.addstr(30, 100, "<<")
    scr.addstr(30, 120, ">>")

def draw_lamps(pos_y, pos_x, value):
    global SCREEN
    n = 16
    xval = pos_x + ((lamp_spacing * n) - 6) // 2
    for i, x in enumerate(range(pos_x, pos_x + (lamp_spacing * n), lamp_spacing)):
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
            val = lamp_on
            scr.addch(pos_y, x + off, lamp_on, curses.color_pair(1))
        else:
            scr.addch(pos_y, x + off, lamp_off, curses.color_pair(1))

def draw_switch(pos_y, pos_x, val=False):
    if not val:
        scr.addstr(pos_y, pos_x - 1, '(' + sw_down + ')')
        scr.addch(pos_y + 1, pos_x, '⚈')
    else:
        scr.addch(pos_y - 1, pos_x, '⚈')
        scr.addstr(pos_y, pos_x - 1, '(' + sw_up + ')')

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
    draw_lamp_labels()
    draw_switch_labels()

    # Lights
    draw_lamps(16, 10, LP_RO)
    draw_lamps(22, 10, LP_SO)
    draw_lamps(28, 10, LP_SA)

    # Switches
    draw_switch_row(10, 85, 9, MS_VAL)
    draw_switch_row(18, 90, 8, AM_VAL & 0xFF)
    draw_switch_row(24, 90, 8, (AM_VAL & 0xFF00) >> 8)
    draw_switch(3,122,True)

    draw_dial_labels()

def draw_front_labels():
    global LP_ALERTS
    scr.addstr(15, 98, "ADD REG")
    for x in range(76, 124, 14):
        scr.addstr(17,x,"8  4  2  1")

    scr.addstr(19, 113, "OP.REG")
    for x in range(104, 124, 14):
        scr.addstr(21,x,"8  4  2  1")
    scr.addstr(21, 76, "OF NZ IM JE   I  C1 C2 C3")


    for x in range(3, 69, 8):
        i = (x - 3) // 8
        if (i == 1):
            continue
        if (i > 1):
            i -= 1
        wires = button_pins[i]
        for j,w in enumerate(wires):
            if w == -2 or (w != -1 and (LP_ALERTS & (1 << w)) != 0):
                scr.addstr(15 + j, x, button_labels[i][j], button_colors[i][j])
            else:
                scr.addstr(15 + j, x, button_labels[i][j], clr_off)

    for x in range(3, 69, 8):
        i = (x - 3) // 8 + 8
        if (i == 9):
            continue
        if (i > 9):
            i -= 1
        wires = button_pins[i]
        for j,w in enumerate(wires):
            if w == -2 or (w != -1 and (LP_ALERTS & (1 << w)) != 0):
                scr.addstr(19 + j, x, button_labels[i][j], button_colors[i][j])
            else:
                scr.addstr(19 + j, x, button_labels[i][j], clr_off)

def draw_front_panel():
    global MS_VAL, AM_VAL, LP_RO, LP_SO, LP_SA, LP_ALERTS
    for y in range(0, 14):
        for x in range(0, MAX_X):
            scr.addch(y,x,' ', curses.color_pair(2))
    for y in range(14, 23):
        for x in range(0, 2):
            scr.addch(y,x,' ', curses.color_pair(2))
        for x in range(129, MAX_X):
            scr.addch(y,x,' ', curses.color_pair(2))

    for y in range(23, MAX_Y - 2):
        for x in range(0, MAX_X):
            scr.addch(y,x,' ', curses.color_pair(2))
    draw_lamps(16, 75, LP_ADD_REG)
    draw_lamps(20, 75, LP_OP_REG)

    # Buttons: top row
    j = 0
    for y in range (15, 18):
        for x in range (3, 75):
            ii = (x - 3) // 8
            if (ii > 1):
                i = ii - 1
            else:
                i = ii
            if (x - 2) % 8 == 0:
                scr.addch(y,x,' ')
                continue
            wires = button_pins[i]
            w = wires[j]
            if w == -2 or ((w != -1) and (ii != 1) and (LP_ALERTS & (1 << w) != 0)):
                clr = button_colors[i][j]
            else:
                clr = clr_off
            scr.addch(y, x, ' ', clr)
        j += 1
    # Buttons: bottom row
    j = 0
    for y in range (19, 22):
        for x in (range (3, 75)):
            ii = (x - 3) // 8
            if (ii > 1):
                ii -= 1
            i = ii + 8
            if (x - 2) % 8 == 0:
                scr.addch(y,x,' ')
                continue
            wires = button_pins[i]
            w = wires[j]
            if w == -2 or ((w != -1) and (ii != 1) and (LP_ALERTS & (1 << w) != 0)):
                clr = button_colors[i][j]
            else:
                clr = clr_off
            scr.addch(y,x, ' ', clr)
        j += 1
    draw_front_labels()


def parse_mouse(i, y, x):
    global MS_VAL
    global AM_VAL
    global BUTTONS_VAL
    global SCREEN
    global ROT_VAL
    if (SCREEN == 'top'):
        # Temporary rotor
        #
        if (y > 28) and (y < 32) and (x >= 100) and (x < 110):
            ROT_VAL -= 1
            if (ROT_VAL < 0):
                ROT_VAL = 0
            return
        if (y > 28) and (y < 32)  and (x > 120) and (x < 130):
            ROT_VAL += 1
            if (ROT_VAL > 13):
                ROT_VAL = 13
            return
        # End temp rotor
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
        if (x < 3) or (x > 75):
            return
        if (y < 18):
            button_pressed = ((x - 3) // 8)
        else:
            button_pressed = ((x - 3) // 8) + 9
        if (button_pressed == 1 or button_pressed == 10):
            return
        if (button_pressed > 1) and (button_pressed < 10):
            button_pressed -= 1
        if (button_pressed > 10):
            button_pressed -= 2
        BUTTONS_VAL |= (1 << button_pressed)


def comm_cpu():
    global CPU_sock;
    global CPU_sock_connected;
    global MS_VAL, AM_VAL, BUTTONS_VAL, ROT_VAL
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
        CPU_sock.send(struct.pack("HHHHHHHHHb",
            0, 0, 0,  # Console lamps, do not set
            0, 0, 0,  # Front panel lamps
            MS_VAL, AM_VAL, # Switches
            BUTTONS_VAL,
            ROT_VAL))

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
    global LP_SO, LP_SA, LP_ALERTS
    scr.timeout(20)
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
            # Test
            LP_ALERTS += 1;
        scr.clear()


wrapper(main)

curses.echo()
curses.endwin()
