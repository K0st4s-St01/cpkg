#include "manifest.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int manifest_load(const char *path, manifest_t *m) {
    memset(m, 0, sizeof(*m));

    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "error: cannot open %s\n", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0) { fclose(f); return -1; }
    char *content = malloc(fsize + 1);
    fread(content, 1, fsize, f);
    content[fsize] = '\0';
    fclose(f);

    json_val_t *root = json_parse(content);
    free(content);
    if (!root || root->type != JSON_OBJ) {
        json_free(root);
        fprintf(stderr, "error: invalid cpkg.json\n");
        return -1;
    }

    const char *name = json_get_str(root, "name");
    if (!name) { json_free(root); fprintf(stderr, "error: missing 'name' in cpkg.json\n"); return -1; }
    m->name = strdup(name);

    const char *ver = json_get_str(root, "version");
    m->version = ver ? strdup(ver) : strdup("0.1.0");

    const char *cc = json_get_str(root, "compiler");
    m->compiler = cc ? strdup(cc) : strdup("gcc");

    const char *out = json_get_str(root, "output");
    m->output = out ? strdup(out) : NULL;

    json_val_t *src_arr = json_get(root, "source");
    if (src_arr && src_arr->type == JSON_ARR) {
        m->num_source_dirs = src_arr->arr.len;
        m->source_dirs = calloc(m->num_source_dirs, sizeof(char *));
        for (size_t i = 0; i < m->num_source_dirs; i++)
            if (src_arr->arr.items[i]->type == JSON_STR)
                m->source_dirs[i] = strdup(src_arr->arr.items[i]->str_val);
    } else {
        m->num_source_dirs = 1;
        m->source_dirs = calloc(1, sizeof(char *));
        m->source_dirs[0] = strdup("src");
    }

    json_val_t *inc_arr = json_get(root, "include");
    if (inc_arr && inc_arr->type == JSON_ARR) {
        m->num_include_dirs = inc_arr->arr.len;
        m->include_dirs = calloc(m->num_include_dirs, sizeof(char *));
        for (size_t i = 0; i < m->num_include_dirs; i++)
            if (inc_arr->arr.items[i]->type == JSON_STR)
                m->include_dirs[i] = strdup(inc_arr->arr.items[i]->str_val);
    }

    json_val_t *libs_arr = json_get(root, "libs");
    if (libs_arr && libs_arr->type == JSON_ARR) {
        m->num_libs = libs_arr->arr.len;
        m->libs = calloc(m->num_libs, sizeof(char *));
        for (size_t i = 0; i < m->num_libs; i++)
            if (libs_arr->arr.items[i]->type == JSON_STR)
                m->libs[i] = strdup(libs_arr->arr.items[i]->str_val);
    }

    json_val_t *cflags_arr = json_get(root, "cflags");
    if (cflags_arr && cflags_arr->type == JSON_ARR) {
        m->num_cflags = cflags_arr->arr.len;
        m->cflags = calloc(m->num_cflags, sizeof(char *));
        for (size_t i = 0; i < m->num_cflags; i++)
            if (cflags_arr->arr.items[i]->type == JSON_STR)
                m->cflags[i] = strdup(cflags_arr->arr.items[i]->str_val);
    }

    json_val_t *defines_arr = json_get(root, "defines");
    if (defines_arr && defines_arr->type == JSON_ARR) {
        m->num_defines = defines_arr->arr.len;
        m->defines = calloc(m->num_defines, sizeof(char *));
        for (size_t i = 0; i < m->num_defines; i++)
            if (defines_arr->arr.items[i]->type == JSON_STR)
                m->defines[i] = strdup(defines_arr->arr.items[i]->str_val);
    }

    json_val_t *deps_obj = json_get(root, "dependencies");
    if (deps_obj && deps_obj->type == JSON_OBJ) {
        m->num_deps = deps_obj->obj.len;
        m->dep_names = calloc(m->num_deps, sizeof(char *));
        m->dep_urls = calloc(m->num_deps, sizeof(char *));
        m->dep_versions = calloc(m->num_deps, sizeof(char *));
        for (size_t i = 0; i < m->num_deps; i++) {
            m->dep_names[i] = strdup(deps_obj->obj.pairs[i].key);
            json_val_t *dep_val = deps_obj->obj.pairs[i].val;
            if (dep_val->type == JSON_STR) {
                m->dep_urls[i] = strdup(dep_val->str_val);
            } else if (dep_val->type == JSON_OBJ) {
                const char *url = json_get_str(dep_val, "git");
                if (!url) url = json_get_str(dep_val, "url");
                m->dep_urls[i] = url ? strdup(url) : NULL;
                const char *dv = json_get_str(dep_val, "version");
                m->dep_versions[i] = dv ? strdup(dv) : NULL;
            }
        }
    }

    json_free(root);
    return 0;
}

int manifest_save(const char *path, const manifest_t *m) {
    FILE *f = fopen(path, "w");
    if (!f) { perror("fopen"); return -1; }

    int need_comma = 0;

    fprintf(f, "{\n");

#define PRINT_FIELD(fmt, ...) do { \
        if (need_comma) fprintf(f, ",\n"); \
        fprintf(f, fmt, ##__VA_ARGS__); \
        need_comma = 1; \
    } while(0)

    PRINT_FIELD("  \"name\": \"%s\"", m->name ? m->name : "");
    PRINT_FIELD("  \"version\": \"%s\"", m->version ? m->version : "0.1.0");
    PRINT_FIELD("  \"compiler\": \"%s\"", m->compiler ? m->compiler : "gcc");

    if (m->output) PRINT_FIELD("  \"output\": \"%s\"", m->output);

    if (m->num_source_dirs > 0) {
        fputs(need_comma ? ",\n  \"source\": [" : "  \"source\": [", f);
        need_comma = 1;
        for (size_t i = 0; i < m->num_source_dirs; i++) {
            if (i > 0) fputc(',', f);
            fprintf(f, "\"%s\"", m->source_dirs[i]);
        }
        fputc(']', f);
    }

    if (m->num_include_dirs > 0) {
        fputs(need_comma ? ",\n  \"include\": [" : "  \"include\": [", f);
        need_comma = 1;
        for (size_t i = 0; i < m->num_include_dirs; i++) {
            if (i > 0) fputc(',', f);
            fprintf(f, "\"%s\"", m->include_dirs[i]);
        }
        fputc(']', f);
    }

    if (m->num_libs > 0) {
        fputs(need_comma ? ",\n  \"libs\": [" : "  \"libs\": [", f);
        need_comma = 1;
        for (size_t i = 0; i < m->num_libs; i++) {
            if (i > 0) fputc(',', f);
            fprintf(f, "\"%s\"", m->libs[i]);
        }
        fputc(']', f);
    }

    if (m->num_cflags > 0) {
        fputs(need_comma ? ",\n  \"cflags\": [" : "  \"cflags\": [", f);
        need_comma = 1;
        for (size_t i = 0; i < m->num_cflags; i++) {
            if (i > 0) fputc(',', f);
            fprintf(f, "\"%s\"", m->cflags[i]);
        }
        fputc(']', f);
    }

    if (m->num_defines > 0) {
        fputs(need_comma ? ",\n  \"defines\": [" : "  \"defines\": [", f);
        need_comma = 1;
        for (size_t i = 0; i < m->num_defines; i++) {
            if (i > 0) fputc(',', f);
            fprintf(f, "\"%s\"", m->defines[i]);
        }
        fputc(']', f);
    }

    if (m->num_deps > 0) {
        if (need_comma) fputs(",", f);
        fprintf(f, "\n  \"dependencies\": {\n");
        for (size_t i = 0; i < m->num_deps; i++) {
            fprintf(f, "    \"%s\": {", m->dep_names[i]);
            if (m->dep_urls[i]) fprintf(f, "\"git\": \"%s\"", m->dep_urls[i]);
            if (m->dep_versions[i]) fprintf(f, ", \"version\": \"%s\"", m->dep_versions[i]);
            fprintf(f, "}");
            if (i < m->num_deps - 1) fprintf(f, ",");
            fprintf(f, "\n");
        }
        fprintf(f, "  }");
        need_comma = 1;
    }

#undef PRINT_FIELD

    fputs("\n}\n", f);
    fclose(f);
    return 0;
}

int manifest_init(const char *name, const char *version) {
    manifest_t m;
    memset(&m, 0, sizeof(m));
    m.name = strdup(name);
    m.version = strdup(version ? version : "0.1.0");
    m.compiler = strdup("gcc");
    m.num_source_dirs = 1;
    m.source_dirs = calloc(1, sizeof(char *));
    m.source_dirs[0] = strdup("src");
    m.num_include_dirs = 1;
    m.include_dirs = calloc(1, sizeof(char *));
    m.include_dirs[0] = strdup("include");
    int ret = manifest_save("cpkg.json", &m);
    manifest_free(&m);
    return ret;
}

void manifest_free(manifest_t *m) {
    free(m->name);
    free(m->version);
    free(m->compiler);
    free(m->output);
    for (size_t i = 0; i < m->num_source_dirs; i++) free(m->source_dirs[i]);
    free(m->source_dirs);
    for (size_t i = 0; i < m->num_include_dirs; i++) free(m->include_dirs[i]);
    free(m->include_dirs);
    for (size_t i = 0; i < m->num_libs; i++) free(m->libs[i]);
    free(m->libs);
    for (size_t i = 0; i < m->num_cflags; i++) free(m->cflags[i]);
    free(m->cflags);
    for (size_t i = 0; i < m->num_defines; i++) free(m->defines[i]);
    free(m->defines);
    for (size_t i = 0; i < m->num_deps; i++) {
        free(m->dep_names[i]);
        free(m->dep_urls[i]);
        free(m->dep_versions[i]);
    }
    free(m->dep_names);
    free(m->dep_urls);
    free(m->dep_versions);
    memset(m, 0, sizeof(*m));
}
