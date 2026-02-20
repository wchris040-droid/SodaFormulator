#ifndef VERSION_H
#define VERSION_H

typedef struct {
    int major;
    int minor;
    int patch;
} Version;

typedef enum {
    INCREMENT_MAJOR,
    INCREMENT_MINOR,
    INCREMENT_PATCH
} IncrementType;

Version create_version(int major, int minor, int patch);
Version increment_version(Version current, IncrementType type);
void print_version(Version v);
void get_full_version_string(Version v, char* output);

#endif
