#ifndef DT_PACKET_QUEUE_H
#define DT_PACKET_QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dtp_av.h"
#include "dt_lock.h"

#define QUEUE_MAX_PACK_NUM 8
#define QUEUE_MAX_ABUF_SIZE 20*1024*1024 // 20M
#define QUEUE_MAX_VBUF_SIZE 50*1024*1024 // 50M

typedef struct pack {
    dt_av_pkt_t *frame;
    //dt_av_pkt_t *next;
    struct pack *next;
} dt_packet_list_t;

typedef struct {
    int nb_packets;
    int size;
    dt_packet_list_t *first,
                     *last; //get packet from first packet and insert packet after last packet
    dt_lock_t mutex;
} dt_packet_queue_t;

int packet_queue_init(dt_packet_queue_t * queue);
int packet_queue_get(dt_packet_queue_t * queue, dt_av_pkt_t ** frame);
int packet_queue_put(dt_packet_queue_t * queue, dt_av_pkt_t * frame);
int packet_queue_size(dt_packet_queue_t * queue);
int packet_queue_data_size(dt_packet_queue_t * queue);
int packet_queue_release(dt_packet_queue_t * queue);

#endif
