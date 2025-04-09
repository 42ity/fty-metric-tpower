/// Copyright (C) 2015 - 2020 Eaton
///
/// This program is free software; you can redistribute it and/or modify
/// it under the terms of the GNU General Public License as published by
/// the Free Software Foundation; either version 2 of the License, or
/// (at your option) any later version.
///
/// This program is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
/// GNU General Public License for more details.
///
/// You should have received a copy of the GNU General Public License along
/// with this program; if not, write to the Free Software Foundation, Inc.,
/// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

/// @file   calc_power.h
/// @brief  Functions for calculating a total rack and DC power from database values.
/// @author Alena Chernikava <AlenaChernikava@Eaton.com>
/// @author Michal Vyskocil  <MichalVyskocil@Eaton.com>

#pragma once

#include <fty_common_db.h>
#include <tntdb/connect.h>
#include <map>
#include <vector>
#include <string>

// ===========================================================================
// Functions that find power sources
// ===========================================================================

/// For every rack analyses its power topology and for each rack returns a list of power devices that belong to "input
/// power".
///
/// Because this function is intent to support an agent (but agent has no idea about ids) that listens to the stream, so
/// an output is supposed to be names of devices.
///
/// \param conn - a connection to the database
///
/// \return in case of success: status = 1,
///                             item is set to be a map of rack names
///                                  onto its power sources
///         in case of fail:    status = 0,
///                             errtype is set,
///                             errsubtype is set,
///                             msg is set
db_reply<std::map<std::string, std::vector<std::string>>> select_devices_total_power_racks(tntdb::Connection& conn);

/// For every dc analyses its power topology and for each dc returns a list of power devices that belong to "input
/// power".
///
/// Because this function is intent to support an agent (but agent has no idea about ids) that listens to the stream, so
/// an output is supposed to be names of devices.
///
/// @param conn - a connection to the database
///
/// @return in case of success: status = 1,
///                             item is set to be a map of dc names
///                                  onto its power sources
///         in case of fail:    status = 0,
///                             errtype is set,
///                             errsubtype is set,
///                             msg is set
db_reply<std::map<std::string, std::vector<std::string>>> select_devices_total_power_dcs(tntdb::Connection& conn);
