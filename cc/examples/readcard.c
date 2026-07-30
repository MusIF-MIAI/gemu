/* Read one 80-column card through the integrated reader and return its first
 * column. Shows the whole peripheral interface: open, transfer, status, close,
 * with no PER instruction or order block in sight. */
#include <ge.h>

char card[80];

int main(void)
{
    int rdr = _open_reader();

    _read(rdr, card, 80);

    if (GE_STATUS(rdr) != GE_QR_GT)
        lon();                  /* light OPERATOR CALL and let the engineer look */

    _close(rdr);
    return card[0];
}
