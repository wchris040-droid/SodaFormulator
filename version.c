#include <stdio.h>
#include "version.h"

Version create_version(int major, int minor, int patch) {
    Version v;
    v.major = major;
    v.minor = minor;
    v.patch = patch;
    return v;
}

Version increment_version(Version current, IncrementType type) {
    Version new_ver = current;
    
    switch(type) {
        case INCREMENT_MAJOR:
            new_ver.major++;
            new_ver.minor = 0;
            new_ver.patch = 0;
            break;
        case INCREMENT_MINOR:
            new_ver.minor++;
            new_ver.patch = 0;
            break;
        case INCREMENT_PATCH:
            new_ver.patch++;
            break;
    }
    
    return new_ver;
}

void print_version(Version v) {
    printf("%d.%d.%d", v.major, v.minor, v.patch);
}

void get_full_version_string(Version v, char* output) {
    sprintf(output, "%d.%d.%d", v.major, v.minor, v.patch);
}
