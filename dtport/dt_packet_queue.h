#ifndef DT_PACKET_QUEUE_H
#define DT_PACKET_QUEUE_H

#include "dt_av.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QUEUE_MAX_PACK_NUM 8
#define QUEUE_MAX_ABUF_SIZE 20*1024*1024	//20M
#define QUEUE_MAX_VBUF_SIZE 10*1024*1024	// 10M

typedef struct pack
{
	dt_av_frame_t frame;
	//dt_av_frame_t *next;
	struct pack *next;
} dt_packet_list_t;

typedef struct
{
	int nb_packets;
	int size;
	dt_packet_list_t *first, *last;	//get packet from first packet and insert packet after last packet
	int nmutex;
} dt_packet_queue_t;

int packet_queue_init (dt_packet_queue_t * queue);
int packet_queue_get (dt_packet_queue_t * queue, dt_av_frame_t * frame);
int packet_queue_put (dt_packet_queue_t * queue, dt_av_frame_t * frame);
int packet_queue_size (dt_packet_queue_t * queue);
int packet_queue_data_size (dt_packet_queue_t * queue);
int packet_queue_release (dt_packet_queue_t * queue);

#endif
