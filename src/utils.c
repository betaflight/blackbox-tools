#include "utils.h"
#include <string.h>

/**
 * Find the last path separator in a string (either '/' or '\' on Windows).
 * Returns a pointer to the last path separator, or NULL if none found.
 */
const char *findLastPathSeparator(const char *path)
{
    const char *lastSlash = strrchr(path, '/');
#ifdef WIN32
    const char *lastBackslash = strrchr(path, '\\');
    if (lastBackslash && (!lastSlash || lastBackslash > lastSlash)) {
        lastSlash = lastBackslash;
    }
#endif
    return lastSlash;
}

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
                          const char **outOutputPrefix, int *outOutputPrefixLen) {
    const char *lastSlash;
    if (hasOutputDir) {
        lastSlash = findLastPathSeparator(filename);
        if (lastSlash) {
            if (outBaseNamePrefix) *outBaseNamePrefix = lastSlash + 1;
            if (outBaseNamePrefixLen) *outBaseNamePrefixLen = logNameEnd - (lastSlash + 1);
        } else {
            if (outBaseNamePrefix) *outBaseNamePrefix = filename;
            if (outBaseNamePrefixLen) *outBaseNamePrefixLen = logNameEnd - filename;
        }
        if (outOutputPrefix) *outOutputPrefix = NULL;
        if (outOutputPrefixLen) *outOutputPrefixLen = 0;
    } else {
        if (outOutputPrefix) *outOutputPrefix = filename;
        if (outOutputPrefixLen) *outOutputPrefixLen = logNameEnd - filename;
        if (outBaseNamePrefix) *outBaseNamePrefix = filename;
        if (outBaseNamePrefixLen) *outBaseNamePrefixLen = logNameEnd - filename;
    }
}
