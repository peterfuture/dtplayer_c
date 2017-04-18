#include "dt_interrupt.h"

int dt_check_interrupt(dt_interrupt_cb *cb)
{
    int ret;
    if (cb && cb->callback && (ret = cb->callback(cb->opaque))) {
        return ret;
    }
    return 0;
}
