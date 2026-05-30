/*
 * style.h — parse a --style spec into a decoration component bitset.
 */
#ifndef MAT_STYLE_H
#define MAT_STYLE_H

#include "config.h"

/*
 * Parse specs like "full", "plain", "numbers,grid", "full,-header-filesize",
 * or "+numbers" (a leading +/- modifies the default set; otherwise the set is
 * built from scratch). Writes the resulting bitset to *out.
 * Returns 0 on success, -1 on an unknown component (name copied to errbuf).
 */
int mat_style_parse(const char *spec, unsigned *out, char *errbuf,
                    size_t errlen);

#endif /* MAT_STYLE_H */
