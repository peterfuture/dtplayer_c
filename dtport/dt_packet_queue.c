#include "unistd.h"

#include "dt_utils.h"

#include "dt_packet_queue.h"

#define TAG "PORT-QUEUE"
/*packet queue operation*/
int packet_queue_init(dt_packet_queue_t * queue)
{
    if (!queue) {
        return -1;
    }
    memset(queue, 0, sizeof(dt_packet_queue_t));
    dt_lock_init(&queue->mutex, NULL);
    return 0;
}

int packet_queue_put_frame(dt_packet_queue_t * queue, dt_av_pkt_t * frame)
{
    if (!queue) {
        printf("[%s:%d] queue==NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }
    dt_packet_list_t *list;
    list = malloc(sizeof(dt_packet_list_t));
    if (!list) {
        return -1;
    }
    //list->frame=*frame;
    //memcpy(&list->frame, frame, sizeof(dt_av_pkt_t));
    list->frame = frame;
    list->next = NULL;
    if (!(queue->last)) {
        queue->first = list;
    } else {
        queue->last->next = list;
    }
    queue->last = list;
    queue->nb_packets++;
    queue->size += frame->size;
    //queue->size+=frame->size+sizeof(*list);
    //printf("[%s:%d] packet_nb=%d size:%d \n",__FUNCTION__,__LINE__,queue->nb_packets,queue->size);
    return 0;
}

int packet_queue_put(dt_packet_queue_t * queue, dt_av_pkt_t * frame)
{
    int ret;
    dt_lock(&queue->mutex);

    if (frame->type == 0 && queue->size >= dtp_setting.video_max_size) {
        dt_debug(TAG, "[%s:%d]video packet queue size:%d exceed max:%d\n",
                 __FUNCTION__, __LINE__, queue->size, dtp_setting.video_max_size);
        ret = -1;
        goto END;
    }

    if (frame->type == 1 && queue->size >= dtp_setting.audio_max_size) {
        dt_debug(TAG, "[%s:%d]audio packet queue size:%d exceed max:%d\n",
                 __FUNCTION__, __LINE__, queue->size, dtp_setting.audio_max_size);
        ret = -2;
        goto END;
    }
    ret = packet_queue_put_frame(queue, frame);
END:
    dt_unlock(&queue->mutex);
    //printf("[%s:%d] packet queue in ok\n",__FUNCTION__,__LINE__);
    return ret;
}

int packet_queue_get_frame(dt_packet_queue_t * queue, dt_av_pkt_t **pkt)
{
    if (queue->nb_packets == 0) {
        dt_debug(TAG, "[%s:%d] No packet left\n", __FUNCTION__, __LINE__);
        return -1;
    }
    dt_packet_list_t *list;
    list = queue->first;
    dt_av_pkt_t * frame = NULL;
    if (list) {
        queue->first = list->next;
        if (!queue->first) {
            queue->last = NULL;
        }
        queue->nb_packets--;
        frame = list->frame;
        queue->size -= frame->size;
        //queue->size-=frame->size+sizeof(*list);
        list->frame = NULL;
        *pkt = frame;
        free(list);
        dt_debug(TAG, "[%s:%d] queue get frame ok\n", __FUNCTION__, __LINE__);
        return 0;
    }
    return -1;
}

int packet_queue_get(dt_packet_queue_t * queue, dt_av_pkt_t **frame)
{
    int ret;
    dt_lock(&queue->mutex);
    dt_debug(TAG, "[%s:%d]READ FRAME BEGIN \n", __FUNCTION__, __LINE__);
    ret = packet_queue_get_frame(queue, frame);
    dt_debug(TAG, "[%s:%d]READ FRAME END \n", __FUNCTION__, __LINE__);
    dt_unlock(&queue->mutex);
    return ret;
}

int packet_queue_size(dt_packet_queue_t * queue)
{
    return queue->nb_packets;
}

int packet_queue_data_size(dt_packet_queue_t * queue)
{
    return queue->size;
}

int packet_queue_flush(dt_packet_queue_t * queue)
{
    dt_packet_list_t *list1, *list2;
    dt_lock(&queue->mutex);
    for (list1 = queue->first; list1 != NULL; list1 = list2) {
        list2 = list1->next;
        //dt_debug(TAG,"free queue, data addr:%p list1:%p size:%d\n",list1->frame.data,list1,list1->frame.size);
        //free(list1->frame->data);
        dtp_packet_free(list1->frame);
        //data malloc outside ,but free here if receive stop cmd
        //free(&list1->frame);//will free in free(list1) ops
        free(list1);
    }
    queue->last = NULL;
    queue->first = NULL;
    queue->nb_packets = 0;
    queue->size = 0;
    dt_unlock(&queue->mutex);
    return 0;
}

int packet_queue_release(dt_packet_queue_t * queue)
{
    return packet_queue_flush(queue);
}
