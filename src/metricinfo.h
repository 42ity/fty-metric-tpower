/*  =========================================================================
    metricinfo - Measurement

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
/*! \file   metricinfo.h
 *  \author Alena Chernikava <AlenaChernikava@Eaton.com>
 *  \brief  This class is intended to handle one metric
 */

#ifndef SRC_METRICINFO_H_
#define SRC_METRICINFO_H_

#include <string>
#include <ctime>


class MetricInfo {

public:
    std::string generateTopic(void) const {
        return _source + "@" + _element_name;
    };

    MetricInfo() {
        _timestamp = 0;
        _ltl = 5 * 60;
    };

    MetricInfo (
        const std::string &element_name,
        const std::string &source,
        const std::string &units,
        double value,
        int64_t timestamp,
        const std::string &destination
        ):
        _element_name (element_name),
        _source (source),
        _units (units),
        _value (value),
        _element_destination_name (destination),
        _ltl (5*60)
    {
        // we need to have a precise value of "now"
        if ( timestamp != -1 ) {
            _timestamp = timestamp;
        }
        else {
            _timestamp = ::time(NULL);
        }

    };



    double getValue (void) const{
        return _value;
    };

    std::string getElementName (void) const{
        return _element_name;
    };

    int64_t getTimestamp (void) const {
        return _timestamp;
    };

    bool isUnknown() const {
        if ( _element_name.empty() ||
             _source.empty() ||
             _units.empty() ) {
            return true;
        }
        return false;
    };


    // new function!!!!
    int32_t getLtl(void) {
        return _ltl;
    }
    std::string getUnits(void) const
    { return _units; };
    std::string getSource (void) const {
        return _source;
    };
    void setTime() { _timestamp = std::time(NULL); };
    void setUnits(const std::string &U) { _units = U; };
    friend inline bool operator==( const MetricInfo &lhs, const MetricInfo &rhs );
    // new function!!!!
    friend inline bool operator!=( const MetricInfo &lhs, const MetricInfo &rhs );

    // This class is very close to metric info
    // So let it use fields directly
    friend class MetricList;

private:
    std::string _element_name;
    std::string _source;
    std::string _units;
    double      _value;
    int64_t     _timestamp;
    std::string _element_destination_name;

    // life tme limit [s]
    int32_t _ltl;

};

// new function!!!!
inline bool operator==( const MetricInfo &lhs, const MetricInfo &rhs ) {
    return ( lhs._units == rhs._units &&
             lhs._value == rhs._value
           );
}

// new function!!!!
inline bool operator!=( const MetricInfo &lhs, const MetricInfo &rhs ) {
    return ! ( lhs == rhs );
}

#endif // SRC_METRICINFO_H_
