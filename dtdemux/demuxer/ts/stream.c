/* stream.c: basic stream and packet-handling */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_tsdemux.h"

static void *
xalloc(size_t nbytes)
{
    void *p;

    if (NULL == (p = calloc(1, nbytes))) {
        fprintf(stderr, "Memory allocation error: failed to allocate %lu bytes\n", (unsigned long) nbytes);
        abort();
    }
    return p;
}

ts_stream_t *
ts_stream_create(const ts_options_t *opts)
{
    ts_stream_t *p;
    void *(*alloc)(size_t ptr);

    if (NULL == opts->allocmem) {
        alloc = xalloc;
    } else {
        alloc = opts->allocmem;
    }
    if (NULL == (p = (ts_stream_t *) alloc(sizeof(ts_stream_t)))) {
        return NULL;
    }
    p->opts = opts;
    p->allocmem = alloc;
    if (NULL == opts->freemem) {
        p->freemem = free;
    } else {
        p->freemem = opts->freemem;
    }
    if (NULL == opts->reallocmem) {
        p->reallocmem = realloc;
    } else {
        p->reallocmem = opts->reallocmem;
    }
    p->nitpid = PID_DVB_NIT;
    p->tdtpid = PID_DVB_TDT;
    return p;
}

int
ts_stream_read_packetf(ts_stream_t *stream, ts_packet_t *packet, FILE *src)
{
    size_t c, plen, bp;
    int n;
    uint8_t buf[192], *bufp;

    plen = TS_PACKET_LENGTH;
    bufp = buf;
    if (stream->opts->timecode) {
        plen += sizeof(uint32_t);
    }
    if (stream->opts->prepad) {
        for (c = 0; c < stream->opts->prepad && EOF != fgetc(src); c++);
        if (c < stream->opts->prepad) {
            return -1;
        }
    }
    if (0 == stream->opts->timecode && 1 == stream->opts->autosync) {
        bp = 0xFFFF;
        c = 0;
        if (stream->lastsync) {
            for (c = 0; c <= stream->lastsync; c++) {
                if (EOF == (n = fgetc(src))) {
                    return -1;
                }
                if (0xFFFF == bp && TS_SYNC_BYTE == n) {
                    bp = c;
                }
                bufp[c] = n;
            }
            if (TS_SYNC_BYTE == n) {
                /* Sync byte position matches last packet */
                bufp[0] = TS_SYNC_BYTE;
                plen--;
                bufp++;
            } else if (bp != 0xFFFF) {
                /* Sync byte occurred early */
                fprintf(stderr, "%s: Retraining; sync occurred at relative 0x%02x, expected at 0x%02x\n", stream->opts->filename, (uint8_t) bp, (uint8_t) stream->lastsync);
                if (bp) {
                    /* Move everything from &bufp[bp] back to the start of the
                     * buffer and adjust plen
                     */
                    memmove(bufp, &(bufp[bp]), stream->lastsync - bp);
                    bufp += stream->lastsync - bp;
                    plen -= stream->lastsync - bp;
                }
                stream->lastsync = bp;
            }
        }
        if (0xFFFF == bp) {
            for (; 0 == stream->opts->synclimit || c < stream->opts->synclimit; c++) {
                if (EOF == (n = fgetc(src))) {
                    return -1;
                } else if (TS_SYNC_BYTE == n) {
                    break;
                }
            }
            if (n != TS_SYNC_BYTE) {
                return -1;
            }
            if (stream->lastsync != c) {
                fprintf(stderr, "%s: skipped %lu bytes (autosync)\n", stream->opts->filename, (unsigned long) c);
            }
            stream->lastsync = c;
            bufp[0] = n;
            bufp++;
            plen--;
        }
    }
    if (1 != fread(bufp, plen, 1, src)) {
        return -1;
    }
    if (stream->opts->postpad) {
        for (c = 0; c < stream->opts->postpad && EOF != fgetc(src); c++);
        if (c < stream->opts->postpad) {
            return -1;
        }
    }
    return ts_stream_read_packet(stream, packet, buf);
}

int
ts_stream_read_packet(ts_stream_t *stream, ts_packet_t *packet, const uint8_t *bufp)
{
    memset(packet, 0, sizeof(ts_packet_t));
    packet->stream = stream;
    stream->seq++;
    if (stream->opts->timecode) {
        packet->timecode = (bufp[0] << 24) | (bufp[1] << 16) | (bufp[2] << 8) | bufp[3];
        bufp += 4;
    }
    packet->sync = bufp[0];
    bufp++; /* Skip sync byte */
    //  printf("flags = %02x\n", bufp[0]);
    packet->transerr = (bufp[0] & 0x80) >> 7;
    packet->unitstart = (bufp[0] & 0x40) >> 6;
    packet->priority = (bufp[0] & 0x20) >> 5;
    packet->pid = ((bufp[0] & 0x1f) << 8) | bufp[1];
    packet->sc = (bufp[2] & 0xc0) >> 6;
    packet->hasaf = (bufp[2] & 0x20) >> 5;
    packet->haspd = (bufp[2] & 0x10) >> 4;
    packet->continuity = bufp[2] & 0x0f;
    bufp += 3;
    packet->payloadlen = 184;
    memcpy(packet->payload, bufp, packet->payloadlen);
    return ts__packet_decode(packet);
}

int
ts_stream_fill_packet(ts_stream_t *stream, ts_packet_t *packet, const uint8_t *bufp)
{
    memset(packet, 0, sizeof(ts_packet_t));
    packet->stream = stream;
    stream->seq++;

    packet->sync = bufp[0];
    packet->transerr = (bufp[1] & 0x80) >> 7;
    packet->unitstart = (bufp[1] & 0x40) >> 6;
    packet->priority = (bufp[1] & 0x20) >> 5;
    packet->pid = ((bufp[1] & 0x1f) << 8) | bufp[2];
    packet->sc = (bufp[3] & 0xc0) >> 6;
    packet->hasaf = (bufp[3] & 0x20) >> 5;
    packet->haspd = (bufp[3] & 0x10) >> 4;
    packet->continuity = bufp[3] & 0x0f;


    if (packet->pid == 0x1FFF) {
        //skip
        return 0;
    }

    //  packet->payloadlen = 184;
    //  memcpy(packet->payload, bufp, packet->payloadlen);

    packet->pts = -1;
    packet->dts = -1;

    int adap_len = 0;
    uint8_t *adap_data = NULL;
    int adapation_field_contrl = (bufp[3] & 0x30) >> 4;

    switch (adapation_field_contrl) {
    case 0:
        //printf("no ad, no payload \n");
        packet->payloadlen = 0;
        break;
    case 1:
        //printf("payload only \n");
        adap_len = 0;
        packet->payloadlen = 184;
        memcpy(packet->payload, bufp + 4, packet->payloadlen);
        break;
    case 2:
        //printf("adaption only \n");
        adap_len = bufp[4];
        //printf("adaption only len:%d \n",adap_len);
        if (adap_len > 0) {
            adap_data = bufp + 4;
        }
        packet->payloadlen = 0;
        break;
    case 3:
        //printf("adaption and payload \n");
        adap_len = bufp[4];
        if (adap_len > 0) {
            adap_data = bufp + 4;
        }
        packet->payloadlen = 188 - 5 - adap_len;
        memcpy(packet->payload, bufp + 5 + adap_len, packet->payloadlen);
        break;
    default:
        printf("err case \n");
        packet->payloadlen = 0;
        break;
    }

    //get pts info
    //if(adap_len > 0 && packet->unitstart)
    if (adap_len > 0) {
        uint8_t *pcrbuf = adap_data;
        int len = pcrbuf[0];
        if (len <= 0 || len > 183) { //broken from the stream layer or invalid
            goto QUIT;
        }
        pcrbuf++;
        int flags = pcrbuf[0];
        int has_pcr;
        has_pcr = flags & 0x10;
        pcrbuf++;
        if (!has_pcr) {
            //printf("do not have pcr flag \n");
            goto QUIT;
        }
        int64_t pcr = -1;
        int64_t pcr_ext = -1;
        unsigned int v = 0;
        //v = (uint32_t)pcrbuf[3]<<24 | pcrbuf[2]<<16 | pcrbuf[1]<<8 |pcrbuf[0];
        v = (uint32_t)pcrbuf[0] << 24 | pcrbuf[1] << 16 |   pcrbuf[2] << 8 | pcrbuf[3];
        pcr = ((int64_t)v << 1) | (pcrbuf[4] >> 7);
        pcr_ext = (pcrbuf[4] & 0x01) << 8;
        pcr_ext |= pcrbuf[5];
        packet->pts = (pcr * 300 + pcr_ext) / 300;
        //printf("get pts:%lld pid:%x\n",pcr,packet->pid);
    }
QUIT:
    return 0;
}
