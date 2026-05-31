/*
 * gecode.c - GE 100-series internal graphic character code.
 *
 * GE 100 Series Graphic Character Set — machine byte -> glyph, transcribed from
 * the GE APS Reference Manual (EDV-AFL vol. 03) Figure 3, p.16. The 64-character
 * graphic set occupies 0x40-0x5F and 0xA0-0xBF; all other codes are non-graphic.
 * Used to annotate DB data bytes (gdis) and to render printer/typewriter output
 * with the character it represents ON THE MACHINE (not ASCII — which did not yet
 * exist). Glyphs that have no ASCII equivalent (up/left arrows 0xA0/0xBA, long
 * dash 0xAA, bullet 0xAC) render '.'.
 *
 * This table is corroborated by the CRZ card transcoder: CRZ[2] §5.3 "Table 3 —
 * IBM card code and Internal GECB code equivalent" (vol. [2] p.113) shows the
 * transcoder converts card punches to this same internal code (digit '0'=0x40,
 * 'J'=0xA1). The EBCDIC-like codes in transcode.c (digits 0xF0-0xF9) are an
 * artifact of the externally-supplied funktionalcpu.bin, not the machine's
 * code — see docs/ISA.md §2.1 for the full reconciliation.
 */

#include "gecode.h"

static const char GE_GLYPH[256] = {
    [0x40]='0',[0x41]='1',[0x42]='2',[0x43]='3',[0x44]='4',[0x45]='5',
    [0x46]='6',[0x47]='7',[0x48]='8',[0x49]='9',
    [0x4A]='[',[0x4B]='#',[0x4C]='@',[0x4D]=':',[0x4E]='>',[0x4F]='?',
    [0x50]=' ',[0x51]='A',[0x52]='B',[0x53]='C',[0x54]='D',[0x55]='E',
    [0x56]='F',[0x57]='G',[0x58]='H',[0x59]='I',
    [0x5A]='&',[0x5B]='.',[0x5C]=']',[0x5D]='(',[0x5E]='<',[0x5F]='\\',
    [0xA1]='J',[0xA2]='K',[0xA3]='L',[0xA4]='M',[0xA5]='N',[0xA6]='O',
    [0xA7]='P',[0xA8]='Q',[0xA9]='R',
    [0xAB]='$',[0xAD]=')',[0xAE]=';',[0xAF]='\'',
    [0xB0]='+',[0xB1]='/',[0xB2]='S',[0xB3]='T',[0xB4]='U',[0xB5]='V',
    [0xB6]='W',[0xB7]='X',[0xB8]='Y',[0xB9]='Z',
    [0xBB]=',',[0xBC]='%',[0xBD]='=',[0xBE]='"',[0xBF]='!',
};

char ge_glyph(uint8_t b)
{
    char c = GE_GLYPH[b];
    return c ? c : '.';
}
