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
#include "robotkernel/helpers.h"
#include "robotkernel/service.h"
#include "robotkernel/robotkernel.h"

#include <functional>
#include <algorithm>
#include <stdexcept>
#include <type_traits>

#include "ln_helper/field.h"
#include "ln_helper/datatype.h"
#include "ln_helper/service.h"
#include "ln_helper/helper.h"

using namespace std;
using namespace robotkernel;
using namespace robotkernel::helpers;
//using namespace ln_md_helper;

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
    if (_svc.owner.compare(robotkernel::name()) == 0) {
        svc_name = _clnt.clnt->name + "." + _svc.name;
    }

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

template <typename T>
void assign_to_adr(uint8_t *& adr, const T& value) {
    *reinterpret_cast<T*>(adr) = value;
    adr = static_cast<uint8_t*>(adr) + sizeof(T);
}

template <typename T>
T assign_from_adr(uint8_t *&adr) {
    T value = *reinterpret_cast<T*>(adr);
    adr = static_cast<uint8_t*>(adr) + sizeof(T);
    return value;
}

int ln_bridge::service::handle(ln::service_request& req) {
    uint8_t svc[1024];
    req.set_data(&svc[0], signature.c_str());
    uint8_t *adr = (uint8_t *)&svc[0];

    // request arguments
    YAML::Node service_request, service_response;

    auto svc_desc = robotkernel::get_service_definition(_svc.service_definition);
    YAML::Node message_definition = YAML::Load(svc_desc);
    _clnt.log(verbose, "got message definition:\n%s\n", svc_desc.c_str());

    if (message_definition["request"]) {
        const YAML::Node& request = message_definition["request"];

        std::function<YAML::Node(const std::string&, const bool&, uint8_t*&)> process_request_entry = 
            [&process_request_entry]
            (const std::string& dtype, const bool& is_array, uint8_t*& adr) -> YAML::Node 
        {
            auto get_type_string = [&]() -> YAML::Node {
                auto local_add = [](uint8_t*& adr) -> std::string {
                    uint32_t tmp_len = assign_from_adr<uint32_t>(adr);
                    char *tmp_str = assign_from_adr<char *>(adr);
                    return std::string(tmp_str, tmp_len);
                };
                
                if (is_array) {
                    uint32_t vec_len = assign_from_adr<uint32_t>(adr);
                    uint8_t *vec_adr = assign_from_adr<uint8_t *>(adr);
                    std::vector<std::string> entries;

                    for (unsigned i = 0; i < vec_len; ++i) {
                        entries.push_back(local_add(vec_adr));
                    }

                    return YAML::Node(entries);
                } else {
                    return YAML::Node(local_add(adr));
                }
            };

            auto get_type = [&](auto from_type_tag, auto to_type_tag) -> YAML::Node {
                using T = decltype(from_type_tag);
                using U = decltype(to_type_tag);

                if (is_array) {
                    uint32_t vec_len = assign_from_adr<uint32_t>(adr);
                    uint8_t *vec_adr = assign_from_adr<uint8_t *>(adr);
                    std::vector<U> entries;

                    for (unsigned i = 0; i < vec_len; ++i) {
                        entries.push_back(static_cast<U>(assign_from_adr<T>(vec_adr)));
                    }

                    return YAML::Node(entries);
                } else {
                    return YAML::Node(static_cast<U>(assign_from_adr<T>(adr)));
                }
            };

            if (dtype == "uint64_t")      return get_type(uint64_t{}, uint64_t{});
            else if (dtype == "int64_t")  return get_type(int64_t{},  int64_t{});
            else if (dtype == "uint32_t") return get_type(uint32_t{}, uint32_t{});
            else if (dtype == "int32_t")  return get_type(int32_t{},  int32_t{});
            else if (dtype == "uint16_t") return get_type(uint16_t{}, uint16_t{});
            else if (dtype == "int16_t")  return get_type(int16_t{},  int16_t{});
            else if (dtype == "uint8_t")  return get_type(uint8_t{},  uint16_t{}); 
            else if (dtype == "int8_t")   return get_type(int8_t{},   int16_t{}); 
            else if (dtype == "float")    return get_type(float{},    float{});
            else if (dtype == "double")   return get_type(double{},   double{});
            else if (dtype == "string")   return get_type_string();
            else { // this is a custom type 
                auto dtype_desc = robotkernel::get_datatype_definition(dtype);
                YAML::Node dtype_node = YAML::Load(dtype_desc);
                YAML::Node fields_node = dtype_node["fields"];

                if (is_array) {
                    uint32_t vec_len = assign_from_adr<uint32_t>(adr);
                    uint8_t *vec_adr = assign_from_adr<uint8_t *>(adr);

                    std::vector<YAML::Node> vec_entries;
                    for (unsigned i = 0; i < vec_len; ++i) {
                        vec_entries.push_back(process_request_entry(dtype, false, vec_adr));
                    }

                    return YAML::Node(vec_entries);
                } else {
                    YAML::Node tmp_node;
                    for (const auto& f : fields_node) {
                        string name = f.first.as<std::string>();
                        string dtype = get_as<string>(f.second, "type");
                        bool is_array = get_as<bool>(f.second, "array", false);

                        tmp_node[name] = process_request_entry(dtype, is_array, adr);                    
                    }

                    return tmp_node;
                }
                return YAML::Node();
            }
        };

        for (YAML::const_iterator it = request.begin(); it != request.end(); ++it) {
            string name = it->first.as<std::string>();
            string dtype = get_as<string>(it->second, "type");
            bool is_array = get_as<bool>(it->second, "array", false);

            service_request[name] = process_request_entry(dtype, is_array, adr);
        }
    }

    // call robotkernel service
    _svc.callback(service_request, service_response);

    std::list<uint8_t *> to_free;
    std::list<uint8_t *> to_delete;
    std::list<uint8_t *> to_delete_vec;

    if (message_definition["response"]) {
        const YAML::Node& response = message_definition["response"];

        std::function<void(const YAML::Node&, const std::string&, const bool&, uint8_t*&)> process_response_entry = 
            [&service_response, &to_free, &to_delete, &to_delete_vec, &process_response_entry](
                    const YAML::Node& resp_node, const std::string& dtype, const bool& is_array, uint8_t*& adr) -> void 
        {
            auto add_type_string = [&]() {
                auto local_add = [&to_free](const std::string& tmp_string, uint8_t*& tmp_adr) -> void {
                    char *tmp_cstring = NULL;
                    if (tmp_string.size()) {
                        tmp_cstring = (char *)strdup(tmp_string.c_str());
                        to_free.push_back(reinterpret_cast<uint8_t *>(tmp_cstring));
                    } 
    
                    assign_to_adr(tmp_adr, (uint32_t)tmp_string.size());
                    assign_to_adr(tmp_adr, tmp_cstring);
                };

                if (is_array) {
                    const std::vector<std::string> str_elem = resp_node.as<std::vector<std::string> >();

                    struct string_vec { uint32_t len; char *str; };
                    struct string_vec *string_entries = new struct string_vec[str_elem.size()];
                    to_delete_vec.push_back(reinterpret_cast<uint8_t *>(string_entries));

                    uint8_t *tmp_adr = reinterpret_cast<uint8_t *>(string_entries);
                    for (const auto& entry : str_elem) { local_add(entry, tmp_adr); }

                    assign_to_adr(adr, static_cast<uint32_t>(str_elem.size()));
                    assign_to_adr(adr, reinterpret_cast<uint8_t *>(string_entries));
                } else {
                    local_add(resp_node.as<string>(), adr);
                }
            };

            auto add_type = [&](auto from_type_tag, auto to_type_tag) {
                using T = decltype(from_type_tag);
                using U = decltype(to_type_tag);

                if (is_array) {
                    const std::vector<T> elem = resp_node.as<std::vector<T> >();

                    U* entries = new U[elem.size()];
                    to_delete_vec.push_back(reinterpret_cast<uint8_t *>(entries));

                    for (unsigned i = 0; i < elem.size(); ++i) { entries[i] = elem[i]; }

                    assign_to_adr(adr, static_cast<uint32_t>(elem.size()));
                    assign_to_adr(adr, entries);
                } else {
                    assign_to_adr(adr, static_cast<U>(resp_node.as<T>()));
                }
            };

            std::function<size_t(const YAML::Node&)> calc_ln_size = [&calc_ln_size](const YAML::Node& dtype_node) -> size_t {
                size_t ret = 0;

                for (const auto& f : dtype_node) {
                    const std::string& tmp_dtype = get_as<std::string>(f, "type");

                    if (get_as<bool>(f, "array", false) || (tmp_dtype == "string")) {
                        ret += sizeof(uint32_t) /* size field */ + sizeof(uint8_t *) /* array data */;
                    } else if (ln_helper::is_builtin_dtype(tmp_dtype)) {
                        ret += ln_helper::ln_datatype_size(tmp_dtype);
                    } else {
                        auto tmp_dtype_desc = robotkernel::get_datatype_definition(tmp_dtype);
                        YAML::Node tmp_dtype_node = YAML::Load(tmp_dtype_desc);
                        if (tmp_dtype_node["fields"]) { ret += calc_ln_size(tmp_dtype_node["fields"]); }
                    }
                }

                return ret;
            };

            if (dtype == "uint64_t")      add_type(uint64_t{}, uint64_t{});
            else if (dtype == "int64_t")  add_type(int64_t{},  int64_t{});
            else if (dtype == "uint32_t") add_type(uint32_t{}, uint32_t{});
            else if (dtype == "int32_t")  add_type(int32_t{},  int32_t{});
            else if (dtype == "uint16_t") add_type(uint16_t{}, uint16_t{});
            else if (dtype == "int16_t")  add_type(int16_t{},  int16_t{});
            else if (dtype == "uint8_t")  add_type(uint16_t{}, uint8_t{});
            else if (dtype == "int8_t")   add_type(int16_t{},  int8_t{});
            else if (dtype == "float")    add_type(float{},    float{});
            else if (dtype == "double")   add_type(double{},   double{});
            else if (dtype == "string")   add_type_string();
            else { // this is a custom type 
                auto dtype_desc = robotkernel::get_datatype_definition(dtype);
                YAML::Node dtype_node = YAML::Load(dtype_desc);
                YAML::Node fields_node = dtype_node["fields"];

                if (is_array) {
                    const std::vector<YAML::Node> elem = resp_node.as<std::vector<YAML::Node> >();

                    size_t entry_size = calc_ln_size(fields_node);
                    uint8_t *entries = new uint8_t[entry_size * elem.size()];
                    to_delete_vec.push_back(entries);

                    uint8_t *tmp_adr = entries;
                    for (const auto& entry : elem) {
                        process_response_entry(entry, dtype, false, tmp_adr);
                    }

                    assign_to_adr(adr, static_cast<uint32_t>(elem.size()));
                    assign_to_adr(adr, entries);
                } else {
                    for (const auto& f : fields_node) {
                        string name = f.first.as<std::string>();
                        string dtype = get_as<string>(f.second, "type");
                        bool is_array = get_as<bool>(f.second, "array", false);

                        process_response_entry(resp_node[name], dtype, is_array, adr);
                    }
                }
            }
        };
        
        for (YAML::const_iterator it = response.begin(); it != response.end(); ++it) {
            string name = it->first.as<std::string>();
            string dtype = get_as<string>(it->second, "type");
            bool is_array = get_as<bool>(it->second, "array", false);

            process_response_entry(service_response[name], dtype, is_array, adr);
        }
    }

    req.respond();

    for (auto it = to_delete.begin(); it != to_delete.end(); ++it) { delete (*it); }
    for (auto it = to_delete_vec.begin(); it != to_delete_vec.end(); ++it) { delete[] (*it); }
    for (auto it = to_free.begin(); it != to_free.end(); ++it) { free((*it)); }

    return 0;
}

void ln_bridge::service::_create_ln_message_definition() {
    ln_helper::helper h;
    auto svc_desc = robotkernel::get_service_definition(_svc.service_definition);
    YAML::Node sd_node = YAML::Load(svc_desc);
    name = _svc.service_definition;

    _clnt.log(verbose, "%s: starting creating ln message definition and signature...\n", name.c_str());

    std::function<void(const YAML::Node&, ln_helper::helper&)> get_custom_dtypes = 
        [&](const YAML::Node& node, ln_helper::helper& h) -> void 
    {
        _clnt.log(verbose, "%s -> get_custom_dtypes\n", name.c_str());

        for (const auto& e : node) {
            // something like "myfield: { type: uint32_t, array: true }"
            // or             "anotherfield: { type: mycustom }"
            auto dtype = ::robotkernel::helpers::get_as<std::string>(e.second, "type");
            if (!ln_helper::is_builtin_dtype(dtype) && (h.dt_map.find(dtype) == h.dt_map.end())) {
                _clnt.log(verbose, "%s: trying to add custom dtype \"%s\"\n", name.c_str(), dtype.c_str());

                auto dtype_desc = ::robotkernel::get_datatype_definition(dtype);
                
                _clnt.log(verbose, "%s: got desc\n%s\n", name.c_str(), dtype_desc.c_str());
                auto dtype_node = YAML::Load(dtype_desc);
                h.add_datatype(name, dtype_node);

                _clnt.log(verbose, "%s: added \"%s\", now recurse\n", name.c_str(), dtype.c_str()); 
                if (dtype_node["fields"]) { get_custom_dtypes(dtype_node["fields"], h); }
            }
        }
    };


    if (sd_node["request"]) get_custom_dtypes(sd_node["request"], h);
    if (sd_node["response"]) get_custom_dtypes(sd_node["response"], h);

    _clnt.log(verbose, "%s: added all custom dtypes\n", name.c_str());

    auto svc = h.add_service(_svc.owner + "." + _svc.name, sd_node);

    _clnt.log(verbose, "%s: got our helper service\n", name.c_str());

    ln_helper::ln_signature_stream lnss;
    lnss << *svc;
    signature = lnss.str();

    for (const auto& dtype : h.dt_map) {
        ln_helper::ln_mddef_stream mdss;
        mdss << *dtype.second;
        sub_mds[dtype.first] = mdss.str();
    }

    ln_helper::ln_mddef_stream mdss;
    mdss << *svc;
    md = mdss.str();

    _clnt.log(verbose,"signature: %s\n\n, md:\n%s\n", signature.c_str(), md.c_str());
}

