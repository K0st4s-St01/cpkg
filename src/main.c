#include "manifest.h"
#include "build.h"
#include "deps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define CPKG_VERSION "0.1.0"
#define CPKG_PATH_MAX 4096

static void print_usage(const char *prog) {
    fprintf(stderr, "cpkg v%s - C Package Manager\n", CPKG_VERSION);
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "  %s init <name>        create a new project\n", prog);
    fprintf(stderr, "  %s build              compile and link the project\n", prog);
    fprintf(stderr, "  %s run [args...]      build and run the project\n", prog);
    fprintf(stderr, "  %s add <git-url>      add a dependency\n", prog);
    fprintf(stderr, "  %s install            install all dependencies\n", prog);
    fprintf(stderr, "  %s project install    build and install the project\n", prog);
    fprintf(stderr, "  %s help               show this help\n", prog);
}

static int mkdir_p(const char *path) {
    char tmp[CPKG_PATH_MAX];
    if (snprintf(tmp, sizeof(tmp), "%s", path) >= (int)sizeof(tmp)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    size_t len = strlen(tmp);
    if (len == 0) return -1;
    if (len > 1 && tmp[len - 1] == '/') tmp[len - 1] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) {
        fprintf(stderr, "error: cannot open '%s'\n", src);
        return -1;
    }

    FILE *out = fopen(dst, "wb");
    if (!out) {
        fprintf(stderr, "error: cannot write '%s'\n", dst);
        fclose(in);
        return -1;
    }

    char buf[8192];
    size_t n;
    int ret = 0;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fprintf(stderr, "error: failed writing '%s'\n", dst);
            ret = -1;
            break;
        }
    }

    if (ferror(in)) {
        fprintf(stderr, "error: failed reading '%s'\n", src);
        ret = -1;
    }

    if (fclose(out) != 0) {
        fprintf(stderr, "error: failed closing '%s'\n", dst);
        ret = -1;
    }
    fclose(in);

    if (ret == 0 && chmod(dst, 0755) != 0) {
        fprintf(stderr, "error: failed to mark '%s' executable\n", dst);
        ret = -1;
    }

    return ret;
}

static const char *path_basename(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int project_install(const char *prog, int argc, char **argv) {
    const char *prefix = getenv("CPKG_PREFIX");
    if (!prefix || !prefix[0]) prefix = getenv("PREFIX");
    if (!prefix || !prefix[0]) prefix = "/usr/local";

    const char *destdir = getenv("DESTDIR");
    if (!destdir) destdir = "";

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--prefix") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "usage: %s project install [--prefix <dir>] [--destdir <dir>]\n", prog);
                return 1;
            }
            prefix = argv[++i];
        } else if (strcmp(argv[i], "--destdir") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "usage: %s project install [--prefix <dir>] [--destdir <dir>]\n", prog);
                return 1;
            }
            destdir = argv[++i];
        } else {
            fprintf(stderr, "unknown option for project install: %s\n", argv[i]);
            fprintf(stderr, "usage: %s project install [--prefix <dir>] [--destdir <dir>]\n", prog);
            return 1;
        }
    }

    manifest_t m;
    if (manifest_load("cpkg.json", &m) != 0) return 1;

    if (deps_install_all(&m) != 0) {
        manifest_free(&m);
        return 1;
    }

    if (build_project(&m, 0, 0, NULL) != 0) {
        manifest_free(&m);
        return 1;
    }

    char output_name[CPKG_PATH_MAX];
    int n;
    if (m.output) {
        n = snprintf(output_name, sizeof(output_name), "%s", m.output);
    } else {
        n = snprintf(output_name, sizeof(output_name), "build/%s", m.name);
    }
    if (n < 0 || n >= (int)sizeof(output_name)) {
        fprintf(stderr, "error: project output path is too long\n");
        manifest_free(&m);
        return 1;
    }

    char bin_dir[CPKG_PATH_MAX];
    n = snprintf(bin_dir, sizeof(bin_dir), "%s%s/bin", destdir, prefix);
    if (n < 0 || n >= (int)sizeof(bin_dir)) {
        fprintf(stderr, "error: install directory path is too long\n");
        manifest_free(&m);
        return 1;
    }
    if (mkdir_p(bin_dir) != 0) {
        fprintf(stderr, "error: failed to create install directory '%s'\n", bin_dir);
        manifest_free(&m);
        return 1;
    }

    char install_path[CPKG_PATH_MAX];
    n = snprintf(install_path, sizeof(install_path), "%s/%s", bin_dir, path_basename(output_name));
    if (n < 0 || n >= (int)sizeof(install_path)) {
        fprintf(stderr, "error: install path is too long\n");
        manifest_free(&m);
        return 1;
    }
    if (copy_file(output_name, install_path) != 0) {
        manifest_free(&m);
        return 1;
    }

    printf("installed project '%s' to %s\n", m.name, install_path);
    manifest_free(&m);
    return 0;
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

    } else if (strcmp(cmd, "project") == 0) {
        if (argc >= 3 && strcmp(argv[2], "install") == 0) {
            return project_install(argv[0], argc - 3, argv + 3);
        }
        fprintf(stderr, "usage: %s project install [--prefix <dir>] [--destdir <dir>]\n", argv[0]);
        return 1;

    } else {
        fprintf(stderr, "unknown command: %s\n", cmd);
        print_usage(argv[0]);
        return 1;
    }
}
