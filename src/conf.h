/*
 * conf.h — configuration from files and the environment.
 *
 * Precedence (low to high): /etc/mat/config, ~/.config/mat/config, $MAT_OPTS,
 * individual $MAT_* vars, then the command line. All config sources are applied
 * as defaults before the real argv is parsed, so the command line always wins.
 *
 * The no-config common case is cheap: env reads are free and the two config
 * files are just absent-file opens; nothing is allocated.
 */
#ifndef MAT_CONF_H
#define MAT_CONF_H

#include <stdbool.h>
#include <stddef.h>

#include "config.h"

/* Apply config-file + environment defaults to *cfg. Skips the files when
 * no_config is set. */
void mat_conf_apply(struct config *cfg, bool no_config);

/* User config path ($MAT_CONFIG_PATH | $XDG_CONFIG_HOME/mat/config |
 * $HOME/.config/mat/config). Writes to buf; returns buf, or NULL if unknown. */
const char *mat_conf_user_path(char *buf, size_t n);

/* Print a commented config-file template to stdout (--generate-config-file). */
void mat_conf_print_template(void);

/*
 * Split a string into argv-style tokens (whitespace separated, '...'/"..."
 * quoting, and '#'-to-end-of-line comments when comments is true). Allocates
 * *out and each token; free with mat_tokens_free. Returns the token count.
 * Exposed for unit testing.
 */
int mat_tokenize(const char *s, bool comments, char ***out);
void mat_tokens_free(char **arr, int n);

#endif /* MAT_CONF_H */
