/*
 * channel.c - generic CPU<->peripheral line bundle (see channel.h).
 *
 * Pure line-level state: these ops only set/read the lines of a ge_channel.
 * The CPU microcode reads the lines (request -> RC0x, data -> NE_knot, fini ->
 * RIG1) and drives output via channel_accept_output -> the peripheral sink. No
 * CPU flip-flops are forced here; the machine's own sequencer moves the data.
 */

#include "channel.h"

void channel_offer_input(struct ge *ge, struct ge_channel *ch,
                         uint8_t data, uint8_t end)
{
    (void)ge;
    ch->data       = data;
    ch->data_valid = 1;
    ch->fini       = end ? 1 : 0;
    ch->req        = 1;   /* raise the cycle request (OR-source for RC02) */
}

void channel_clear_input(struct ge *ge, struct ge_channel *ch)
{
    (void)ge;
    ch->data       = 0;
    ch->data_valid = 0;
    ch->fini       = 0;
    ch->req        = 0;
}

void channel_accept_output(struct ge *ge, struct ge_channel *ch, uint8_t c)
{
    if (ch->sink)
        ch->sink(ge, ch, c);
}

uint8_t channel_get_req(struct ge_channel *ch)  { return ch->req; }
uint8_t channel_get_data(struct ge_channel *ch) { return ch->data; }
uint8_t channel_get_fini(struct ge_channel *ch) { return ch->fini; }
