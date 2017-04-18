#ifndef DT_INTERRUPT_T
#define DT_INTERRUPT_T

typedef struct dt_interrupt_cb {
    int (*callback)(void*);
    void *opaque;
} dt_interrupt_cb;

int dt_check_interrupt(dt_interrupt_cb *cb);

#endif
