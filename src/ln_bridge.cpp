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

#include "ln_bridge.h"
#include "ln_md_helper.h"
#include "robotkernel/helpers.h"
#include "robotkernel/service.h"
#include "robotkernel/rk_type.h"
#include "robotkernel/robotkernel.h"

#include <functional>
#include <algorithm>
#include <stdexcept>

using namespace std;
using namespace robotkernel;
using namespace ln_md_helper;

BRIDGE_DEF(bridge_ln, ln_bridge::client);

//! construct ln_bridge client
ln_bridge::client::client(const char*& bridgename, YAML::Node& node) :
    bridge_base(bridgename, "bridge_ln", node),
    runnable(0, 0, bridgename),
    clnt(NULL)
{
    pthread_mutex_init(&service_map_lock, NULL);

    group_name = string_printf("ln_bridge_%s", bridgename);

    string umd = get_as<string>(node, "upload_message_definitions", "never");
    if (umd == "never") {
        upload_message_definitions = never;
    } else if (umd == "on_demand") {
        upload_message_definitions = on_demand;
    } else if (umd == "always") {
        upload_message_definitions = always;
    } else {
        throw runtime_error("key \"upload_message_definitions\" has to be one of [ \"never\", \"on_demand\", \"always\" ]");
    }
}

//! destruct ln_bridge client
ln_bridge::client::~client() {
    stop();

    for (auto it = service_map.begin(); it != service_map.end(); ++it)
        delete it->second;

    service_map.clear();

    if (clnt)
        delete clnt;

    pthread_mutex_destroy(&service_map_lock);
}

//! init method
void ln_bridge::client::init() {
    log(info, "starting client handler thread\n");

    start();
}

//!< handler function called if thread is running
void ln_bridge::client::run() {
    while (running()) {
        if (clnt) {
            struct timespec ts = { 0, 100000000 };
            nanosleep(&ts, NULL);
        } else {
            try {
                log(verbose, "creating new ln client...\n");
                clnt = new ln::client(name, 0, NULL);
                clnt->set_max_threads("main", 16);

                pthread_mutex_lock(&service_map_lock);

                for (service_map_t::iterator it = service_map.begin();
                        it != service_map.end(); ++it) {
                    it->second->register_service();
                }

                clnt->handle_service_group_in_thread_pool(group_name.c_str(), "main");
                pthread_mutex_unlock(&service_map_lock);
            } catch(exception& e) {
                log(warning, "creating ln client failed: %s\n", e.what());

                sleep(1);
                clnt = NULL;
            }
        }
    }
}


//! create and register ln service
/*!
 * \param svc robotkernel service struct
 */
void ln_bridge::client::add_service(const robotkernel::service_t& svc) {
    log(verbose, "trying to add service \"%s.%s\"\n", svc.owner.c_str(), svc.name.c_str());

    ln_bridge::service *ln_svc = new ln_bridge::service(*this, svc);

    log(verbose, "created ln service \"%s.%s\"\nmd:\n%s\nsignature:\n%s\n", 
            svc.owner.c_str(), svc.name.c_str(), ln_svc->md.c_str(), ln_svc->signature.c_str());

    pthread_mutex_lock(&service_map_lock);
    service_map[std::make_pair(svc.owner, svc.name)] = ln_svc;
    pthread_mutex_unlock(&service_map_lock);
}

//! unregister and remove ln service 
/*!
 * \param svc robotkernel service struct
 */
void ln_bridge::client::remove_service(
        const robotkernel::service_t& svc) {
    pthread_mutex_lock(&service_map_lock);

    for (auto it = service_map.begin(); it != service_map.end(); ++it) {
        if ((it->first.first == svc.owner) && (it->first.second == svc.name)) {
            ln_bridge::service *ln_svc = it->second;
            service_map.erase(it);
            delete ln_svc;

            break;
        }
    }

    pthread_mutex_unlock(&service_map_lock);
}

//! construct ln_bridge service
/*!
 * \param clnt ln_bridge client
 * \param svc robotkernel service
 */
ln_bridge::service::service(ln_bridge::client& clnt, 
        const robotkernel::service_t& svc) : _clnt(clnt), _svc(svc), _ln_service(NULL), name("") {
    _create_ln_message_definition(); 

    register_service();
}

//! register service to ln
void ln_bridge::service::register_service() {
    if (!_clnt.clnt || _ln_service)
        return;

    string svc_md_name;

    if (name != "") {
        svc_md_name = name;

        try {
            std::string message_definition;
            unsigned int message_size;
            std::string hash;

            _clnt.clnt->get_message_definition(name,
                    message_definition, message_size, hash);
        } catch(exception& e) {
            svc_md_name = string("robotkernel/") + name;
        }
    } else {
        name = _svc.name;
        string prefix = _clnt.clnt->name + "." + _svc.owner + ".";
        size_t svc_hash = hash<string>()(prefix);
        svc_md_name = to_string(svc_hash) + "." + name;
    }

    // create service name
    string svc_name = _clnt.clnt->name + "." + _svc.owner + "." + _svc.name;

    // put ln message definition. this will create 
    // ~/ln_message_definitions/gen/<svc_name>
    for (map<string, string>::iterator it = sub_mds.begin(); 
            it != sub_mds.end(); ++it) {
        _clnt.clnt->put_message_definition(it->first, it->second);
    }

    if (_clnt.upload_message_definitions != client::never) {
        bool already_put = false;
        for (const auto& kv : _clnt.stored_mds) {
            if (!kv.second.compare(md)) {
                already_put = true;
                svc_md_name = kv.first;
                break;
            }
        }

        if (!already_put) {
            bool needs_upload = true;

            if (_clnt.upload_message_definitions == client::on_demand) {
                try {
                    std::string message_definition;
                    unsigned int message_size;
                    std::string hash;

                    _clnt.clnt->get_message_definition(svc_md_name,
                            message_definition, message_size, hash);

                    _clnt.stored_mds[svc_md_name] = md;
                    needs_upload = false;
                } catch(exception& e) {}
            }

            if (needs_upload) {
                _clnt.log(verbose, "putting md %s\n", svc_md_name.c_str());
                _clnt.clnt->put_message_definition(svc_md_name, md);

                _clnt.stored_mds[svc_md_name] = md;
                svc_md_name = "gen/" + svc_md_name;
            }
        }
    }

    // get ln service provider
    _ln_service = _clnt.clnt->get_service_provider(
            svc_name, svc_md_name, signature);

    // set handler and register
    _ln_service->set_handler(&ln_bridge::service::service_cb, this);
    _ln_service->do_register(_clnt.group_name.c_str());
}; 

//! destruct ln_bridge service
ln_bridge::service::~service() {
    if (_ln_service) {
        _clnt.clnt->release_service(_ln_service);
        _ln_service = NULL;
    }
}

typedef struct __attribute__((__packed__)) ln_vector {
    uint32_t len;
    const uint8_t *val;
} __attribute__((__packed__)) ln_vector_t;

int ln_bridge::service::handle(ln::service_request& req) {
    uint8_t svc[1024];
    req.set_data(&svc[0], signature.c_str());
    uint64_t adr = (uint64_t)&svc[0];

    // request arguments
    robotkernel::service_arglist_t service_request;

    YAML::Node message_definition = YAML::Load(_svc.service_definition);
    if (message_definition["request"]) {
        const YAML::Node& request = message_definition["request"];

        for (YAML::const_iterator it = request.begin(); 
                it != request.end(); ++it) {
            for (const auto& kv : *it) {
                string key   = kv.first.as<string>();
                string value = kv.second.as<string>();

                string ln_dt = service_datatype_to_ln(key);

                if (ln_dt == "char*") {
                    uint32_t tmp_len = ((uint32_t *)adr)[0];
                    adr += 4;
                    char *tmp_adr = ((char **)adr)[0];
                    adr += sizeof(char*);

                    service_request.push_back(string(tmp_adr, tmp_len));
                } else if (starts_with(key, "vector")) {
                    const size_t equals_idx = key.find_first_of('/');
                    if (std::string::npos != equals_idx)
                    {
                        //signature "uint32_t 4 1,[uint32_t 4 1,char* 1 1]* 8 1|uint32_t 4 1,[uint32_t 4 1,char* 1 1]* 8 1"

                        key = key.substr(equals_idx + 1);
                        ln_dt = service_datatype_to_ln(key);

#define add_vector_type(type) \
                        if (ln_dt.compare(#type) == 0) {                                                            \
                            uint32_t len = ((uint32_t *)adr)[0];                                                    \
                            adr += 4;                                                                               \
                            std::vector<type> entries(len);                                                         \
                            type *tmp_adr = ((type **)adr)[0];                                                      \
                            memcpy(&entries[0], tmp_adr, sizeof(type) * len);                                       \
                            adr += sizeof(type *);                                                                  \
                            service_request.push_back(entries);                                                     \
                        }
                        add_vector_type(uint64_t)
                        else add_vector_type(int64_t)
                        else add_vector_type(uint32_t)
                        else add_vector_type(int32_t)
                        else add_vector_type(uint16_t)
                        else add_vector_type(int16_t)
                        else add_vector_type(uint8_t)
                        else add_vector_type(int8_t)
                        else add_vector_type(float)
                        else add_vector_type(double)
#undef add_vector_type

#define add_vector_type_char(type) \
                        if (ln_dt.compare(#type) == 0) {                                                            \
                            uint32_t len = ((uint32_t *)adr)[0];                                                    \
                            adr += 4;                                                                               \
                            std::vector<string> entries(len);                                                       \
                            ln_vector_t* lnentries = *(ln_vector_t **)adr;                                          \
                            adr += sizeof(ln_vector_t *);                                                           \
                            for (unsigned i = 0; i < len; ++i) {                                                    \
                                entries[i] = string((char *)(lnentries[i].val), (lnentries[i].len));                \
                            }                                                                                       \
                            service_request.push_back(entries);                                                     \
                        }
                        add_vector_type_char(char*);
#undef add_vector_type_char
                    }

                } else if (ends_with(ln_dt, string("*"))) {               
                    service_request.push_back(((uint32_t *)adr)[0]);    //<! array length
                    adr += 4;                
#define push_back_type(type) \
                    if (ln_dt.compare(#type) == 0) {                    \
                        service_request.push_back(((type*)adr)[0]);     \
                        adr += sizeof(type);                            \
                    }

                    push_back_type(uint64_t*)
                    else push_back_type(int64_t*)
                    else push_back_type(uint32_t*)
                    else push_back_type(int32_t*)
                    else push_back_type(uint16_t*)
                    else push_back_type(int16_t*)
                    else push_back_type(uint8_t*)
                    else push_back_type(int8_t*)
                    else push_back_type(float*)
                    else push_back_type(double*)
                } else {
                    push_back_type(uint64_t)
                    else push_back_type(int64_t)
                    else push_back_type(uint32_t)
                    else push_back_type(int32_t)
                    else push_back_type(uint16_t)
                    else push_back_type(int16_t)
                    else push_back_type(uint8_t)
                    else push_back_type(int8_t)
                    else push_back_type(float)
                    else push_back_type(double)
#undef push_back_type
                }
            }
        }
    }

    // call robotkernel service
    robotkernel::service_arglist_t service_response;
    _svc.callback(service_request, service_response);

    std::list<uint8_t *> to_free;
    std::list<uint8_t *> to_delete;
    std::list<uint8_t *> to_delete_vec;

    if (message_definition["response"]) {
        const YAML::Node& response = message_definition["response"];
        int i = 0;

        for (YAML::const_iterator it = response.begin(); 
                it != response.end(); ++it) {
            for (const auto& kv : *it) {
                string key   = kv.first.as<string>();
                string value = kv.second.as<string>();

                string ln_dt = service_datatype_to_ln(key);
                //            int ln_dt_size = ln_datatype_size(ln_dt);

                if (ln_dt.compare("char*") == 0) {
                    const string& tmp_string = service_response[i++];
                    ((uint32_t *)adr)[0] = (uint32_t)tmp_string.size();
                    adr += 4;
                    if (tmp_string.size()) {
                        ((const char **)adr)[0] = (const char *)strdup(tmp_string.c_str());
                        to_free.push_back((uint8_t *)(((const char **)adr)[0]));
                    } else 
                        ((const char **)adr)[0] = NULL;
                    adr += sizeof(char *);
                } else if (starts_with(key, "vector")) {
                    const size_t equals_idx = key.find_first_of('/');
                    if (std::string::npos != equals_idx)
                    {
                        //signature "uint32_t 4 1,[uint32_t 4 1,char* 1 1]* 8 1|uint32_t 4 1,[uint32_t 4 1,char* 1 1]* 8 1"

                        key = key.substr(equals_idx + 1);
                        ln_dt = service_datatype_to_ln(key);


#define add_vector_type(type) \
                        if (ln_dt.compare(#type) == 0) {                                                            \
                            const std::vector<type>& elem = service_response[i++];                                  \
                            ((uint32_t *)adr)[0] = (uint32_t)elem.size();                                           \
                            adr += 4;                                                                               \
                            type* entries = new type[elem.size()];                                                  \
                            to_delete_vec.push_back((uint8_t *)entries);                                            \
                            memcpy(&entries[0], &elem[i], sizeof(type) * elem.size());                              \
                            ((type **)adr)[0] = entries;                                                            \
                            adr += sizeof(type *);                                                                  \
                        }

#define add_vector_type_char(type) \
                        if (ln_dt.compare(#type) == 0) {                                                            \
                            const std::vector<string>& elem = service_response[i++];                                \
                            ((uint32_t *)adr)[0] = (uint32_t)elem.size();                                           \
                            adr += 4;                                                                               \
                            ln_vector_t* entries = new ln_vector_t[elem.size()];                                    \
                            to_delete_vec.push_back((uint8_t *)entries);                                            \
                            for (unsigned i = 0; i < elem.size(); ++i) {                                            \
                                string entry = elem[i];                                                             \
                                entries[i].len = entry.length();                                                    \
                                entries[i].val = (const uint8_t *)(strdup(entry.c_str()));                          \
                                to_free.push_back((uint8_t *)entries[i].val);                                       \
                            }                                                                                       \
                            ((ln_vector_t **)adr)[0] = entries;                                                     \
                            adr += sizeof(void*);                                                                   \
                        }

                        add_vector_type(uint64_t)
                        else add_vector_type(int64_t)
                        else add_vector_type(uint32_t)
                        else add_vector_type(int32_t)
                        else add_vector_type(uint16_t)
                        else add_vector_type(int16_t)
                        else add_vector_type(uint8_t)
                        else add_vector_type(int8_t)
                        else add_vector_type(float)
                        else add_vector_type(double)
                        else add_vector_type_char(char*)
                    }
                } else if (ends_with(ln_dt, string("*"))) {
                    ((uint32_t *)adr)[0] = service_response[i++];
                    adr += 4;

#define push_back_type(type) \
                    if (ln_dt.compare(#type) == 0) {             \
                        ((type*)adr)[0] = service_response[i++]; \
                        adr += sizeof(type);                     \
                    }

                    push_back_type(uint64_t*)
                    else push_back_type(int64_t*)
                    else push_back_type(uint32_t*)
                    else push_back_type(int32_t*)
                    else push_back_type(uint16_t*)
                    else push_back_type(int16_t*)
                    else push_back_type(uint8_t*)
                    else push_back_type(int8_t*)
                    else push_back_type(float*)
                    else push_back_type(double*)
                } else {
                    push_back_type(uint64_t)
                    else push_back_type(int64_t)
                    else push_back_type(uint32_t)
                    else push_back_type(int32_t)
                    else push_back_type(uint16_t)
                    else push_back_type(int16_t)
                    else push_back_type(uint8_t)
                    else push_back_type(int8_t)
                    else push_back_type(float)
                    else push_back_type(double)

#undef push_back_type
                }
            }
        }
    }

    req.respond();

    for (std::list<uint8_t *>::iterator it = to_delete.begin();
            it != to_delete.end(); ++it) {
        delete (*it);
    }
    for (std::list<uint8_t *>::iterator it = to_delete_vec.begin();
            it != to_delete_vec.end(); ++it) {
        delete[] (*it);
    }
    for (std::list<uint8_t *>::iterator it = to_free.begin();
            it != to_free.end(); ++it) {
        free((*it));
    }

    return 0;
}

void ln_bridge::service::_create_ln_message_definition() {
    std::stringstream ss_md, ss_signature;
    YAML::Node message_definition = YAML::Load(_svc.service_definition);
    ss_md << "service" << endl;

    if (message_definition["name"]) {
        name = message_definition["name"].as<string>();
    }

    if (message_definition["request"]) {
        ss_md << "request" << endl;

        const YAML::Node& request = message_definition["request"];
        ln_md_helper::process_node(request, ss_md, ss_signature, sub_mds);
    }

    ss_signature << "|";

    if (message_definition["response"]) {
        ss_md << "response" << endl;
        const YAML::Node& response = message_definition["response"];
        ln_md_helper::process_node(response, ss_md, ss_signature, sub_mds);
    }

    signature = ss_signature.str();
    md = ss_md.str();
}

