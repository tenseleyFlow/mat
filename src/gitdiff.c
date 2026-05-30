#include "gitdiff.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ensure_cap(struct mat_changes *ch, size_t need)
{
    if (need < ch->cap)
        return;
    size_t nc = ch->cap ? ch->cap * 2 : 256;
    while (nc <= need)
        nc *= 2;
    enum mat_change *nb = realloc(ch->line, nc * sizeof *nb);
    if (nb == NULL)
        return;
    for (size_t i = ch->cap; i < nc; i++)
        nb[i] = MAT_CHG_NONE;
    ch->line = nb;
    ch->cap = nc;
}

static void set_line(struct mat_changes *ch, size_t L, enum mat_change c)
{
    if (L == 0)
        return;
    ensure_cap(ch, L + 1);
    if (L < ch->cap) {
        ch->line[L] = c;
        if (L > ch->nlines)
            ch->nlines = L;
    }
}

bool mat_changes_load(struct mat_changes *ch, const char *path)
{
    memset(ch, 0, sizeof *ch);
    if (path == NULL || path[0] == '\0')
        return false;

    char cmd[2048];
    int n = snprintf(cmd, sizeof cmd,
                     "git diff --no-color -U0 -- '%s' 2>/dev/null", path);
    if (n < 0 || (size_t)n >= sizeof cmd)
        return false;

    FILE *fp = popen(cmd, "r");
    if (fp == NULL)
        return false;

    char line[4096];
    size_t cur_new = 0, cur_count = 0, cur_old_count = 0, cur_idx = 0;
    while (fgets(line, (int)sizeof line, fp) != NULL) {
        if (line[0] != '@' || line[1] != '@')
            continue;
        /* Parse @@ -old,count +new,count @@ */
        const char *p = line + 4; /* skip "@@ -" */
        /* skip old range */
        long old_count = 1;
        while (*p && *p != ' ' && *p != ',')
            p++;
        if (*p == ',') {
            p++;
            old_count = strtol(p, NULL, 10);
            while (*p && *p != ' ')
                p++;
        }
        if (*p == ' ')
            p++; /* skip space */
        if (*p == '+')
            p++;
        long new_start = strtol(p, (char **)&p, 10);
        long new_count = 1;
        if (*p == ',') {
            p++;
            new_count = strtol(p, NULL, 10);
        }
        cur_new = (size_t)(new_start > 0 ? new_start : 1);
        cur_count = (size_t)(new_count > 0 ? new_count : 0);
        cur_old_count = (size_t)(old_count > 0 ? old_count : 0);
        cur_idx = 0;

        if (cur_count == 0 && cur_old_count > 0) {
            /* Pure deletion: mark the line after the deletion point. */
            set_line(ch, cur_new, MAT_CHG_REMOVED);
        } else {
            for (size_t i = 0; i < cur_count; i++) {
                enum mat_change c = (cur_old_count > 0 && i < cur_old_count)
                                        ? MAT_CHG_MODIFIED
                                        : MAT_CHG_ADDED;
                set_line(ch, cur_new + i, c);
            }
        }
        (void)cur_idx;
    }
    pclose(fp);
    return ch->nlines > 0;
}

void mat_changes_free(struct mat_changes *ch)
{
    free(ch->line);
    ch->line = NULL;
    ch->cap = 0;
    ch->nlines = 0;
}

const char *mat_change_marker(enum mat_change c)
{
    switch (c) {
    case MAT_CHG_ADDED:
        return "+";
    case MAT_CHG_MODIFIED:
        return "~";
    case MAT_CHG_REMOVED:
        return "_";
    default:
        return " ";
    }
}

const char *mat_change_color(enum mat_change c)
{
    switch (c) {
    case MAT_CHG_ADDED:
        return "\x1b[32m"; /* green */
    case MAT_CHG_MODIFIED:
        return "\x1b[33m"; /* yellow */
    case MAT_CHG_REMOVED:
        return "\x1b[31m"; /* red */
    default:
        return "";
    }
}
