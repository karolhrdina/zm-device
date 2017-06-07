/*  =========================================================================
    zm_device - zm device actor

    Copyright (c) the Contributors as noted in the AUTHORS file.  This file is part
    of zmon.it, the fast and scalable monitoring system.                           
                                                                                   
    This Source Code Form is subject to the terms of the Mozilla Public License, v.
    2.0. If a copy of the MPL was not distributed with this file, You can obtain   
    one at http://mozilla.org/MPL/2.0/.                                            
    =========================================================================
*/

/*
@header
    zm_device - zm device actor
@discuss
@end
*/

#include "zm_device_classes.h"

//  Structure of our actor

struct _zm_device_t {
    zsock_t *pipe;              //  Actor command pipe
    zpoller_t *poller;          //  Socket poller
    bool terminated;            //  Did caller ask us to quit?
    bool verbose;               //  Verbose logging enabled?
    //  TODO: Declare properties
    zconfig_t *config;          //  Server configuration
    mlm_client_t *client;       //  Malamute client
    zhash_t *consumers;            // list of streams to subscribe
    zm_proto_t *msg;            //  Last received message
};


//  --------------------------------------------------------------------------
//  Create a new zm_device instance

static zm_device_t *
zm_device_new (zsock_t *pipe, void *args)
{
    zm_device_t *self = (zm_device_t *) zmalloc (sizeof (zm_device_t));
    assert (self);

    self->pipe = pipe;
    self->terminated = false;
    self->poller = zpoller_new (self->pipe, NULL);

    //  TODO: Initialize properties
    self->config = NULL;
    self->consumers = NULL;
    self->msg = NULL;
    self->client = mlm_client_new ();
    assert (self->client);
    zpoller_add (self->poller, mlm_client_msgpipe (self->client));

    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the zm_device instance

static void
zm_device_destroy (zm_device_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        zm_device_t *self = *self_p;

        //  TODO: Free actor properties
        zconfig_destroy (&self->config);
        zhash_destroy (&self->consumers);
        zm_proto_destroy (&self->msg);
        zpoller_remove (self->poller, self->client);
        mlm_client_destroy (&self->client);

        //  Free object itself
        zpoller_destroy (&self->poller);
        free (self);
        *self_p = NULL;
    }
}

static const char*
zm_device_cfg_endpoint (zm_device_t *self)
{
    assert (self);
    if (self->config) {
        return zconfig_resolve (self->config, "malamute/endpoint", NULL);
    }
    return NULL;
}

static const char*
zm_device_cfg_name (zm_device_t *self)
{
    assert (self);
    if (self->config) {
        return zconfig_resolve (self->config, "server/name", NULL);
    }
    return NULL;
}

static const char *
zm_device_cfg_producer (zm_device_t *self) {
    assert (self);
    if (self->config) {
        return zconfig_resolve (self->config, "malamute/producer", NULL);
    }
    return NULL;
}

static const char*
zm_device_cfg_consumer_first (zm_device_t *self) {
    assert (self);
    zhash_destroy (&self->consumers);
    self->consumers = zhash_new ();
    
    zconfig_t *cfg = zconfig_locate (self->config, "malamute/consumer");
    if (cfg) {
        zconfig_t *child = zconfig_child (cfg);
        while (child) {
            zhash_insert (self->consumers, zconfig_name (child), zconfig_value (child));
            child = zconfig_next (child);
        }
    }

    return (const char*) zhash_first (self->consumers);
}

static const char*
zm_device_cfg_consumer_next (zm_device_t *self) {
    assert (self);
    assert (self->consumers);
     return (const char*) zhash_next (self->consumers);
}

static const char*
zm_device_cfg_consumer_stream (zm_device_t *self) {
    assert (self);
    assert (self->consumers);
     return zhash_cursor (self->consumers);
}

static int
zm_device_connect_to_malamute (zm_device_t *self)
{
    if (!self->config) {
        zsys_warning ("zm-device: no CONFIGuration provided, there is nothing to do");
        return -1;
    }

    const char *endpoint = zm_device_cfg_endpoint (self);
    const char *name = zm_device_cfg_name (self);

    if (!self->client) {
        self->client = mlm_client_new ();
        zpoller_add (self->poller, mlm_client_msgpipe (self->client));
    }

    int r = mlm_client_connect (self->client, endpoint, 5000, name);
    if (r == -1) {
        zsys_warning ("Can't connect to malamute endpoint %", endpoint);
        return -1;
    }

    if (zm_device_cfg_producer (self)) {
        r = mlm_client_set_producer (self->client, zm_device_cfg_producer (self));
        if (r == -1) {
            zsys_warning ("Can't setup publisher on stream %", zm_device_cfg_producer (self));
            return -1;
        }
    }

    const char *pattern = zm_device_cfg_consumer_first (self);
    while (pattern) {
        const char *stream = zm_device_cfg_consumer_stream (self);
        r = mlm_client_set_consumer (self->client, stream, pattern);
        if (r == -1) {
            zsys_warning ("Can't setup consumer %s/%s", stream, pattern);
            return -1;
        }
        pattern = zm_device_cfg_consumer_next (self);
    }
    return 0;
}

//  Start this actor. Return a value greater or equal to zero if initialization
//  was successful. Otherwise -1.

static int
zm_device_start (zm_device_t *self)
{
    assert (self);

    int r = zm_device_connect_to_malamute (self);
    if (r == -1)
        return r;

    return 0;
}


//  Stop this actor. Return a value greater or equal to zero if stopping 
//  was successful. Otherwise -1.

static int
zm_device_stop (zm_device_t *self)
{
    assert (self);

    //  TODO: Add shutdown actions
    zpoller_remove (self->poller, self->client);
    mlm_client_destroy (&self->client);

    return 0;
}

//  Config message, second argument is string representation of config file
static int
zm_device_config (zm_device_t *self, zmsg_t *request)
{
    assert (self);
    assert (request);

    char *str_config = zmsg_popstr (request);
    if (str_config) {
        zconfig_t *foo = zconfig_str_load (str_config);
        if (foo) {
            zconfig_destroy (&self->config);
            self->config = foo;
        }
        else {
            zsys_warning ("zm_device: can't load config file from string");
            return -1;
        }
    }
    else
        return -1;
    return 0;
}


//  Here we handle incoming message from the node

static void
zm_device_recv_api (zm_device_t *self)
{
    //  Get the whole message of the pipe in one go
    zmsg_t *request = zmsg_recv (self->pipe);
    if (!request)
       return;        //  Interrupted

    char *command = zmsg_popstr (request);
    if (streq (command, "START"))
        zm_device_start (self);
    else
    if (streq (command, "STOP"))
        zm_device_stop (self);
    else
    if (streq (command, "VERBOSE"))
        self->verbose = true;
    else
    if (streq (command, "$TERM"))
        //  The $TERM command is send by zactor_destroy() method
        self->terminated = true;
    else
    if (streq (command, "CONFIG"))
        zm_device_config (self, request);
    else {
        zsys_error ("invalid command '%s'", command);
        assert (false);
    }
    zstr_free (&command);
    zmsg_destroy (&request);
}

static void
zm_device_recv_mlm_mailbox (zm_device_t *self)
{
    assert (self);
    zm_proto_print (self->msg);
}

static void
zm_device_recv_mlm_stream (zm_device_t *self)
{
    assert (self);
    zm_proto_print (self->msg);

    if (zm_proto_id (self->msg) != ZM_PROTO_DEVICE) {
        if (self->verbose)
            zsys_warning ("message from sender=%s, with subject=%s os not DEVICE",
            mlm_client_sender (self->client),
            mlm_client_subject (self->client));
        return;
    }
}

static void
zm_device_recv_mlm (zm_device_t *self)
{
    assert (self);
    zmsg_t *request = mlm_client_recv (self->client);
    int r = zm_proto_recv (self->msg, request);
    zmsg_destroy (&request);
    if (r != 0) {
        if (self->verbose)
            zsys_warning ("can't read message from sender=%s, with subject=%s",
            mlm_client_sender (self->client),
            mlm_client_subject (self->client));
        return;
    }

    if (streq (mlm_client_command (self->client), "MAILBOX DELIVER"))
        zm_device_recv_mlm_mailbox (self);
    else
    if (streq (mlm_client_command (self->client), "STREAM DELIVER"))
        zm_device_recv_mlm_stream (self);
}

//  --------------------------------------------------------------------------
//  This is the actor which runs in its own thread.

void
zm_device_actor (zsock_t *pipe, void *args)
{
    zm_device_t * self = zm_device_new (pipe, args);
    if (!self)
        return;          //  Interrupted

    //  Signal actor successfully initiated
    zsock_signal (self->pipe, 0);

    while (!self->terminated) {
        zsock_t *which = (zsock_t *) zpoller_wait (self->poller, 0);

        if (which == self->pipe)
            zm_device_recv_api (self);
        else
        if (which == mlm_client_msgpipe (self->client))
            zm_device_recv_mlm (self);
       //  Add other sockets when you need them.
    }
    zm_device_destroy (&self);
}

//  --------------------------------------------------------------------------
//  Self test of this actor.

void
zm_device_test (bool verbose)
{
    printf (" * zm_device: ");
    //  @selftest
    //  Simple create/destroy test
    // actor test
    zactor_t *zm_device = zactor_new (zm_device_actor, NULL);

    zactor_destroy (&zm_device);
    //  @end

    printf ("OK\n");
}
