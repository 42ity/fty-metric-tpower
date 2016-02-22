/*  =========================================================================
    bios_agent_tpower_server - Actor generating new metrics

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

#ifndef BIOS_AGENT_TPOWER_SERVER_H_INCLUDED
#define BIOS_AGENT_TPOWER_SERVER_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

//  @interface
//  Create a new bios_agent_tpower_server
TPOWER_EXPORT bios_agent_tpower_server_t *
    bios_agent_tpower_server_new (void);

//  Destroy the bios_agent_tpower_server
TPOWER_EXPORT void
    bios_agent_tpower_server_destroy (bios_agent_tpower_server_t **self_p);

//  Self test of this class
TPOWER_EXPORT void
    bios_agent_tpower_server_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
