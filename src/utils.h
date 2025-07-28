#ifndef UTILS_H_
#define UTILS_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * Find the last path separator in a string (either '/' or '\' on Windows).
 * @param path The input path string to search (can be NULL)
 * @return Pointer to the last path separator, or NULL if none found or path is NULL
 */
const char *findLastPathSeparator(const char *path);

/**
 * Extract base name prefix from a filename based on output directory settings
 * @param filename The input filename to process (must not be NULL)
 * @param logNameEnd Pointer to the end of the log name (must not be NULL and >= filename)
 * @param hasOutputDir Whether an output directory is specified
 * @param outBaseNamePrefix Pointer to store the base name prefix (can be NULL)
 * @param outBaseNamePrefixLen Pointer to store the base name prefix length (can be NULL)
 * @param outOutputPrefix Pointer to store the output prefix (can be NULL)
 * @param outOutputPrefixLen Pointer to store the output prefix length (can be NULL)
 * @note If invalid input is provided, all output parameters are set to NULL/0 and function returns early
 */
void extractBaseNamePrefix(const char *filename, const char *logNameEnd, bool hasOutputDir,
                          const char **outBaseNamePrefix, int *outBaseNamePrefixLen,
                          const char **outOutputPrefix, int *outOutputPrefixLen);

#endif
