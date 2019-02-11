#ifndef __LN_BRIDGE_H__
#define __LN_BRIDGE_H__

#include "robotkernel/kernel.h"
#include "robotkernel/service.h"
#include "robotkernel/runnable.h"

#include "ln.h"
#include "ln_cppwrapper.h"
#include "robotkernel/bridge_base.h"

namespace ln_bridge {
#ifdef EMACS
}
#endif

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

        void _create_ln_message_defition();
        void _process_node(const YAML::Node& node,
                std::stringstream& ss_md, std::stringstream& ss_signature);

        ln_bridge::client& _clnt;
        const robotkernel::service_t& _svc;
        ln::service *_ln_service;
        std::string md;
        std::map<std::string, std::string> sub_mds;
        std::string signature;

        int handle(ln::service_request& req);

        static int service_cb(ln::client&, ln::service_request& req, 
                void* user_data) {
            service *self = (service *)user_data;
            return self->handle(req);
        }
};
        
#ifdef EMACS
{
#endif
}

#endif

