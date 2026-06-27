#include "manifest.h"
#include "build.h"
#include "deps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define CPKG_VERSION "0.1.0"

static void print_usage(const char *prog) {
    fprintf(stderr, "cpkg v%s - C Package Manager\n", CPKG_VERSION);
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "  %s init <name>        create a new project\n", prog);
    fprintf(stderr, "  %s build              compile and link the project\n", prog);
    fprintf(stderr, "  %s run [args...]      build and run the project\n", prog);
    fprintf(stderr, "  %s add <git-url>      add a dependency\n", prog);
    fprintf(stderr, "  %s install            install all dependencies\n", prog);
    fprintf(stderr, "  %s help               show this help\n", prog);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0 || strcmp(cmd, "help") == 0) {
        print_usage(argv[0]);
        return 0;
    }
    if (strcmp(cmd, "--version") == 0 || strcmp(cmd, "-v") == 0) {
        printf("cpkg version %s\n", CPKG_VERSION);
        return 0;
    }

    if (strcmp(cmd, "init") == 0) {
        if (argc < 3) {
            fprintf(stderr, "usage: %s init <project-name>\n", argv[0]);
            return 1;
        }
        const char *name = argv[2];
        const char *version = argc > 3 ? argv[3] : NULL;

        if (mkdir(name, 0755) != 0) {
            fprintf(stderr, "error: failed to create directory '%s'\n", name);
            return 1;
        }

        if (chdir(name) != 0) {
            fprintf(stderr, "error: failed to enter directory '%s'\n", name);
            return 1;
        }

        if (manifest_init(name, version) != 0) {
            fprintf(stderr, "error: failed to initialize project\n");
            return 1;
        }

        mkdir("src", 0755);
        mkdir("include", 0755);

        char main_c[1024];
        snprintf(main_c, sizeof(main_c), "src/main.c");
        FILE *f = fopen(main_c, "w");
        if (f) {
            fprintf(f, "#include <stdio.h>\n\n");
            fprintf(f, "int main(int argc, char **argv) {\n");
            fprintf(f, "    printf(\"Hello from %s!\\n\");\n", name);
            fprintf(f, "    return 0;\n");
            fprintf(f, "}\n");
            fclose(f);
        }

        printf("created project '%s'\n", name);
        printf("  cpkg.json   - project manifest\n");
        printf("  src/main.c  - entry point\n");
        printf("  include/    - header directory\n");
        return 0;

    } else if (strcmp(cmd, "build") == 0) {
        manifest_t m;
        if (manifest_load("cpkg.json", &m) != 0) return 1;

        if (deps_install_all(&m) != 0) {
            manifest_free(&m);
            return 1;
        }

        int ret = build_project(&m, 0, 0, NULL);
        manifest_free(&m);
        return ret != 0 ? 1 : 0;

    } else if (strcmp(cmd, "run") == 0) {
        manifest_t m;
        if (manifest_load("cpkg.json", &m) != 0) return 1;

        if (deps_install_all(&m) != 0) {
            manifest_free(&m);
            return 1;
        }

        int ret = build_project(&m, 1, argc - 2, argv + 2);
        manifest_free(&m);
        return ret != 0 ? 1 : 0;

    } else if (strcmp(cmd, "add") == 0) {
        if (argc < 3) {
            fprintf(stderr, "usage: %s add <git-url> [version]\n", argv[0]);
            return 1;
        }
        const char *url = argv[2];
        const char *version = argc > 3 ? argv[3] : NULL;

        manifest_t m;
        if (manifest_load("cpkg.json", &m) != 0) return 1;

        const char *name_start = strrchr(url, '/');
        char dep_name[256];
        if (name_start) {
            const char *p = name_start + 1;
            char tmp[256];
            size_t j = 0;
            while (*p && *p != '.' && *p != '#' && *p != '?' && j < sizeof(tmp) - 1)
                tmp[j++] = *p++;
            tmp[j] = '\0';
            if (j > 0) {
                snprintf(dep_name, sizeof(dep_name), "%s", tmp);
            } else {
                snprintf(dep_name, sizeof(dep_name), "dep%zu", m.num_deps);
            }
        } else {
            snprintf(dep_name, sizeof(dep_name), "dep%zu", m.num_deps);
        }

        for (size_t i = 0; i < m.num_deps; i++) {
            if (strcmp(m.dep_names[i], dep_name) == 0) {
                fprintf(stderr, "error: dependency '%s' already exists\n", dep_name);
                manifest_free(&m);
                return 1;
            }
        }

        m.num_deps++;
        m.dep_names = realloc(m.dep_names, m.num_deps * sizeof(char *));
        m.dep_urls = realloc(m.dep_urls, m.num_deps * sizeof(char *));
        m.dep_versions = realloc(m.dep_versions, m.num_deps * sizeof(char *));
        m.dep_names[m.num_deps - 1] = strdup(dep_name);
        m.dep_urls[m.num_deps - 1] = strdup(url);
        m.dep_versions[m.num_deps - 1] = version ? strdup(version) : NULL;

        if (manifest_save("cpkg.json", &m) != 0) {
            manifest_free(&m);
            return 1;
        }

        if (deps_install(&m, dep_name, url, version) != 0) {
            manifest_free(&m);
            return 1;
        }

        manifest_free(&m);
        printf("added dependency '%s'\n", dep_name);
        return 0;

    } else if (strcmp(cmd, "install") == 0) {
        manifest_t m;
        if (manifest_load("cpkg.json", &m) != 0) return 1;
        int ret = deps_install_all(&m);
        manifest_free(&m);
        return ret != 0 ? 1 : 0;

    } else {
        fprintf(stderr, "unknown command: %s\n", cmd);
        print_usage(argv[0]);
        return 1;
    }
}
