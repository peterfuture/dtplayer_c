/*
 * =====================================================================================
 *
 *    Filename   :  dt_event.c
 *    Description:
 *    Version    :  1.0
 *    Created    :  2016年04月22日 11时13分05秒
 *    Revision   :  none
 *    Compiler   :  gcc
 *    Author     :  peter-s
 *    Email      :  peter_future@outlook.com
 *    Company    :  dt
 *
 * =====================================================================================
 */

/* event-send-handle module */
/*
 This is event transport module,

 service_mgt:
 service manage structure

 main-service:
 first service, Receive and store all event
 can not be removed

 event_transport_loop
 get event form main-service, transport to dest service

*/

/*
 * User manual
 *
 * 1 create service manager context
 * dt_service_mgt_t *mgt = dt_service_create();
 *
 * 2 alloc & register user service
 * service_t *service = dt_alloc_service(EVENT_SERVER_ID_TEST, EVENT_SERVER_NAME_TEST);
 * dt_register_service(mgt, service);
 *
 * 3 alloc & send event
 * event_t *event = dt_alloc_event(EVENT_SERVER_ID_TEST, EVNET_TEST);
 * dt_send_event_sync(mgt, event);
 *
 * 4 query or get event and handle
 * event_t *event_tmp = dt_get_event(service);
 *
 * 5 remove user service
 * dt_remove_service(mgt, service);
 *
 * 6 release service manager & main-service
 * dt_service_release(mgt);
 *
 * Attention:
 * 1 make sure remove all user defined service before release service manager
 * 2 EVERN_SERVER_ID_MAIN 0X0 is reservice. User defined event service can not reuser 0x0
 *
 * */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "dt_event.h"
#include "dt_mem.h"
#include "dt_log.h"

#define TAG "SERVICE"

/* private func */
static void *event_transport_loop();
static int dt_transport_event(event_t * event, dt_service_mgt_t * mgt);

dt_service_mgt_t *dt_service_create()
{
    pthread_t tid;
    int ret = 0;
    dt_service_mgt_t *mgt = (dt_service_mgt_t *)dt_mallocz(sizeof(
                                dt_service_mgt_t));
    mgt->service = NULL;
    mgt->service_count = 0;
    mgt->exit_flag = 0;
    dt_lock_init(&mgt->service_lock, NULL);

    service_t *service = dt_alloc_service(EVENT_SERVER_ID_MAIN,
                                          EVENT_SERVER_NAME_MAIN);
    service->event_count = 0;
    service->event = NULL;
    service->next = NULL;
    dt_lock_init(&service->event_lock, NULL);

    ret = dt_register_service(mgt , service);
    if (ret < 0) {
        dt_error(TAG, "SERVER REGISTER FAILED \n");
        ret = -1;
        goto end;
    }

    ret = pthread_create(&tid, NULL, (void *) &event_transport_loop, mgt);
    if (ret != 0) {
        dt_error(TAG, "TRANSTROP LOOP CREATE FAILED \n");
        ret = -2;
        goto end;
    }
    mgt->transport_loop_id = tid;
    dt_info(TAG, "TRANSTROP LOOP CREATE OK, TID:%u \n", (unsigned)tid);
end:
    if (ret < 0) {
        free(service);
        free(mgt);
    }
    return mgt;
}

/*
 * release service
 * free memory, stop event trans loop
 * */
int dt_service_release(dt_service_mgt_t *mgt)
{
    dt_lock(&mgt->service_lock);
    /*stop loop */
    mgt->exit_flag = 1;
    pthread_join(mgt->transport_loop_id, NULL);
    /*
     * Remove service & event
     * */
    service_t *entry = mgt->service;
    service_t *entry_next = NULL;
    while (entry) {
        entry_next = entry->next;
        dt_remove_service(mgt, entry);
        entry = entry_next;
    }

    dt_unlock(&mgt->service_lock);
    free(mgt);
    return 0;
}

service_t *dt_alloc_service(int id, char *name)
{
    service_t *service = (service_t *) malloc(sizeof(service_t));
    if (service) {
        service->event = NULL;
        service->event_count = 0;
        service->id = id;
        if (strlen(name) < MAX_EVENT_SERVER_NAME_LEN) {
            strcpy(service->name, name);
        } else {
            strcpy(service->name, "Unkown");
        }
        service->next = NULL;
        dt_lock_init(&service->event_lock, NULL);
    } else {
        service = NULL;
    }
    dt_info(TAG, "ALLOC SERVER:%p \n", service);
    return service;
}

int dt_register_service(dt_service_mgt_t *mgt, service_t * service)
{
    int ret = 0;
    if (!mgt) {
        dt_error(TAG, "SERVICE MGT IS NULL\n");
        return -1;
    }
    dt_lock(&mgt->service_lock);
    if (mgt->service_count == 0) {
        mgt->service = service;
    }
    service_t *entry = mgt->service;
    while (entry->next != NULL) {
        if (entry->id == service->id) {
            dt_error(TAG, "SERVICE HAS BEEN REGISTERD BEFORE\n");
            ret = -1;
            goto FAIL;
        }
        entry = entry->next;
    }
    if (entry->next == NULL) {
        entry->next = service;
        service->next = NULL;
        mgt->service_count++;
    }
    dt_unlock(&mgt->service_lock);
    dt_info(TAG, "SERVICE:%s REGISTER OK,SERVERCOUNT:%d \n", service->name,
            mgt->service_count);
    return 0;

FAIL:
    dt_unlock(&mgt->service_lock);
    return ret;

}

/*
    for main service, we just remove event
    for normal service, need to remove event and service
 */

int dt_remove_service(dt_service_mgt_t *mgt, service_t * service)
{
    int count = service->event_count;
    service_t *entry = mgt->service;

    dt_info(TAG, "REMOVE %s \n", service->name);
    /*remove all events */
    if (count == 0) {
        goto REMOVE_SERVICE;
    }
    event_t *event = service->event;
    event_t *event_next = event->next;

    while (event) {
        free(event);
        service->event_count--;
        event = event_next;
        if (event) {
            event_next = event->next;
        }
    }

REMOVE_SERVICE:
    while (entry->next && entry->next->id != service->id) {
        entry = entry->next;
    }

    if (entry->next && entry->next->id == service->id) {
        entry->next = entry->next->next;
        mgt->service_count--;
    }
    if (service->event_count > 0) {
        dt_warning(TAG, "EVENT COUNT !=0 AFTER REMOVE \n");
    }
    dt_info(TAG, "Remove service:%s success \n", service->name);
    free(service);
    return 0;
}

event_t *dt_alloc_event(int service, int type)
{
    event_t *event = (event_t *) malloc(sizeof(event_t));
    if (!event) {
        dt_error(TAG, "EVENT ALLOC FAILED \n");
        return NULL;
    }
    event->next = NULL;
    event->service_id = service;
    event->type = type;
    return event;
}

int dt_send_event_sync(dt_service_mgt_t *mgt, event_t * event)
{
    service_t *service_hub = mgt->service;
    if (!service_hub) {
        dt_error(TAG, "EVENT SEND FAILED \n");
        return -1;
    }
    dt_transport_event(event, mgt);
    return 0;
}

int dt_send_event(dt_service_mgt_t *mgt, event_t * event)
{
    service_t *service_hub = mgt->service;
    if (!service_hub) {
        dt_error(TAG, "EVENT SEND FAILED \n");
        return -1;
    }
    dt_lock(&service_hub->event_lock);
    if (service_hub->event_count == 0) {
        service_hub->event = event;
    } else {
        event_t *entry = service_hub->event;
        while (entry->next) {
            entry = entry->next;
        }
        entry->next = event;

    }
    service_hub->event_count++;
    dt_unlock(&service_hub->event_lock);
    dt_debug(TAG, "EVENT:%d SEND OK, event count:%d \n", event->type,
             service_hub->event_count);
    return 0;
}

static int dt_add_event(event_t * event, service_t * service)
{
    service_t *service_hub = service;
    if (!service_hub) {
        dt_error(TAG, "EVENT SEND FAILED \n");
        return -1;
    }
    dt_lock(&service_hub->event_lock);
    if (service_hub->event_count == 0) {
        service_hub->event = event;
    } else {
        event_t *entry = service_hub->event;
        while (entry->next) {
            entry = entry->next;
        }
        entry->next = event;

    }
    service_hub->event_count++;
    dt_unlock(&service_hub->event_lock);
    return 0;
}

event_t *dt_peek_event(service_t * service)
{
    event_t *entry = NULL;
    dt_lock(&service->event_lock);
    if (service->event_count > 0) {
        entry = service->event;
    }
    dt_unlock(&service->event_lock);
    if (entry != NULL) {
        dt_info(TAG, "PEEK EVENT:%d From Server:%s \n", entry->type, service->name);
    }
    return entry;
}

event_t *dt_get_event(service_t * service)
{
    event_t *entry = NULL;
    dt_lock(&service->event_lock);
    if (service->event_count > 0) {
        entry = service->event;
        service->event = entry->next;
        service->event_count--;
        entry->next = NULL;
    }
    dt_unlock(&service->event_lock);
    if (entry != NULL) {
        dt_info(TAG, "GET EVENT:%d From Server:%s \n", entry->type, service->name);
    }
    return entry;
}

static int dt_transport_event(event_t * event, dt_service_mgt_t * mgt)
{
    dt_lock(&mgt->service_lock);
    service_t *entry = mgt->service;

    int ret = 0;
    while (entry->id != event->service_id) {
        entry = entry->next;
        if (!entry) {
            break;
        }
    }

    if (!entry) {
        dt_error(TAG, "Could not found service for:%d \n ", event->service_id);
        ret = -1;
        goto FAIL;
    }

    ret = dt_add_event(event, entry);
    if (ret < 0) {              //we need to free this event
        dt_error(TAG, "we can not transport this event, id:%d type:%d\n ",
                 event->service_id, event->type);
        free(event);
        ret = -1;
        goto FAIL;
    }
    dt_unlock(&mgt->service_lock);
    return ret;
FAIL:
    dt_unlock(&mgt->service_lock);
    return ret;
}

static void *event_transport_loop(void *arg)
{
    dt_service_mgt_t *mgt = (dt_service_mgt_t *)arg;
    service_t *service_hub = mgt->service;
    event_t *event = NULL;
    do {
        if (mgt->exit_flag) {
            goto QUIT;
        }
        event = dt_get_event(service_hub);
        if (!event) {
            usleep(10 * 1000);
            continue;
        }
        dt_transport_event(event, mgt);
    } while (1);

QUIT:
    dt_info(TAG, "EVENT_TRANSPORT_LOOP QUIT OK\n");
    pthread_exit(NULL);
    return NULL;
}
