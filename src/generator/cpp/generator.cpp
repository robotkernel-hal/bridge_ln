#include "config.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <errno.h>

#include <list>
#include "ln_md_helper.h"
#include <fstream>
typedef void (*get_sd_t)(std::list<std::string>& sd_list);

#include "fs_tools.h"

using namespace std;

int usage(void) {
    printf("usage: bridge_ln_generator --so_file | -s <filename> --out | -o <outdir> [-v]\n");
    printf("\n");
    printf("  --so_file, -s <filename>    so_file to get service definitions from\n");
    printf("  --out, -o <outdir>          output directory for ln message definitions\n");
    printf("  --verbose, -v               be more verbose\n");
    return 0;
}

int main(int argc, char **argv) {
    void *so_handle;
    string so_file_name;
    string md_base_path = "";
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--so_file") == 0) || (strcmp(argv[i], "-s") == 0)) {
            if (++i >= argc) {
                printf("--so_file filename missing\n");
                return usage();
            }

            so_file_name = string(argv[i]);

        } else if ((strcmp(argv[i], "--out") == 0) || (strcmp(argv[i], "-o") == 0)) {
            if (++i >= argc) {
                printf("--out filename missing\n");
                return usage();
            }

            md_base_path = string(argv[i]);
        } else if ((strcmp(argv[i], "--verbose") == 0) || (strcmp(argv[i], "-v") == 0)) {
            verbose = true;
        }
    }

    if ((so_file_name == "") || (md_base_path == "")) {
        return usage();
    }

    printf( "bridge_ln_generator\n"
            "Generating links-and-nodes MDs for %s\n", so_file_name.c_str());

    if (nullptr == (so_handle = dlopen(so_file_name.c_str(), RTLD_LOCAL | RTLD_LAZY))) {
        printf("Unable to open %s: %s\n", so_file_name.c_str(), dlerror());
        return -1;
    }

    get_sd_t get_sd;
    get_sd = (get_sd_t)dlsym(so_handle, "get_sd");

    if (nullptr != get_sd) {
        std::list<std::string> sd_list;
        std::map<std::string, std::string> ln_mds_map;

        (*get_sd)(sd_list);
        for (const auto& el : sd_list) {
            ln_md_helper::create_ln_message_defition(el, ln_mds_map);
        }

        for (const auto& kv : ln_mds_map) {
            string md_fn = md_base_path + string("/") + kv.first.c_str();
            make_path(md_fn.substr(0, md_fn.find_last_of('/')));

            printf("Created MD: %s\n", md_fn.c_str());
            std::ofstream file(md_fn.c_str(), std::ofstream::out);
            file << kv.second;
            file.close();
        }
    } else {
        printf("No 'get_sd' symbol in there -> nothing to do.\n");
    }

    return 0;
}

