#include "dt_buffer.h"

#include "stdlib.h"
#include "stdint.h"
#include "string.h"

/*buffer ops*/

int init_buf(dt_buffer_t * bs, int size)
{
	uint8_t *buffer = (uint8_t *)malloc(size);
	if (!buffer) {
		printf("Err:malloc failed \n");
		bs->data = NULL;
		return -1;
	}
	bs->data = buffer;
	bs->size = size;
	bs->level = 0;
	bs->rd_ptr = bs->wr_ptr = bs->data;
	bs->bInited = 1;
	bs->nMutex = 1;
	return 0;
}

int reset_buf(dt_buffer_t * bs)
{
	if (bs->bInited == 0)
		return -1;
	bs->level = 0;
	bs->rd_ptr = bs->wr_ptr = bs->data;
	bs->nMutex = 1;
	return 0;
}

int uninit_buf(dt_buffer_t * bs)
{
	if (bs->data)
		free(bs->data);
	return 0;
}

//1 empty 0 not empty -1 not inited
int is_buf_empty(dt_buffer_t * bs)
{
	if (bs->bInited == 0)
		return -1;
	if (bs->level == 0)
		return 1;
	else
		return 0;
}

//1 full 0 not full -1 not inited
int is_buf_full(dt_buffer_t * bs)
{
	if (bs->bInited == 0)
		return -1;
	if (bs->level == bs->size)
		return 1;
	else
		return 0;

}

int get_buf_space(dt_buffer_t * bs)
{
	if (bs->bInited == 0)
		return -1;
	return bs->size - bs->level;
}

int get_buf_level(dt_buffer_t * bs)
{
	if (bs->bInited == 0)
		return -1;
	return bs->level;
}

static int read_data(dt_buffer_t * bs, uint8_t *out, int size)
{
	if (bs->bInited == 0)
		return -1;	//read failed
	int ret = is_buf_empty(bs);
	if (ret == 1 || ret < 0) {
		//      printf("=====buffer empty \n");
		return -1;	//buffer empty
	}
	int len = MIN(bs->level, size);
	if (bs->wr_ptr > bs->rd_ptr) {
		memcpy(out, bs->rd_ptr, len);
		bs->rd_ptr += len;
		bs->level -= len;
//              printf("=====read ok: condition 1 read :%d byte \n",len);
		return len;
	} else if (len < (bs->data + bs->size - bs->rd_ptr)) {
		memcpy(out, bs->rd_ptr, len);
		bs->rd_ptr += len;
		bs->level -= len;
//              printf("=====read ok: condition 2 read :%d byte \n",len);
		return len;

	} else {
		int tail_len = (bs->data + bs->size - bs->rd_ptr);
		memcpy(out, bs->rd_ptr, tail_len);
		memcpy(out + tail_len, bs->data, len - tail_len);
		bs->rd_ptr = bs->data + len - tail_len;
		bs->level -= len;
//              printf("=====read ok: condition 3 read :%d byte \n",len);
		return len;
	}

}

//1 empty 0 not empty -1 not inited
int is_buffer_empty(dt_buffer_t * bs)
{
	if (bs->bInited == 0)
		return -1;
	if (bs->level == 0)
		return 1;
	else
		return 0;
}

//1 full 0 not full -1 not inited
int is_buffer_full(dt_buffer_t * bs)
{
	if (bs->bInited == 0)
		return -1;
	if (bs->level == bs->size)
		return 1;
	else
		return 0;

}

int read_buf(dt_buffer_t * bs, uint8_t *out, int size)
{
	int ret = 0;
	if (bs->nMutex == 1) {
		bs->nMutex = 0;
		ret = read_data(bs, out, size);
		bs->nMutex = 1;
	}
	return ret;
}

static int write_data(dt_buffer_t * bs, uint8_t *in, int size)
{
	if (bs->bInited == 0)
		return -1;	//not inited
	int ret = is_buffer_full(bs);
	if (ret == 1) {

		printf("=====buffer full \n");
		return -1;	//buffer full
	}
	//start write data
	int len = MIN(bs->size - bs->level, size);
	if (bs->wr_ptr <= bs->rd_ptr) {
		memcpy(bs->wr_ptr, in, len);
		bs->wr_ptr += len;
		bs->level += len;
//              printf("=====write ok: condition 1 write :%d byte \n",len);
		return len;
	} else if (len < (bs->data + bs->size - bs->wr_ptr)) {
		memcpy(bs->wr_ptr, in, len);
		bs->wr_ptr += len;
		bs->level += len;
//              printf("=====write ok: condition 2 write :%d byte \n",len);
		return len;

	} else {
		int tail_len = (bs->data + bs->size - bs->wr_ptr);
		memcpy(bs->wr_ptr, in, tail_len);
		memcpy(bs->data, in + tail_len, len - tail_len);
		bs->wr_ptr = bs->data + len - tail_len;
		bs->level += len;
//              printf("=====write ok: condition 3 write :%d byte \n",len);
		return len;
	}

}

int write_buf(dt_buffer_t * bs, uint8_t *in, int size)
{
	int ret = 0;
	if (bs->nMutex == 1) {
		bs->nMutex = 0;
		ret = write_data(bs, in, size);
		bs->nMutex = 1;
	}
	return ret;
}
