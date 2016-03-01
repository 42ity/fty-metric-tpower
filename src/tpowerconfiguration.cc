/*  =========================================================================
    tpowerconfiguration - Configuration

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

/*
@header
    tpowerconfiguration - Configuration
@discuss
@end
*/

#include "tpower_classes.h"
#include <stdio.h>
#include <iostream>
#include <string>
#include <exception>
#include <errno.h>
#include <tntdb/connect.h>

#include <stdlib.h>

static std::string url = std::string("mysql:db=box_utf8;user=") +
                  ((getenv("DB_USER")   == NULL) ? "root" : getenv("DB_USER")) +
                  ((getenv("DB_PASSWD") == NULL) ? ""     : 
                      std::string(";password=") + getenv("DB_PASSWD"));

bool TotalPowerConfiguration::
    configure(void)
{
    zsys_info ("loading power topology");
    try {
        // remove old topology
        _racks.clear();
        _affectedRacks.clear();
        _DCs.clear();
        _affectedDCs.clear();
        
        // connect to the database
        tntdb::Connection connection = tntdb::connectCached(url);
        // reading racks
        auto ret = select_devices_total_power_racks (connection);

        if( ret.status ) {
            for( auto & rack_it: ret.item ) {
                zsys_debug("reading rack %s powerdevices", rack_it.first.c_str() );
                auto devices = rack_it.second;
                for( auto device_it: devices ) {
                    zsys_debug("rack %s powerdevice %s", rack_it.first.c_str(), device_it.c_str() );
                    addDeviceToMap(_racks, _affectedRacks, rack_it.first, device_it );
                }
            }
        }
        // reading DCs
        ret = select_devices_total_power_dcs (connection);
        if( ret.status ) {
            for( auto & dc_it: ret.item ) {
                zsys_debug("reading DC %s powerdevices", dc_it.first.c_str() );
                auto devices = dc_it.second;
                for( auto device_it: devices ) {
                    zsys_debug("DC %s powerdevice %s", dc_it.first.c_str(), device_it.c_str() );
                    addDeviceToMap(_DCs, _affectedDCs, dc_it.first, device_it );
                }
            }
        }
        connection.close();
        _reconfigPending = 0;
        return true;
    } catch (const std::exception& e) {
        zsys_error("Failed to read configuration from database. Excepton caught: '%s'.", e.what ());
        _reconfigPending = time(NULL) + 60;
        return false;
    } catch(...) {
        zsys_error ("Failed to read configuration from database. Unknown exception caught.");
        _reconfigPending = time(NULL) + 60;
        return false;
    }
}

void TotalPowerConfiguration::addDeviceToMap(
    std::map< std::string, TPUnit > &elements,
    std::map< std::string, std::string > &reverseMap,
    const std::string & owner,
    const std::string & device )
{
    auto element = elements.find(owner);
    if( element == elements.end() ) {
        auto box = TPUnit();
        box.name(owner);
        box.addPowerDevice(device);
        elements[owner] = box;
    } else {
        element->second.addPowerDevice(device);
    }
    reverseMap[device] = owner;
}


void TotalPowerConfiguration::processAsset( const std::string &topic)
{
    // something is beeing reconfigured, let things to settle down
    if( _reconfigPending == 0 ) {
        zsys_info("Reconfiguration scheduled");
        _reconfigPending = ::time(NULL) + 60; // in 60[s]
    }
    _timeout = getPollInterval();
}

std::vector<MetricInfo> TotalPowerConfiguration::
    processMetric (
        const MetricInfo &M,
        const std::string &topic)
{
    // ASSUMTION: one device can affect only one ASSET of each type ( Datacenter or Rack )
    if( _rackRegex.match(topic) ) {
        auto affected_it = _affectedRacks.find( M.getElementName() );
        if( affected_it != _affectedRacks.end() ) {
            // this device affects some total rack power
            zsys_debug("measurement is interesting for rack %s", affected_it->second.c_str() );
            auto rack_it = _racks.find( affected_it->second );
            if( rack_it != _racks.end() ) {
                // affected rack found
                rack_it->second.setMeasurement(M);
                sendMeasurement( _racks, _rackQuantities );
            }
        }
    }
    if( _dcRegex.match(topic) ) {
        auto affected_it = _affectedDCs.find( M.getElementName() );
        if( affected_it != _affectedDCs.end() ) {
            // this device affects some total DC power
            zsys_debug("measurement is interesting for DC %s", affected_it->second.c_str() );
            auto dc_it = _DCs.find( affected_it->second );
            if( dc_it != _DCs.end() ) {
                // affected dc found
                dc_it->second.setMeasurement(M);
                sendMeasurement( _DCs, _dcQuantities );
            }
        }
    }
    _timeout = getPollInterval();
    // TODO return right metric
    return {M};
}

int send( const char *subject, zmsg_t **msg_p )
{
    // TODO send it!
    return 0;
}


void  TotalPowerConfiguration::sendMeasurement(std::map< std::string, TPUnit > &elements, const std::vector<std::string> &quantities ) {
    for( auto &element : elements ) {
        element.second.calculate( quantities );
        for( auto &q : quantities ) {
            if( element.second.advertise(q) ) {
                try {
                    zmsg_t *message = element.second.measurementMessage(q);
                    if( message ) {
                        std::string topic = "measurement." + q + "@" + element.second.name();
                        MetricInfo M = element.second.getMetricInfo(q);
                        zsys_debug("Sending total power topic: %s value: %lf",
                                  topic.c_str(),
                                  M.getValue());
                        send( topic.c_str(), &message );
                        element.second.advertised(q);
                    }
                } catch(...) { };
            } else {
                // log something from time to time if device calculation is unknown
                auto devices = element.second.devicesInUnknownState(q);
                if( ! devices.empty() ) {
                    std::string devicesText;
                    for( auto &it: devices ) {
                        devicesText += it + " ";
                    }
                    zsys_info("Devices preventing total %s calculation for %s are: %s",
                             q.c_str(),
                             element.first.c_str(),
                             devicesText.c_str() );
                }
            }
        }
    }
}

time_t TotalPowerConfiguration::getPollInterval() {
    time_t T = TPOWER_MEASUREMENT_REPEAT_AFTER;
    for( auto &rack_it : _racks ) {
        for( auto &q : _rackQuantities ) {
            time_t Tx = rack_it.second.timeToAdvertisement(q);
            if( Tx > 0 && Tx < T ) T = Tx;
        }
    }
    for( auto &dc_it : _racks ) {
        for( auto &q : _dcQuantities ) {
            time_t Tx = dc_it.second.timeToAdvertisement(q);
            if( Tx > 0 && Tx < T ) T = Tx;
        }
    }
    if( _reconfigPending ) {
        time_t Tx = _reconfigPending - time(NULL) + 1;
        if( Tx <= 0 ) Tx = 1;
        if( Tx < T ) T = Tx;
    }
    return T * 1000;
}


void TotalPowerConfiguration::onPoll() {
    sendMeasurement( _racks, _rackQuantities );
    sendMeasurement( _DCs, _dcQuantities );
    if( _reconfigPending && ( _reconfigPending <= time(NULL) ) ) configure();
    _timeout = getPollInterval();
}


//  --------------------------------------------------------------------------
//  Self test of this class

void
tpowerconfiguration_test (bool verbose)
{
    printf (" * tpowerconfiguration: ");
    printf ("OK\n");
}
