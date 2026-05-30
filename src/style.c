#include "style.h"

#include <string.h>

/* Resolve a component or preset name to its bits. Returns false if unknown;
 * *is_preset says whether the name set an absolute base (preset) vs a single
 * component to add/remove. */
static bool resolve(const char *name, unsigned *bits, bool *is_preset)
{
    *is_preset = false;
    if (strcmp(name, "plain") == 0) {
        *bits = 0;
        *is_preset = true;
    } else if (strcmp(name, "full") == 0) {
        *bits = MAT_STYLE_FULL;
        *is_preset = true;
    } else if (strcmp(name, "default") == 0 || strcmp(name, "auto") == 0) {
        *bits = MAT_STYLE_DEFAULT;
        *is_preset = true;
    } else if (strcmp(name, "numbers") == 0) {
        *bits = MAT_S_NUMBERS;
    } else if (strcmp(name, "grid") == 0) {
        *bits = MAT_S_GRID;
    } else if (strcmp(name, "header") == 0 ||
               strcmp(name, "header-filename") == 0) {
        *bits = MAT_S_HEADER;
    } else if (strcmp(name, "header-filesize") == 0) {
        *bits = MAT_S_HEADER_SIZE;
    } else if (strcmp(name, "rule") == 0) {
        *bits = MAT_S_RULE;
    } else if (strcmp(name, "snip") == 0) {
        *bits = MAT_S_SNIP;
    } else {
        return false;
    }
    return true;
}

int mat_style_parse(const char *spec, unsigned *out, char *errbuf,
                    size_t errlen)
{
    char copy[256];
    size_t n = strlen(spec);
    if (n >= sizeof copy)
        n = sizeof copy - 1;
    memcpy(copy, spec, n);
    copy[n] = '\0';

    unsigned set = 0;
    bool first = true;
    char *save = NULL;
    for (char *tok = strtok_r(copy, ",", &save); tok != NULL;
         tok = strtok_r(NULL, ",", &save)) {
        int op = 0; /* 0 = set/add, +1 = add, -1 = remove */
        if (*tok == '+') {
            op = 1;
            tok++;
        } else if (*tok == '-') {
            op = -1;
            tok++;
        }

        /* A leading +/- modifies the default; a bare first token replaces. */
        if (first && op != 0)
            set = MAT_STYLE_DEFAULT;
        first = false;

        unsigned bits;
        bool is_preset;
        if (!resolve(tok, &bits, &is_preset)) {
            if (errbuf && errlen) {
                size_t tl = strlen(tok);
                if (tl >= errlen)
                    tl = errlen - 1;
                memcpy(errbuf, tok, tl);
                errbuf[tl] = '\0';
            }
            return -1;
        }

        if (is_preset)
            set = bits; /* presets are absolute */
        else if (op == -1)
            set &= ~bits;
        else
            set |= bits;
    }

    *out = set;
    return 0;
}
