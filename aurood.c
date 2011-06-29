#define _GNU_SOURCE
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <alpm.h>

static alpm_list_t *alpm_find_foreign_packages(pmhandle_t*);
static pmpkg_t *alpm_provides_pkg(pmhandle_t*, const char*);
static char *strtrim(char*);
static pmhandle_t *alpm_init(void);
static int alpm_pkg_is_foreign(pmhandle_t*, pmpkg_t*);

char *strtrim(char *str) {
  char *pch = str;

  if (!str || !*str) {
    return str;
  }

  while (isspace((unsigned char)*pch)) {
    pch++;
  }

  if (pch != str) {
    memmove(str, pch, strlen(pch) + 1);
  }

  if (!*str) {
    return str;
  }

  pch = str + strlen(str) - 1;
  while (isspace((unsigned char)*pch)) {
    pch--;
  }
  *++pch = 0;

  return str;
}

pmhandle_t *alpm_init() {
  pmhandle_t *handle;
  enum _pmerrno_t pmerr;
  FILE *fp;
  char line[PATH_MAX];
  char *ptr, *section = NULL;

  handle = alpm_initialize("/", "/var/lib/pacman", &pmerr);
  if (!handle) {
    fprintf(stderr, "failed to initialize alpm: %s\n", alpm_strerror(pmerr));
    return NULL;
  }

  fp = fopen("/etc/pacman.conf", "r");
  while (fgets(line, PATH_MAX, fp)) {
    strtrim(line);

    if (strlen(line) == 0 || line[0] == '#') {
      continue;
    }
    if ((ptr = strchr(line, '#'))) {
      *ptr = 0;
    }

    if (line[0] == '[' && line[strlen(line) - 1] == ']') {
      ptr = &line[1];
      if (section) {
        free(section);
      }

      section = strdup(ptr);
      section[strlen(section) - 1] = 0;

      if (strcmp(section, "options") != 0) {
        if (!alpm_db_register_sync(handle, section, PM_PGP_VERIFY_OPTIONAL)) {
          goto finish;
        }
      }
    }
  }

finish:
  free(section);
  fclose(fp);
  return handle;
}

int alpm_pkg_is_foreign(pmhandle_t *handle, pmpkg_t *pkg) {
  const char *pkgname = alpm_pkg_get_name(pkg);

  for (alpm_list_t *i = alpm_option_get_syncdbs(handle); i; i = alpm_list_next(i)) {
    if (alpm_db_get_pkg(alpm_list_getdata(i), pkgname)) {
      return 0;
    }
  }

  return 1;
}

alpm_list_t *alpm_find_foreign_packages(pmhandle_t *handle) {
  alpm_list_t *ret = NULL;
  pmdb_t *db_local = alpm_option_get_localdb(handle);

  for (alpm_list_t *i = alpm_db_get_pkgcache(db_local); i; i = alpm_list_next(i)) {
    pmpkg_t *pkg = alpm_list_getdata(i);

    if (alpm_pkg_is_foreign(handle, pkg)) {
      ret = alpm_list_add(ret, pkg);
    }
  }

  return ret;
}

pmpkg_t *alpm_provides_pkg(pmhandle_t *handle, const char *depstring) {

  for (alpm_list_t *i = alpm_option_get_syncdbs(handle); i; i = alpm_list_next(i)) {
    pmdb_t *db = alpm_list_getdata(i);
    pmpkg_t *pkg = alpm_find_satisfier(alpm_db_get_pkgcache(db), depstring);
    if (pkg) {
      return pkg;
    }
  }

  return NULL;
}

int main(void) {
  int ret = 0;
  pmhandle_t *handle;
  alpm_list_t *foreignpkgs;
  char *color_red, *color_green, *color_none;

  foreignpkgs = NULL;

  handle = alpm_init();
  if (!handle) {
    goto finish;
  }

  if (isatty(fileno(stdout))) {
    color_red = "\033[1;31m";
    color_green = "\033[1;32m";
    color_none = "\033[0m";
  } else {
    color_red = "";
    color_green = "";
    color_none = "";
  }

  foreignpkgs = alpm_find_foreign_packages(handle);

  for (alpm_list_t *i = foreignpkgs; i; i = alpm_list_next(i)) {
    pmpkg_t *pkg = alpm_list_getdata(i);

    for (alpm_list_t *j = alpm_pkg_get_provides(pkg); j; j = alpm_list_next(j)) {
      const char *provide, *provver, *pkgver;

      provide = alpm_list_getdata(j);
      pmpkg_t *provider = alpm_provides_pkg(handle, provide);
      if (!provider) {
        continue;
      }

      provver = alpm_pkg_get_version(provider);
      pkgver = alpm_pkg_get_version(pkg);
      if (alpm_pkg_vercmp(provver, pkgver) > 0) {
        /* make sure it's not just a pkgrel bump */
        if (strncmp(pkgver, provver, strrchr(pkgver, '-') - pkgver) == 0) {
          continue;
        }

        printf("%s [provides %s] %s%s%s -> %s%s%s\n",
            alpm_pkg_get_name(pkg), alpm_pkg_get_name(provider),
            color_red, pkgver, color_none,
            color_green, provver, color_none);
        ret++;
      }
    }
  }

  alpm_list_free(foreignpkgs);

finish:
  alpm_release(handle);
  return ret;
}
