//! robotkernel service bridge links-and-nodes
/*!
 * author: Robert Burger <robert.burger@dlr.de>
 */

/*
 * This file is part of bridge_cli.
 *
 * bridge_cli is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * bridge_cli is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with bridge_cli; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef LN_BRIDGE_H
#define LN_BRIDGE_H

#include "robotkernel/robotkernel.h"
#include "robotkernel/service.h"
#include "robotkernel/runnable.h"

#include "ln/ln.h"
#include "ln/cppwrapper.h"
#include "robotkernel/bridge_base.h"

#include <yaml-cpp/yaml.h>


namespace ln_bridge {

class service;

class client : 
    public std::enable_shared_from_this<client>,
    public robotkernel::bridge_base, 
    public robotkernel::runnable {
    public:
        //! construct ln_bridge client
        client(const char*& bridgename, YAML::Node& node);

        //! destruct ln_bridge client
        ~client();

        //! init method
        void init();

        //! create and register ln service
        /*!
         * \param svc robotkernel service struct
         */
        void add_service(const robotkernel::service_t& svc);

        //! unregister and remove ln service 
        /*!
         * \param svc robotkernel service struct
         */
        void remove_service(const robotkernel::service_t& svc);
        
        //!< handler function called if thread is running
        void run();

    public:
        //! links-and-nodes client handle
        ln::client *clnt;

        //! links-and-nodes services map
        typedef std::map<std::pair<std::string, std::string>, ln_bridge::service *> service_map_t;
        service_map_t service_map;
        pthread_mutex_t service_map_lock;
        std::map<std::string, std::string> stored_mds;

        std::string group_name;

        enum {
            never     = 0,
            on_demand = 1,
            always    = 2
        } upload_message_definitions;
};

class service {
    public:
        //! construct ln_bridge service
        /*!
         * \param clnt ln_bridge client
         * \param svc robotkernel service
         */
        service(ln_bridge::client& clnt, 
                const robotkernel::service_t& svc);

        //! destruct ln_bridge service
        ~service();

        //! register service to ln
        void register_service();

        void _create_ln_message_definition();

        ln_bridge::client& _clnt;
        const robotkernel::service_t& _svc;
        ln::service *_ln_service;
        std::string md;
        std::map<std::string, std::string> sub_mds;
        std::string signature;
        std::string name;

        int handle(ln::service_request& req);

        static int service_cb(ln::client&, ln::service_request& req, 
                void* user_data) {
            service *self = (service *)user_data;
            return self->handle(req);
        }
};
        
}

#endif

