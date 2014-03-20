/* event-send-handle module */
/*
 This is event transport module,

 server_mgt:
 service manage structure

 main-server: 
 first server, Receive and store all event 
 can not be removed

 event_transport_loop
 get event form main-server, transport to dest service

*/

#include "dt_event.h"
#include "dt_event_def.h"
#include "dt_log.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define TAG "EVENT-TRANSPORT"

static dt_server_mgt_t server_mgt;
static event_server_t main_server;

static void *event_transport_loop ();

int dt_event_server_init ()
{
	pthread_t tid;
	int ret = 0;

	server_mgt.server = NULL;
	server_mgt.server_count = 0;
	server_mgt.exit_flag = 0;
	dt_lock_init (&server_mgt.server_lock, NULL);

	main_server.id = EVENT_SERVER_MAIN;
	strcpy (main_server.name, "SERVER-MAIN");
	main_server.event_count = 0;
	main_server.event = NULL;
	main_server.next = NULL;
	dt_lock_init (&main_server.event_lock, NULL);

	ret = dt_register_server (&main_server);
	if (ret < 0)
	{
		dt_error (TAG, "SERVER REGISTER FAILED \n");
		return -1;
	}

	ret = pthread_create (&tid, NULL, (void *) &event_transport_loop, NULL);
	if (ret != 0)
	{
		dt_error (TAG, "TRANSTROP LOOP CREATE FAILED \n");
		return -1;
	}
	server_mgt.transport_loop_id = tid;
	dt_info (TAG, "TRANSTROP LOOP CREATE OK, TID:%d \n", tid);
	return 0;
}

/*
 * release server
 * free memory, stop event trans loop
 * */
int dt_event_server_release ()
{
	dt_server_mgt_t *mgt = &server_mgt;
	dt_lock (&mgt->server_lock);

	/*stop loop */
	mgt->exit_flag = 1;
	pthread_join (mgt->transport_loop_id, NULL);
	/*
	 * for main server, we just remove event
	 * for normal server, need to remove event and server
	 * */
	event_server_t *entry = mgt->server;
	event_server_t *entry_next = NULL;
	while (entry)
	{
		entry_next = entry->next;
		dt_remove_server (entry);
		entry = entry_next;
	}

	dt_unlock (&mgt->server_lock);
	return 0;
}

event_server_t *dt_alloc_server ()
{
	event_server_t *server = (event_server_t *) malloc (sizeof (event_server_t));
	if (server)
	{
		server->event = NULL;
		server->event_count = 0;
		server->id = -1;
		server->next = NULL;
		dt_lock_init (&server->event_lock, NULL);
	}
	else
		server = NULL;
	dt_info (TAG, "ALLOC SERVER:%p \n", server);
	return server;
}

int dt_register_server (event_server_t * server)
{
	dt_server_mgt_t *mgt = &server_mgt;
	int ret = 0;
	if (!mgt)
	{
		dt_error (TAG, "SERVICE MGT IS NULL\n");
		return -1;
	}
	dt_lock (&mgt->server_lock);
	if (mgt->server_count == 0)
		mgt->server = server;
	event_server_t *entry = mgt->server;
	while (entry->next != NULL)
	{
		if (entry->id == server->id)
		{
			dt_error (TAG, "SERVICE HAS BEEN REGISTERD BEFORE\n");
			ret = -1;
			goto FAIL;
		}
		entry = entry->next;
	}
	if (entry->next == NULL)
	{
		entry->next = server;
		server->next = NULL;
		mgt->server_count++;
	}
	dt_unlock (&mgt->server_lock);
	dt_info (TAG, "SERVICE:%s REGISTER OK,SERVERCOUNT:%d \n", server->name, mgt->server_count);
	return 0;

  FAIL:
	dt_unlock (&mgt->server_lock);
	return ret;

}

/*
    for main server, we just remove event
    for normal server, need to remove event and server
 */

int dt_remove_server (event_server_t * server)
{
	dt_server_mgt_t *mgt = &server_mgt;
	int count = server->event_count;

	dt_info (TAG, "REMOVE %s \n", server->name);
	/*remove all events */
	if (count == 0)
		goto REMOVE_SERVICE;
	event_t *event = server->event;
	event_t *event_next = event->next;

	while (event)
	{
		free (event);
		server->event_count--;
		event = event_next;
		if (event)
			event_next = event->next;
	}

  REMOVE_SERVICE:
	if (server->id == EVENT_SERVER_MAIN)
		return 0;
	event_server_t *entry = mgt->server;
	while (entry->next && entry->next->id != server->id)
		entry = entry->next;

	if (entry->next && entry->next->id == server->id)
	{
		entry->next = entry->next->next;
		mgt->server_count--;
	}
	if (server->event_count > 0)
		dt_warning (TAG, "EVENT COUNT !=0 AFTER REMOVE \n");
	if (server->id != EVENT_SERVER_MAIN)
		free (server);
	return 0;
}

event_t *dt_alloc_event ()
{
	event_t *event = (event_t *) malloc (sizeof (event_t));
	if (!event)
	{
		dt_error (TAG, "EVENT ALLOC FAILED \n");
		return NULL;
	}
	event->next = NULL;
	event->server_id = -1;
	event->type = -1;
	return event;
}

int dt_send_event (event_t * event)
{
	dt_server_mgt_t *mgt = &server_mgt;
	event_server_t *server_hub = mgt->server;
	if (!server_hub)
	{
		dt_error (TAG, "EVENT SEND FAILED \n");
		return -1;
	}
	dt_lock (&server_hub->event_lock);
	if (server_hub->event_count == 0)
		server_hub->event = event;
	else
	{
		event_t *entry = server_hub->event;
		while (entry->next)
			entry = entry->next;
		entry->next = event;

	}
	server_hub->event_count++;
	dt_unlock (&server_hub->event_lock);
	dt_debug (TAG, "EVENT:%d SEND OK, event count:%d \n", event->type, server_hub->event_count);
	return 0;
}

int dt_add_event (event_t * event, event_server_t * server)
{
	event_server_t *server_hub = server;
	if (!server_hub)
	{
		dt_error (TAG, "EVENT SEND FAILED \n");
		return -1;
	}
	dt_lock (&server_hub->event_lock);
	if (server_hub->event_count == 0)
		server_hub->event = event;
	else
	{
		event_t *entry = server_hub->event;
		while (entry->next)
			entry = entry->next;
		entry->next = event;

	}
	server_hub->event_count++;
	dt_unlock (&server_hub->event_lock);
	return 0;
}

event_t *dt_get_event (event_server_t * server)
{
	event_t *entry = NULL;
	dt_lock (&server->event_lock);
	if (server->event_count > 0)
	{
		entry = server->event;
		server->event = entry->next;
		server->event_count--;
		entry->next = NULL;
	}
	dt_unlock (&server->event_lock);
	if (entry != NULL)
		dt_info (TAG, "GET EVENT:%d From Server:%s \n", entry->type, server->name);
	return entry;
}

static int dt_transport_event (event_t * event, dt_server_mgt_t * mgt)
{
	dt_lock (&mgt->server_lock);
	event_server_t *entry = mgt->server;

	int ret = 0;
	while (entry->id != event->server_id)
	{
		entry = entry->next;
		if (!entry)
			break;
	}

	if (!entry)
	{
		dt_error (TAG, "Could not found server for:%d \n ", event->server_id);
		ret = -1;
		goto FAIL;
	}

	ret = dt_add_event (event, entry);
	if (ret < 0)				//we need to free this event
	{
		dt_error (TAG, "we can not transport this event, id:%d type:%d\n ", event->server_id, event->type);
		free (event);
		ret = -1;
		goto FAIL;
	}
	dt_unlock (&mgt->server_lock);
	return ret;
  FAIL:
	dt_unlock (&mgt->server_lock);
	return ret;
}

static void *event_transport_loop ()
{
	dt_server_mgt_t *mgt = &server_mgt;
	event_server_t *server_hub = mgt->server;
	event_t *event = NULL;
	do
	{
		if (mgt->exit_flag)
			goto QUIT;
		usleep (10000);
		event = dt_get_event (server_hub);
		if (!event)
			continue;
		dt_transport_event (event, mgt);
	}
	while (1);

  QUIT:
	dt_info (TAG, "EVENT_TRANSPORT_LOOP QUIT OK\n");
	pthread_exit (NULL);
	return NULL;
}
