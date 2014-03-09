#ifndef DT_BUFFER_T
#define DT_BUFFER_T

#include "dt_lock.h"
#include "dt_macro.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	uint8_t *data;
	int size;
	int level;
	uint8_t *rd_ptr;
	uint8_t *wr_ptr;
	bool bInited;
	int nMutex;		//0 busy 1 free
} dt_buffer_t;

int init_buf(dt_buffer_t * bs, int size);
int reset_buf(dt_buffer_t * bs);
int uninit_buf(dt_buffer_t * bs);
int is_buf_empty(dt_buffer_t * bs);
int is_buf_full(dt_buffer_t * bs);
int get_buf_space(dt_buffer_t * bs);
int get_buf_level(dt_buffer_t * bs);
int is_buffer_empty(dt_buffer_t * bs);
int is_buffer_full(dt_buffer_t * bs);
int read_buf(dt_buffer_t * bs, uint8_t *out, int size);
int write_buf(dt_buffer_t * bs, uint8_t *in, int size);

#endif
