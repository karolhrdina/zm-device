/*  =========================================================================
    zm_device - zm device actor

    Copyright (c) the Contributors as noted in the AUTHORS file.  This file is part
    of zmon.it, the fast and scalable monitoring system.                           
                                                                                   
    This Source Code Form is subject to the terms of the Mozilla Public License, v.
    2.0. If a copy of the MPL was not distributed with this file, You can obtain   
    one at http://mozilla.org/MPL/2.0/.                                            
    =========================================================================
*/

#ifndef ZM_ASSET_H_INCLUDED
#define ZM_ASSET_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif


//  @interface
//  Create new zm_device actor instance.
//  @TODO: Describe the purpose of this actor!
//
//      zactor_t *zm_device = zactor_new (zm_device, NULL);
//
//  Destroy zm_device instance.
//
//      zactor_destroy (&zm_device);
//
//  Enable verbose logging of commands and activity:
//
//      zstr_send (zm_device, "VERBOSE");
//
//  Start zm_device actor.
//
//      zstr_sendx (zm_device, "START", NULL);
//
//  Stop zm_device actor.
//
//      zstr_sendx (zm_device, "STOP", NULL);
//
//  This is the zm_device constructor as a zactor_fn;
ZM_ASSET_EXPORT void
    zm_device_actor (zsock_t *pipe, void *args);

//  Self test of this actor
ZM_ASSET_EXPORT void
    zm_device_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
