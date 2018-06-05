/*  =========================================================================
    proto_metric_unavailable - metric unavailable protocol send part
    Note: This file was manually amended, see below

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

#ifndef PROTO_METRIC_UNAVAILABLE_H_INCLUDED
#define PROTO_METRIC_UNAVAILABLE_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _proto_metric_unavailable_t proto_metric_unavailable_t;

//  @interface
//  Create a new proto_metric_unavailable
FTY_METRIC_TPOWER_EXPORT proto_metric_unavailable_t *
    proto_metric_unavailable_new (void);

//  Destroy the proto_metric_unavailable
FTY_METRIC_TPOWER_EXPORT void
    proto_metric_unavailable_destroy (proto_metric_unavailable_t **self_p);

//  Self test of this class
//  Note: Keep this definition in sync with fty_metric_tpower_classes.h
FTY_METRIC_TPOWER_PRIVATE void
    proto_metric_unavailable_test (bool verbose);

//  @end

#ifdef __cplusplus
}
#endif

#endif
