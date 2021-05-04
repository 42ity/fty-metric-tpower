/*  =========================================================================
    fty_metric_tpower_classes - private header file

    Copyright (C) 2014 - 2020 Eaton

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

#ifndef FTY_METRIC_TPOWER_CLASSES_H_INCLUDED
#define FTY_METRIC_TPOWER_CLASSES_H_INCLUDED

//  Platform definitions, must come first

//  External API
#include "../include/fty_metric_tpower.h"

//  Extra headers

//  Internal API

#include "metricinfo.h"
#include "calc_power.h"
#include "tpowerconfiguration.h"
#include "metriclist.h"
#include "tp_unit.h"
#include "watchdog.h"

//  *** To avoid double-definitions, only define if building without draft ***
#ifndef FTY_METRIC_TPOWER_BUILD_DRAFT_API

//  Self test of this class.
void metricinfo_test (bool verbose);

//  Self test of this class.
void calc_power_test (bool verbose);

//  Self test of this class.
void tpowerconfiguration_test (bool verbose);

//  Self test of this class.
void metriclist_test (bool verbose);

//  Self test of this class.
void tp_unit_test (bool verbose);

//  Self test for private classes
void fty_metric_tpower_private_selftest (bool verbose, const char *subtest);

#endif // FTY_METRIC_TPOWER_BUILD_DRAFT_API

#endif
