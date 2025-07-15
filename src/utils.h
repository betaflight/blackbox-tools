#ifndef UTILS_H_
#define UTILS_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * Find the last path separator in a string (either '/' or '\' on Windows).
 * Returns a pointer to the last path separator, or NULL if none found.
 */
const char *findLastPathSeparator(const char *path);

/**
 * Extract base name prefix from a filename based on output directory settings
 * @param filename The input filename to process
 * @param logNameEnd Pointer to the end of the log name (before extension)
 * @param hasOutputDir Whether an output directory is specified
 * @param outBaseNamePrefix Pointer to store the base name prefix
 * @param outBaseNamePrefixLen Pointer to store the base name prefix length
 * @param outOutputPrefix Pointer to store the output prefix
 * @param outOutputPrefixLen Pointer to store the output prefix length
 */
void extractBaseNamePrefix(const char *filename, const char *logNameEnd, bool hasOutputDir,
                          const char **outBaseNamePrefix, int *outBaseNamePrefixLen,
                          const char **outOutputPrefix, int *outOutputPrefixLen);

#endif
