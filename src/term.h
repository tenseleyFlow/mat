/*
 * term.h — terminal environment queries for the decoration path.
 *
 * None of this is touched by the fast or cooked paths; it is only consulted
 * once decorations are requested.
 */
#ifndef MAT_TERM_H
#define MAT_TERM_H

#include <stdbool.h>

/* Columns for the decoration frame. If explicit > 0 it wins; otherwise try
 * TIOCGWINSZ, then $COLUMNS, then fall back to 80. */
int mat_term_width(int explicit_width);

/* True if stdout is a terminal. */
bool mat_stdout_is_tty(void);

/* True if NO_COLOR is set to a non-empty value (https://no-color.org). */
bool mat_no_color(void);

#endif /* MAT_TERM_H */
