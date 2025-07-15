/*
 * semver.h
 *
 * Copyright (c) 2015-2017 Tomas Aparicio
 * MIT licensed
 *
 * Adapted for blackbox-tools
 */

#ifndef SEMVER_H_
#define SEMVER_H_

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef SEMVER_VERSION
#define SEMVER_VERSION "0.2.0"
#endif

/**
 * semver_t struct
 */
typedef struct semver_version_s {
  int major;
  int minor;
  int patch;
  char * metadata;
  char * prerelease;
} semver_t;

/**
 * Function prototypes
 */

/* Parsing functions */
int semver_parse(const char *str, semver_t *ver);
int semver_parse_version(const char *str, semver_t *ver);

/* Comparison functions */
int semver_compare(semver_t x, semver_t y);
int semver_compare_version(semver_t x, semver_t y);
int semver_compare_prerelease(semver_t x, semver_t y);

/* Comparison operators */
int semver_gt(semver_t x, semver_t y);
int semver_gte(semver_t x, semver_t y);
int semver_lt(semver_t x, semver_t y);
int semver_lte(semver_t x, semver_t y);
int semver_eq(semver_t x, semver_t y);
int semver_neq(semver_t x, semver_t y);

/* Range satisfaction */
int semver_satisfies(semver_t x, semver_t y, const char *op);
int semver_satisfies_caret(semver_t x, semver_t y);
int semver_satisfies_patch(semver_t x, semver_t y);

/* Utility functions */
int semver_is_valid(const char *s);
int semver_clean(char *s);
void semver_render(semver_t *x, char *dest);
int semver_numeric(semver_t *x);

/* Version bump functions */
void semver_bump(semver_t *x);
void semver_bump_minor(semver_t *x);
void semver_bump_patch(semver_t *x);

/* Memory management */
void semver_free(semver_t *x);

/* Convenience functions for blackbox-tools */
int semver_parse_string(const char *version_str, semver_t *ver);
int semver_compare_strings(const char *version1, const char *version2);
int semver_gte_string(const char *currentVersion, const char *targetVersion);

#ifdef __cplusplus
}
#endif

#endif
