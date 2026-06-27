#ifndef CPKG_MANIFEST_H
#define CPKG_MANIFEST_H

#include <stddef.h>

typedef struct {
    char *name;
    char *version;
    char *compiler;
    char **source_dirs;
    size_t num_source_dirs;
    char **include_dirs;
    size_t num_include_dirs;
    char **libs;
    size_t num_libs;
    char **cflags;
    size_t num_cflags;
    char **defines;
    size_t num_defines;
    char **dep_names;
    char **dep_urls;
    char **dep_versions;
    size_t num_deps;
    char *output;
} manifest_t;

int manifest_load(const char *path, manifest_t *m);
int manifest_save(const char *path, const manifest_t *m);
int manifest_init(const char *name, const char *version);
void manifest_free(manifest_t *m);

#endif
