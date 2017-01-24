/*  =========================================================================
    proto_metric_unavailable - metric unavailable protocol send part

    Copyright (C) 2014 - 2017 Eaton                                        
                                                                           
    This program is free software; you can redistribute it and/or modify   
    it under the terms of the GNU General Public License as published by   
    the Free Software Foundation; either version 2 of the License, or      
    (at your option) any later version.                                    
                                                                           
    This program is distributed in the hope that it will be useful,        
    but WITHOUT ANY WARRANTY; without even the implied warranty of         
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          
    GNU General Public License for more details.                           
                                                                           
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.            
    =========================================================================
*/

/*
@header
    proto_metric_unavailable - metric unavailable protocol send part
@discuss
@end
*/

#include "fty_metric_tpower_classes.h"

//  Structure of our class

struct _proto_metric_unavailable_t {
    int filler;     //  Declare class properties here
};


//  --------------------------------------------------------------------------
//  Create a new proto_metric_unavailable

proto_metric_unavailable_t *
proto_metric_unavailable_new (void)
{
    proto_metric_unavailable_t *self = (proto_metric_unavailable_t *) zmalloc (sizeof (proto_metric_unavailable_t));
    assert (self);
    //  Initialize class properties here
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the proto_metric_unavailable

void
proto_metric_unavailable_destroy (proto_metric_unavailable_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        proto_metric_unavailable_t *self = *self_p;
        //  Free class properties here
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}

//  --------------------------------------------------------------------------
//  Self test of this class

void
proto_metric_unavailable_test (bool verbose)
{
    printf (" * proto_metric_unavailable: ");

    //  @selftest
    //  Simple create/destroy test
    proto_metric_unavailable_t *self = proto_metric_unavailable_new ();
    assert (self);
    proto_metric_unavailable_destroy (&self);
    //  @end
    printf ("OK\n");
}
