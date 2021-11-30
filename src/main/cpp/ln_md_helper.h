#ifndef LN_MD_HELPER__H
#define LN_MD_HELPER__H

#include <iostream>
#include <map>
#include <algorithm>

#include "yaml-cpp/yaml.h"

namespace ln_md_helper {

// Checks whether `str' starts with `start' ignoring case
static bool starts_with(const std::string& str, const std::string& start) {
    if (&start == &str) 
        return true; // str and start are the same string

    if (start.length() > str.length()) 
        return false;

    for (size_t i = 0; i < start.length(); ++i) {
        if (start[i] != str[i]) 
            return false;
    }

    return true;
}

static bool ends_with(const std::string& a, const std::string& b) {
    if (b.size() > a.size()) return false;
    return std::equal(a.begin() + a.size() - b.size(), a.end(), b.begin());
}


static std::string service_datatype_to_ln(std::string datatype) {
    if (datatype == "string")
        return std::string("char*");

    return datatype;
}

static bool ln_datatype_is_primitive(std::string datatype) {
    if (datatype == "string")
        return false;

    return true;
}

static std::pair<std::string, int> ln_datatype_size_data[] = {
    std::make_pair("int64_t", 8),
    std::make_pair("uint64_t", 8),
    std::make_pair("int32_t", 4),
    std::make_pair("uint32_t", 4),
    std::make_pair("int16_t", 2),
    std::make_pair("uint16_t", 2),
    std::make_pair("int8_t", 1),
    std::make_pair("uint8_t", 1),
    std::make_pair("int64_t*", 8),
    std::make_pair("uint64_t*", 8),
    std::make_pair("int32_t*", 4),
    std::make_pair("uint32_t*", 4),
    std::make_pair("int16_t*", 2),
    std::make_pair("uint16_t*", 2),
    std::make_pair("int8_t*", 1),
    std::make_pair("uint8_t*", 1),
    std::make_pair("float", 4),
    std::make_pair("double", 8),
    std::make_pair("char*", 1)
};

static std::map<std::string, int> ln_datatype_size_map(
        ln_datatype_size_data,
        ln_datatype_size_data + sizeof(ln_datatype_size_data) / 
        sizeof(ln_datatype_size_data[0]));

static int ln_datatype_size(const std::string& ln_datatype) {
    if (ln_datatype_size_map.find(ln_datatype) != 
            ln_datatype_size_map.end())
        return ln_datatype_size_map[ln_datatype];

    return 0;
}

static void process_node(const YAML::Node& node,
        std::stringstream& ss_md, std::stringstream& ss_signature,
        std::map<std::string, std::string>& sub_mds)
{
    std::map<std::string, std::string> defines_map;

    for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
        if (it != node.begin())
            ss_signature << ",";

        for (const auto& kv : *it) {
            std::string key   = kv.first.as<std::string>();
            std::string value = kv.second.as<std::string>();
            
            std::replace(value.begin(), value.end(), '.', '_');

            if (starts_with(key, "vector")) {
                const size_t equals_idx = key.find_first_of('/');
                if (std::string::npos != equals_idx)
                {
                    //signature "uint32_t 4 1,[uint32_t 4 1,char* 1 1]* 8 1|uint32_t 4 1,[uint32_t 4 1,char* 1 1]* 8 1"

                    std::string vector = key.substr(0, equals_idx);
                    std::string real_key = key.substr(equals_idx + 1);

                    bool is_primitive = ln_datatype_is_primitive(real_key);

                    std::string ln_dt = service_datatype_to_ln(real_key);
                    int ln_dt_size = ln_datatype_size(ln_dt);

                    if (is_primitive) {
                        ss_signature << "uint32_t 4 1," << ln_dt << "* " << ln_dt_size << " 1";
                        ss_md << ln_dt << "* " << value << std::endl;
                    } else {
                        ss_signature << "uint32_t 4 1,[";

                        std::string sub_md_name = std::string("gen/") + key;

                        if (real_key == "string") {
                            // special case, use ln builtin type for that
                            sub_md_name = "ln/string";
                        } else {
                            std::stringstream ss_sub_md;
                            ss_sub_md << ln_dt << " data" << std::endl;
                            sub_mds[key] = ss_sub_md.str();
                        }

                        if (defines_map.find(real_key) == defines_map.end()) {
                            ss_md << "define " << real_key << " as \"" << sub_md_name << "\"" << std::endl;
                            defines_map[real_key] = sub_md_name;
                        } else if (defines_map[real_key] != sub_md_name) {                            
                            std::cout << "error: defines_map[" << real_key << "]=" << defines_map[real_key] << ", but we need " << sub_md_name << std::endl;
                        }

                        ss_md << real_key << "* " << value << std::endl;

                        if (ends_with(ln_dt, std::string("*")))
                            ss_signature << "uint32_t 4 1,";

                        ss_signature << ln_dt << " " << ln_dt_size << " " << "1";

                        ss_signature << "]* " << sizeof(void*) << " 1";
                    }
                }
                else
                {
                    //name = name_value;
                    std::cout << "error after vector" << std::endl;
                }
            } else {
                std::string ln_dt = service_datatype_to_ln(key);
                int ln_dt_size = ln_datatype_size(ln_dt);
                ss_md << ln_dt << " " << value << std::endl;

                if (ends_with(ln_dt, std::string("*")))
                    ss_signature << "uint32_t 4 1,";

                ss_signature << ln_dt << " " << ln_dt_size << " " << "1";
            }
        }
    }
}

inline void create_ln_message_defition(const std::string& sd,
        std::map<std::string, std::string>& mds_map) 
{
    std::stringstream ss_md, ss_signature;
    YAML::Node message_definition = YAML::Load(sd);
    ss_md << "service" << std::endl;
    std::string name;
    std::string signature;
    std::string md;
    std::map<std::string, std::string> sub_mds;

    if (message_definition["name"]) {
        name = message_definition["name"].as<std::string>();
    }

    if (message_definition["request"]) {
        ss_md << "request" << std::endl;

        const YAML::Node& request = message_definition["request"];
        process_node(request, ss_md, ss_signature, mds_map);
    }

    ss_signature << "|";

    if (message_definition["response"]) {
        ss_md << "response" << std::endl;
        const YAML::Node& response = message_definition["response"];
        process_node(response, ss_md, ss_signature, mds_map);
    }

    signature = ss_signature.str();
    md = ss_md.str();

    mds_map[name] = md;
}

}; // namespace ln_md_helper

#endif // LN_MD_HELPER__H
