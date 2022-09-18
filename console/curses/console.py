#!/usr/bin/python3



import curses
from curses import wrapper
import time

led_off='⚆'
led_on='⚈'
led_spacing = 3

sw_up = '┸'
sw_down = '┰'

MS_VAL = 0
AM_VAL = 0



scr = curses.initscr()
curses.noecho()
curses.cbreak()
curses.nocbreak()
curses.mousemask(1)
scr.keypad(True)
curses.curs_set(0)
curses.start_color()
led_colorpair = curses.init_pair(1, curses.COLOR_YELLOW, curses.COLOR_BLACK)
#
#
#    if (n >= 4):
#        scr.addstr(pos_y + 3, xval, "[" + hex(value & 0xff) + "]")
        #scr.addstr(pos_y + 1, pos_x, "\__")

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
    n = 16
    xval = pos_x + ((led_spacing * n) - 6) // 2
    for i, x in enumerate(range(pos_x, pos_x + (led_spacing * n), led_spacing)):
        off = 1
        if (i > 7):
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

def parse_mouse(i, y, x):
    global MS_VAL
    global AM_VAL
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








def main(stdscr):
    # Clear screen
    val = [ 0x0157, 0x34, 0x99 ]
    while True:
        scr.clear()

        # Led labels & leds
        draw_led_labels()
        draw_leds(16, 10, val[0])
        draw_leds(22, 10, val[1])
        draw_leds(28, 10, val[2])
        val[2]+=1

        #switch labels & switches
        draw_switch_labels()
        draw_switch_row(10, 85, 9, MS_VAL)
        draw_switch_row(18, 90, 8, AM_VAL & 0xFF)
        draw_switch_row(24, 90, 8, (AM_VAL & 0xFF00) >> 8)
        draw_switch(3,122,True)

        #dial labels & dial
        # TODO WIP
        #

        #bottom buttons + display


        k = scr.getch()
        if k == ord('q') or k == ord('Q'):
            break
        if k == curses.KEY_MOUSE:
            (m_id, mx, my, mz, bstat) = curses.getmouse()
            parse_mouse(m_id, my, mx)
            val[1]+= 1;
            scr.addstr(0, 0, str(mx)+ ' ' + str(my))
        scr.refresh()


wrapper(main)



curses.echo()
curses.endwin()
