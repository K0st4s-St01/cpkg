#ifndef CPKG_BUILD_H
#define CPKG_BUILD_H

#include "manifest.h"

int build_project(const manifest_t *m, int run_after, int argc, char **argv);

#endif
