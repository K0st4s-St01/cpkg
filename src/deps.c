#include "deps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

int dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int mkdir_p(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return mkdir(tmp, 0755);
}

int deps_install(const manifest_t *m, const char *dep_name, const char *url, const char *version) {
    (void)m;
    if (!url) {
        fprintf(stderr, "error: no URL for dependency '%s'\n", dep_name);
        return -1;
    }

    if (!dir_exists("cpkg_modules")) {
        if (mkdir_p("cpkg_modules") != 0) {
            perror("mkdir cpkg_modules");
            return -1;
        }
    }

    char dep_dir[1024];
    snprintf(dep_dir, sizeof(dep_dir), "cpkg_modules/%s", dep_name);

    if (dir_exists(dep_dir)) {
        printf("  dependency '%s' already installed\n", dep_name);
        return 0;
    }

    printf("  installing '%s' from %s ...\n", dep_name, url);

    char cmd[4096];
    if (version && version[0]) {
        snprintf(cmd, sizeof(cmd), "git clone --depth 1 --branch %s %s %s 2>/dev/null", version, url, dep_dir);
    } else {
        snprintf(cmd, sizeof(cmd), "git clone --depth 1 %s %s 2>/dev/null", url, dep_dir);
    }

    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "error: failed to clone '%s' from %s\n", dep_name, url);
        return -1;
    }

    printf("  installed '%s'\n", dep_name);
    return 0;
}

int deps_install_all(const manifest_t *m) {
    if (m->num_deps == 0) return 0;
    printf("installing dependencies ...\n");
    for (size_t i = 0; i < m->num_deps; i++) {
        if (deps_install(m, m->dep_names[i], m->dep_urls[i], m->dep_versions[i]) != 0)
            return -1;
    }
    return 0;
}

static void collect_sources_in_dir(const char *dir, char ***files, size_t *count, size_t *cap) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        char sub[1024];
        snprintf(sub, sizeof(sub), "%s/%s", dir, entry->d_name);
        struct stat st;
        if (stat(sub, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            collect_sources_in_dir(sub, files, count, cap);
        } else {
            const char *name = entry->d_name;
            size_t len = strlen(name);
            if (len > 2 && (strcmp(name + len - 2, ".c") == 0)) {
                if (*count >= *cap) {
                    *cap = *cap ? *cap * 2 : 64;
                    *files = realloc(*files, *cap * sizeof(char *));
                }
                (*files)[*count] = malloc(1024);
                snprintf((*files)[*count], 1024, "%s/%s", dir, name);
                (*count)++;
            }
        }
    }
    closedir(d);
}

int deps_find_sources(const manifest_t *m, const char *dep_name, char ***files, size_t *count) {
    (void)m;
    *files = NULL;
    *count = 0;
    size_t cap = 0;

    char dep_dir[1024];
    snprintf(dep_dir, sizeof(dep_dir), "cpkg_modules/%s", dep_name);

    if (!dir_exists(dep_dir)) return 0;

    collect_sources_in_dir(dep_dir, files, count, &cap);
    return 0;
}
