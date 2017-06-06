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
    self->config = zconfig_new ("root", NULL);

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

        //  Free object itself
        zpoller_destroy (&self->poller);
        free (self);
        *self_p = NULL;
    }
}


//  Start this actor. Return a value greater or equal to zero if initialization
//  was successful. Otherwise -1.

static int
zm_device_start (zm_device_t *self)
{
    assert (self);

    //  TODO: Add startup actions

    return 0;
}


//  Stop this actor. Return a value greater or equal to zero if stopping 
//  was successful. Otherwise -1.

static int
zm_device_stop (zm_device_t *self)
{
    assert (self);

    //  TODO: Add shutdown actions

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
        zconfig_t *foo = zconfig_load (str_config);
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
    // Note: If your selftest reads SCMed fixture data, please keep it in
    // src/selftest-ro; if your test creates filesystem objects, please
    // do so under src/selftest-rw. They are defined below along with a
    // usecase for the variables (assert) to make compilers happy.
    const char *SELFTEST_DIR_RO = "src/selftest-ro";
    const char *SELFTEST_DIR_RW = "src/selftest-rw";
    assert (SELFTEST_DIR_RO);
    assert (SELFTEST_DIR_RW);
    // Uncomment these to use C++ strings in C++ selftest code:
    //std::string str_SELFTEST_DIR_RO = std::string(SELFTEST_DIR_RO);
    //std::string str_SELFTEST_DIR_RW = std::string(SELFTEST_DIR_RW);
    //assert ( (str_SELFTEST_DIR_RO != "") );
    //assert ( (str_SELFTEST_DIR_RW != "") );
    // NOTE that for "char*" context you need (str_SELFTEST_DIR_RO + "/myfilename").c_str()

    zactor_t *zm_device = zactor_new (zm_device_actor, NULL);

    zactor_destroy (&zm_device);
    //  @end

    printf ("OK\n");
}