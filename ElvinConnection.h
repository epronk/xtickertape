/* $Id: ElvinConnection.h,v 1.8 1998/10/16 03:46:25 phelps Exp $ */

#ifndef ELVINCONNECTION_H
#define ELVINCONNECTION_H

typedef struct ElvinConnection_t *ElvinConnection;

#include "Subscription.h"
#include "Message.h"
#include "List.h"

typedef void *(*EventLoopRegisterInputFunc)(int fd, void *callback, void *rock);
typedef void (*EventLoopUnregisterInputFunc)(void *rock);
typedef void (*EventLoopRegisterTimerFunc)(unsigned long interval, void *callback, void *rock);

/* Answers a new ElvinConnection */
ElvinConnection ElvinConnection_alloc(
    char *hostname, int port, char *user,
    List subscriptions,
    Subscription errorSubscription);

/* Releases the resources used by the ElvinConnection */
void ElvinConnection_free(ElvinConnection self);

/* Registers the connection with an event loop */
void ElvinConnection_register(
    ElvinConnection self,
    EventLoopRegisterInputFunc registerFunc,
    EventLoopUnregisterInputFunc unregisterFunc,
    EventLoopRegisterTimerFunc registerTimerFunc);

/* Sends a message by posting an Elvin event */
void ElvinConnection_send(ElvinConnection self, Message message);

#endif /* ELVINCONNECTION_H */
