/*
 * Copyright (C) 2015 Eaton
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
extern int agent_tpower_verbose;

#define zsys_debug1(...) \
    do { if (agent_tpower_verbose) zsys_debug (__VA_ARGS__); } while (0);

#include <set>
#include <functional>
#include "calc_power.h"
#include <tntdb/row.h>
#include <tntdb/result.h>

//=================================================================
//NOTE ACE: this is a copy paste functionality from assets part
//should be removed after it would be ported to the asset agent
//=================================================================
#define INPUT_POWER_CHAIN     1

//! Possible error types
enum errtypes {
    //! First error should be UNKNOWN as it maps to zero and zero is weird
    UNKNOWN_ERR,
    DB_ERR,
    BAD_INPUT,
    INTERNAL_ERR,
};

//! Constants for database errors
enum db_err_nos {
    //! First error should be UNKNOWN as it maps to zero and zero is weird
    DB_ERROR_UNKNOWN,
    DB_ERROR_INTERNAL,
    // Probably should be removed at some point and replaced with bad_input_err
    DB_ERROR_BADINPUT,
    DB_ERROR_NOTFOUND,
    DB_ERROR_NOTIMPLEMENTED,
    DB_ERROR_DBCORRUPTED,
    DB_ERROR_NOTHINGINSERTED,
    DB_ERROR_DELETEFAIL,
    DB_ERROR_CANTCONNECT,
};
// ----- table:  t_bios_asset_element_type ------------
// ----- column: id_asset_element_type  ---------------
// TODO tntdb can't manage uint8_t, so for now there is
// uint16_t
typedef uint16_t  a_elmnt_tp_id_t;
// ----- table:  t_bios_asset_element -----------------
// ----- column: id_asset_element ---------------------
typedef uint32_t a_elmnt_id_t;
// ----- table:  t_bios_asset_element_type ------------
// ----- column: id_asset_element_type  ---------------
// TODO tntdb can't manage uint8_t, so for now there is
// uint16_t
typedef uint16_t  a_elmnt_stp_id_t;
template <typename T>
inline db_reply<T> db_reply_new(T& item) {
    db_reply<T> a;

        a.status = 1;
        a.errtype = 0;
        a.errsubtype = 0;
        a.rowid = 0;
        a.affected_rows = 0;
        a.msg = "";
        a.addinfo = NULL;
        a.item = item;
    return a;
}
namespace persist {

enum asset_type {
    TUNKNOWN     = 0,
    GROUP       = 1,
    DATACENTER  = 2,
    ROOM        = 3,
    ROW         = 4,
    RACK        = 5,
    DEVICE      = 6
};

enum asset_subtype {
    SUNKNOWN = 0,
    UPS = 1,
    GENSET,
    EPDU,
    PDU,
    SERVER,
    FEED,
    STS,
    SWITCH,
    STORAGE,
    VIRTUAL,
    N_A = 11
    /* ATTENTION: don't change N_A id. It is used as default value in init.sql for types, that don't have N_A */
};

int
    select_assets_by_container
        (tntdb::Connection &conn,
         a_elmnt_id_t element_id,
         std::function<void(const tntdb::Row&)> cb,
         std::string status)
{
    zsys_debug1 ("container element_id = %" PRIu32, element_id);

    try {
        // Can return more than one row.
        tntdb::Statement st = conn.prepareCached(
            " SELECT "
            "   v.name, "
            "   v.id_asset_element as asset_id, "
            "   v.id_asset_device_type as subtype_id, "
            "   v.type_name as subtype_name, "
            "   v.id_type as type_id "
            " FROM "
            "   v_bios_asset_element_super_parent v "
            " WHERE "
            "   :containerid in (v.id_parent1, v.id_parent2, v.id_parent3, "
            "                    v.id_parent4, v.id_parent5, v.id_parent6, "
            "                    v.id_parent7, v.id_parent8, v.id_parent9, "
            "                    v.id_parent10)   AND                      "
            "                    v.status = :vstatus                       "
        );

        tntdb::Result result = st.set("containerid", element_id).
                                  set("vstatus", status).
                                  select();
        zsys_debug1("[v_bios_asset_element_super_parent]: were selected %" PRIu32 " rows",
                                                            result.size());
        for ( auto &row: result ) {
            cb(row);
        }
        return 0;
    }
    catch (const std::exception& e) {
        zsys_error ("Error: ",e.what());
        return -1;
    }
}


} // namespace end
// ----- table:  t_bios_asset_link_type ---------------
// ----- column: id_asset_link_type -------------------
typedef uint8_t  a_lnk_tp_id_t;

db_reply <std::set <std::pair<a_elmnt_id_t ,a_elmnt_id_t>>>
    select_links_by_container
        (tntdb::Connection &conn,
         a_elmnt_id_t element_id)
{
    zsys_debug1 ("  links are selected for element_id = %" PRIi32, element_id);
    a_lnk_tp_id_t linktype = INPUT_POWER_CHAIN;

    //      all powerlinks are included into "resultpowers"
    std::set <std::pair<a_elmnt_id_t ,a_elmnt_id_t>> item{};
    db_reply <std::set<std::pair<a_elmnt_id_t ,a_elmnt_id_t>>> ret = db_reply_new(item);

    try{
        // v_bios_asset_link are only devices,
        // so there is no need to add more constrains
        tntdb::Statement st = conn.prepareCached(
            " SELECT"
            "   v.id_asset_element_src,"
            "   v.id_asset_element_dest"
            " FROM"
            "   v_bios_asset_link AS v,"
            "   v_bios_asset_element_super_parent AS v1,"
            "   v_bios_asset_element_super_parent AS v2"
            " WHERE"
            "   v.id_asset_link_type = :linktypeid AND"
            "   v.id_asset_element_dest = v2.id_asset_element AND"
            "   v.id_asset_element_src = v1.id_asset_element AND"
            "   ("
            "       ( :containerid IN (v2.id_parent1, v2.id_parent2 ,v2.id_parent3,"
            "                          v2.id_parent4, v2.id_parent5, v2.id_parent6,"
            "                          v2.id_parent7, v2.id_parent8, v2.id_parent9,"
            "                          v2.id_parent10) ) OR"
            "       ( :containerid IN (v1.id_parent1, v1.id_parent2 ,v1.id_parent3,"
            "                          v1.id_parent4, v1.id_parent5, v1.id_parent6,"
            "                          v1.id_parent7, v1.id_parent8, v1.id_parent9,"
            "                          v1.id_parent10) )"
            "   )"
        );

        // can return more than one row
        tntdb::Result result = st.set("containerid", element_id).
                                  set("linktypeid", linktype).
                                  select();
        zsys_debug1("[t_bios_asset_link]: were selected %" PRIu32 " rows",
                                                         result.size());

        // Go through the selected links
        for ( auto &row: result )
        {
            // id_asset_element_src, required
            a_elmnt_id_t id_asset_element_src = 0;
            row[0].get(id_asset_element_src);
            assert ( id_asset_element_src );

            // id_asset_element_dest, required
            a_elmnt_id_t id_asset_element_dest = 0;
            row[1].get(id_asset_element_dest);
            assert ( id_asset_element_dest );

            ret.item.insert(std::pair<a_elmnt_id_t ,a_elmnt_id_t>(id_asset_element_src, id_asset_element_dest));
        } // end for
        ret.status = 1;
        return ret;
    }
    catch (const std::exception &e) {
        ret.status     = 0;
        ret.errtype    = DB_ERR;
        ret.errsubtype = DB_ERROR_INTERNAL;
        ret.msg        = e.what();
        zsys_error (e.what());
        return ret;
    }
}

// ----- table:  t_bios_asset_element -----------------
// ----- column: priority -----------------------------
typedef uint16_t a_elmnt_pr_t;

/**
 * \brief helper structure for results of v_bios_asset_element
 */
struct db_a_elmnt_t {
    a_elmnt_id_t     id;
    std::string      name;
    std::string      status;
    a_elmnt_id_t     parent_id;
    a_elmnt_pr_t     priority;
    a_elmnt_tp_id_t  type_id;
    a_elmnt_stp_id_t subtype_id;
    std::string      asset_tag;
    std::map <std::string, std::string> ext;

    db_a_elmnt_t () :
        id{},
        name{},
        status{},
        parent_id{},
        priority{},
        type_id{},
        subtype_id{},
        asset_tag{},
        ext{}
    {}

    db_a_elmnt_t (
        a_elmnt_id_t     id,
        std::string      name,
        std::string      status,
        a_elmnt_id_t     parent_id,
        a_elmnt_pr_t     priority,
        a_elmnt_tp_id_t  type_id,
        a_elmnt_stp_id_t subtype_id,
        std::string      asset_tag) :

        id(id),
        name(name),
        status(status),
        parent_id(parent_id),
        priority(priority),
        type_id(type_id),
        subtype_id(subtype_id),
        asset_tag(asset_tag),
        ext{}
    {}
};

db_reply <std::vector<db_a_elmnt_t>>
    select_asset_elements_by_type
        (tntdb::Connection &conn,
         a_elmnt_tp_id_t type_id)
{

    std::vector<db_a_elmnt_t> item{};
    db_reply <std::vector<db_a_elmnt_t>> ret = db_reply_new(item);

    try{
        // Can return more than one row.
        tntdb::Statement st = conn.prepareCached(
            " SELECT"
            "   v.name , v.id_parent, v.status, v.priority, v.id, v.id_subtype"
            " FROM"
            "   v_bios_asset_element v"
            " WHERE v.id_type = :typeid"
        );

        tntdb::Result result = st.set("typeid", type_id).
                                  select();
        zsys_debug1("[v_bios_asset_element]: were selected %" PRIu32 " rows",
                                                            result.size());

        // Go through the selected elements
        for ( auto &row: result )
        {
            db_a_elmnt_t m{0,"","",0,5,0,0,""};

            row[0].get(m.name);
            assert ( !m.name.empty() );  // database is corrupted

            row[1].get(m.parent_id);
            row[2].get(m.status);
            row[3].get(m.priority);
            row[4].get(m.id);
            row[5].get(m.subtype_id);

            ret.item.push_back(m);
        }
        ret.status = 1;
        return ret;
    }
    catch (const std::exception &e) {
        ret.status        = 0;
        ret.errtype       = DB_ERR;
        ret.errsubtype    = DB_ERROR_INTERNAL;
        ret.msg           = e.what();
        ret.item.clear();
        zsys_error(e.what());
        return ret;
    }
}

//=================================================================
//NOTE ACE: this is the end of copy paste functionality
//=================================================================

bool is_epdu (const device_info_t &device)
{
    if ( std::get<3>(device) == persist::asset_subtype::EPDU )
        return true;
    else
        return false;
}


bool is_pdu (const device_info_t &device)
{
    if ( std::get<3>(device) ==  persist::asset_subtype::PDU )
        return true;
    else
        return false;
}


bool is_ups (const device_info_t &device)
{
    if ( std::get<3>(device) ==  persist::asset_subtype::UPS )
        return true;
    else
        return false;
}


/**
 *  \brief From set of links derives set of elements that at least once were
 *  a dest device in a link.
 *  link is: from to
 */
static std::set<a_elmnt_id_t>
    find_dests
        (const std::set <std::pair<a_elmnt_id_t, a_elmnt_id_t> > &links,
         a_elmnt_id_t element_id)
{
    std::set<a_elmnt_id_t> dests;

    for ( auto &one_link: links )
    {
        if ( std::get<0>(one_link) == element_id )
            dests.insert(std::get<1>(one_link));
    }
    return dests;
}


/**
 *  \brief An implementation of looking for a "border" devices
 *
 *  A border device is a device, that should be taken as a power
 *  source device at first turn
 */
static void
    update_border_devices
        (const std::map <a_elmnt_id_t, device_info_t> &container_devices,
         const std::set <std::pair<a_elmnt_id_t, a_elmnt_id_t> > &links,
         std::set <device_info_t> &border_devices)
{
    std::set<device_info_t> new_border_devices;
    for ( auto &border_device: border_devices )
    {
        auto adevice_dests = find_dests (links, std::get<0>(border_device));
        for ( auto &adevice: adevice_dests )
        {
            auto it = container_devices.find(adevice);
            if ( it != container_devices.cend() )
                new_border_devices.insert(it->second);
            else
            {
                zsys_error ("DB can be in inconsistant state or some device "
                        "has power source in the other container");
                zsys_error ("device(as element) %" PRIu32 " is not in container",
                                                adevice);
                // do nothing in this case
            }
        }
    }
    border_devices.clear();
    border_devices.insert(new_border_devices.begin(),
                          new_border_devices.end());
}


bool
    is_powering_other_rack (
        const device_info_t &border_device,
        const std::map <a_elmnt_id_t, device_info_t> &devices_in_container,
        const std::set <std::pair<a_elmnt_id_t, a_elmnt_id_t> > &links)
{
    auto adevice_dests = find_dests (links, std::get<0>(border_device));
    for ( auto &adevice: adevice_dests )
    {
        auto it = devices_in_container.find(adevice);
        if ( it == devices_in_container.cend() ) {
            // it means, that destination device is out of the container
            return true;
        }
    }
    return false;
}


/**
 *  \brief An implementation of the algorithm.
 *
 *  Take a first "smart" device in every powerchain thatis closest to "main"
 *  If device is not smart, try to look at upper level. Repeat until
 *  chain ends or until all chains are processed
 */
static std::vector<std::string>
    compute_total_power_v2(
        const std::map <a_elmnt_id_t, device_info_t> &devices_in_container,
        const std::set <std::pair<a_elmnt_id_t, a_elmnt_id_t> > &links,
        std::set <device_info_t> border_devices)
{
    std::vector <std::string> dvc{};

    if ( border_devices.empty() )
        return dvc;
    // it is not a good idea to delete from collection while iterating it
    std::set <device_info_t> todelete{};
    while ( !border_devices.empty() )
    {
        for ( auto &border_device: border_devices )
        {
            if ( ( is_epdu(border_device) ) ||
                 ( is_ups(border_device) &&  ( !is_powering_other_rack (border_device, devices_in_container, links) ) ) )
            {
                dvc.push_back(std::get<1>(border_device));
                // remove from border
                todelete.insert(border_device);
                continue;
            }
            // NOT IMPLEMENTED
            //if ( is_it_device(border_device) )
            //{
            //    // remove from border
            //    // add to ipmi
            //}
        }
        for (auto &todel: todelete)
            border_devices.erase(todel);
        update_border_devices(devices_in_container, links, border_devices);
    }
    return dvc;
}


/**
 *  \brief For every container returns a list of its power sources
 */
static db_reply <std::map<std::string, std::vector<std::string> > >
    select_devices_total_power_container
        (tntdb::Connection  &conn,
         int8_t container_type_id)
{
    zsys_debug1 ("  container_type_id = %" PRIi8, container_type_id);
    // name of the container is mapped onto the vector of names of its power sources
    std::map<std::string, std::vector<std::string> > item{};
    db_reply <std::map<std::string, std::vector<std::string> > > ret =
                db_reply_new(item);

    // there is no need to do all in one select, so let's do it by steps
    // select all containers by type
    auto allContainers = select_asset_elements_by_type
                                    (conn, container_type_id);

    if  ( allContainers.status == 0 )
    {
        ret.status     = 0;
        ret.msg        = allContainers.msg;
        ret.errtype    = allContainers.errtype;
        ret.errsubtype = allContainers.errsubtype;
        zsys_error ("some error appears, during selecting the containers");
        return ret;
    }
    // if there is no containers, then it is an error
    if  ( allContainers.item.empty() )
    {
        ret.status     = 0;
        ret.msg        = "there is no containers of requested type";
        ret.errtype    = DB_ERR;
        ret.errsubtype = DB_ERROR_NOTFOUND;
        zsys_warning (ret.msg.c_str());
        return ret;
    }

    // go through every container and "compute" what should be summed up
    for ( auto &container : allContainers.item )
    {
        // select all devices in the container
        std::map <a_elmnt_id_t, device_info_t> container_devices{};
        std::function<void(const tntdb::Row&)> func = \
            [&container_devices](const tntdb::Row& row)
            {
                a_elmnt_tp_id_t type_id = 0;
                row["type_id"].get(type_id);

                if ( type_id == persist::asset_type::DEVICE )
                {
                    std::string device_name = "";
                    row["name"].get(device_name);

                    a_elmnt_id_t asset_id = 0;
                    row["asset_id"].get(asset_id);

                    a_elmnt_stp_id_t device_type_id = 0;
                    row["subtype_id"].get(device_type_id);

                    std::string device_type_name = "";
                    row["subtype_name"].get(device_type_name);

                    container_devices.emplace (asset_id,
                        std::make_tuple(asset_id, device_name,
                                        device_type_name, device_type_id));
                }
            };


        auto rv = persist::select_assets_by_container
            (conn, container.id, func, "active");

        // here would be placed names of devices to summ up
        std::vector<std::string> result(0);
        if ( rv != 0 )
        {
            zsys_warning ("'%s': problems appeared in selecting devices",
                                                    container.name.c_str());
            // so return an empty set of power devices
            ret.item.insert(std::pair< std::string, std::vector<std::string> >
                                                    (container.name, result));
            continue;
        }
        if ( container_devices.empty() )
        {
            zsys_warning ("'%s': has no devices", container.name.c_str());
            // so return an empty set of power devices
            ret.item.insert(std::pair< std::string, std::vector<std::string> >
                                                    (container.name, result));
            continue;
        }

        auto links = select_links_by_container (conn, container.id);
        if ( links.status == 0 )
        {
            zsys_warning ("'%s': internal problems in links detecting",
                                                    container.name.c_str());
            // so return an empty set of power devices
            ret.item.insert(std::pair< std::string, std::vector<std::string> >
                                                    (container.name, result));
            continue;
        }

        if ( links.item.empty() )
        {
            zsys_warning ("'%s': has no power links", container.name.c_str());
            // so return an empty set of power devices
            ret.item.insert(std::pair< std::string, std::vector<std::string> >
                                                    (container.name, result));
            continue;
        }

        // the set of all border devices ("starting points")
        std::set <device_info_t> border_devices;
        // the set of all destination devices in selected links
        std::set <a_elmnt_id_t> dest_dvcs{};
        //  from (first)   to (second)
        //           +--------------+
        //  B________|______A__C    |
        //           |              |
        //           +--------------+
        //   B is out of the Container
        //   A is in the Container
        //   then A is border device
        for ( auto &oneLink : links.item )
        {
            zsys_debug1 ("  cur_link: %d->%d", oneLink.first, oneLink.second);
            auto it = container_devices.find (oneLink.first);
            if ( it == container_devices.end() )
                // if in the link first point is out of the Container,
                // the second definitely should be in Container,
                // otherwise it is not a "container"-link
            {
                border_devices.insert(
                            container_devices.find(oneLink.second)->second);
            }
            dest_dvcs.insert(oneLink.second);
        }
        //  from (first)   to (second)
        //           +-----------+
        //           |A_____C    |
        //           |           |
        //           +-----------+
        //   A is in the Container (from)
        //   C is in the Container (to)
        //   then A is border device
        //
        //   Algorithm: from all devices in the Container we will
        //   select only those that don't have an incoming links
        //   (they are not a destination device for any link)
        for ( auto &oneDevice : container_devices )
        {
            if ( dest_dvcs.find (oneDevice.first) == dest_dvcs.end() )
                border_devices.insert ( oneDevice.second );
        }

        result = compute_total_power_v2(container_devices, links.item,
                                        border_devices);
        ret.item.insert(std::pair< std::string, std::vector<std::string> >
                                                (container.name, result));
    }
    return ret;
}


db_reply <std::map<std::string, std::vector<std::string> > >
    select_devices_total_power_dcs
        (tntdb::Connection  &conn)
{
    return select_devices_total_power_container (conn, persist::asset_type::DATACENTER);
}


db_reply <std::map<std::string, std::vector<std::string> > >
    select_devices_total_power_racks
        (tntdb::Connection  &conn)
{
    return select_devices_total_power_container (conn, persist::asset_type::RACK);
}

void calc_power_test(bool)
{
    assert(true);
}
