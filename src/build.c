#include "build.h"
#include "deps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#define MAX_CMD 65536

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

static void build_cflags(const manifest_t *m, char *buf, size_t buf_size) {
    buf[0] = '\0';
    size_t pos = 0;

    for (size_t i = 0; i < m->num_include_dirs; i++) {
        pos += snprintf(buf + pos, buf_size - pos, "-I%s ", m->include_dirs[i]);
        if (pos >= buf_size) return;
    }

    for (size_t i = 0; i < m->num_cflags; i++) {
        pos += snprintf(buf + pos, buf_size - pos, "%s ", m->cflags[i]);
        if (pos >= buf_size) return;
    }

    for (size_t i = 0; i < m->num_defines; i++) {
        pos += snprintf(buf + pos, buf_size - pos, "-D%s ", m->defines[i]);
        if (pos >= buf_size) return;
    }

    for (size_t i = 0; i < m->num_deps; i++) {
        char dep_inc[1024];
        snprintf(dep_inc, sizeof(dep_inc), "cpkg_modules/%s", m->dep_names[i]);
        if (dir_exists(dep_inc)) {
            pos += snprintf(buf + pos, buf_size - pos, "-I%s ", dep_inc);
            if (pos >= buf_size) return;
        }
    }
}

static int compile_file(const manifest_t *m, const char *src_file, const char *obj_file, const char *cflags_str) {
    char cmd[MAX_CMD];
    snprintf(cmd, sizeof(cmd), "%s -c %s %s-o %s 2>&1", m->compiler, src_file, cflags_str, obj_file);

    printf("  cc %s\n", src_file);

    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;

    char line[4096];
    int status = 0;
    while (fgets(line, sizeof(line), fp)) {
        fprintf(stderr, "%s", line);
        status = 1;
    }
    int ret = pclose(fp);
    if (ret != 0 || status != 0) return -1;
    return 0;
}

int build_project(const manifest_t *m, int run_after, int argc, char **argv) {
    if (!dir_exists("build")) {
        if (mkdir("build", 0755) != 0) {
            perror("mkdir build");
            return -1;
        }
    }

    fflush(stdout);
    printf("building '%s' v%s ...\n", m->name, m->version ? m->version : "?");

    char cflags_str[MAX_CMD];
    build_cflags(m, cflags_str, sizeof(cflags_str));

    size_t src_count = 0, src_cap = 0;
    char **src_files = NULL;

    for (size_t i = 0; i < m->num_source_dirs; i++) {
        char dir[1024];
        snprintf(dir, sizeof(dir), "%s", m->source_dirs[i]);
        if (strncmp(dir, "./", 2) == 0) memmove(dir, dir + 2, strlen(dir));

        if (dir_exists(dir)) {
            collect_sources_in_dir(dir, &src_files, &src_count, &src_cap);
        } else {
            fprintf(stderr, "warning: source directory '%s' not found\n", dir);
        }
    }

    for (size_t i = 0; i < m->num_deps; i++) {
        size_t dep_count = 0;
        char **dep_files = NULL;
        deps_find_sources(m, m->dep_names[i], &dep_files, &dep_count);
        for (size_t j = 0; j < dep_count; j++) {
            if (src_count >= src_cap) {
                src_cap = src_cap ? src_cap * 2 : 128;
                src_files = realloc(src_files, src_cap * sizeof(char *));
            }
            src_files[src_count++] = dep_files[j];
        }
        free(dep_files);
    }

    if (src_count == 0) {
        fprintf(stderr, "error: no source files found\n");
        return -1;
    }

    size_t obj_count = 0;
    char **obj_files = calloc(src_count, sizeof(char *));

    for (size_t i = 0; i < src_count; i++) {
        char obj_name[1024];
        const char *fname = strrchr(src_files[i], '/');
        fname = fname ? fname + 1 : src_files[i];

        snprintf(obj_name, sizeof(obj_name), "build/%s.o", fname);
        obj_files[obj_count] = strdup(obj_name);

        struct stat src_st, obj_st;
        int needs_compile = 1;
        if (stat(src_files[i], &src_st) == 0 && stat(obj_name, &obj_st) == 0) {
            if (src_st.st_mtime <= obj_st.st_mtime) {
                needs_compile = 0;
            }
        }

        if (needs_compile) {
            if (compile_file(m, src_files[i], obj_name, cflags_str) != 0) {
                fprintf(stderr, "error: compilation failed for %s\n", src_files[i]);
                for (size_t j = 0; j < obj_count; j++) free(obj_files[j]);
                free(obj_files);
                for (size_t j = 0; j < src_count; j++) free(src_files[j]);
                free(src_files);
                return -1;
            }
        } else {
            printf("  (skipped %s - up to date)\n", src_files[i]);
        }
        obj_count++;
    }

    char output_name[1024];
    if (m->output) {
        snprintf(output_name, sizeof(output_name), "%s", m->output);
    } else {
        snprintf(output_name, sizeof(output_name), "build/%s", m->name);
    }

    fflush(stdout);
    printf("  linking %s ...\n", output_name);
    char cmd[MAX_CMD];
    size_t pos = snprintf(cmd, sizeof(cmd), "%s -o %s", m->compiler, output_name);

    for (size_t i = 0; i < obj_count; i++) {
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " %s", obj_files[i]);
        if (pos >= sizeof(cmd)) break;
    }

    for (size_t i = 0; i < m->num_libs; i++) {
        pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -l%s", m->libs[i]);
        if (pos >= sizeof(cmd)) break;
    }

    pos += snprintf(cmd + pos, sizeof(cmd) - pos, " -Lcpkg_modules 2>&1");

    int link_ret = system(cmd);
    if (link_ret != 0) {
        fprintf(stderr, "error: linking failed\n");
        for (size_t j = 0; j < obj_count; j++) free(obj_files[j]);
        free(obj_files);
        for (size_t j = 0; j < src_count; j++) free(src_files[j]);
        free(src_files);
        return -1;
    }

    for (size_t j = 0; j < obj_count; j++) free(obj_files[j]);
    free(obj_files);
    for (size_t j = 0; j < src_count; j++) free(src_files[j]);
    free(src_files);

    printf("build complete: %s\n", output_name);

    if (run_after) {
        fflush(stdout);
        printf("running '%s' ...\n", output_name);
        char run_cmd[MAX_CMD];
        pos = snprintf(run_cmd, sizeof(run_cmd), "./%s", output_name);
        for (int i = 0; i < argc; i++) {
            pos += snprintf(run_cmd + pos, sizeof(run_cmd) - pos, " %s", argv[i]);
        }
        int run_ret = system(run_cmd);
        (void)run_ret;
    }

    return 0;
}
