/*  =========================================================================
    fty_metric_tpower_server - Actor generating new metrics

    Copyright (C) 2014 - 2015 Eaton                                        
                                                                           
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

#ifndef FTY_METRIC_TPOWER_SERVER_H_INCLUDED
#define FTY_METRIC_TPOWER_SERVER_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
//  Metric tpower server actor
FTY_METRIC_TPOWER_EXPORT void
fty_metric_tpower_server (zsock_t *pipe, void* args);

//  Self test of this class
FTY_METRIC_TPOWER_EXPORT void
fty_metric_tpower_server_test (bool verbose);

//  @end

#ifdef __cplusplus
}
#endif

#endif
