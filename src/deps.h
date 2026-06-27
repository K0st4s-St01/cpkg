#ifndef CPKG_DEPS_H
#define CPKG_DEPS_H

#include "manifest.h"

int deps_install(const manifest_t *m, const char *dep_name, const char *url, const char *version);
int deps_install_all(const manifest_t *m);
int deps_find_sources(const manifest_t *m, const char *dep_name, char ***files, size_t *count);
int dir_exists(const char *path);

#endif
