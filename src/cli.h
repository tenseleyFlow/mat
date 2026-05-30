/*
 * cli.h — argv parsing into struct config.
 *
 * Hand-rolled (no getopt) so the common path stays allocation-free and we keep
 * full control of cat's exact option grammar. Sprint 00 recognizes the operand
 * grammar, --help/--version, and the classic cat transform flags (so the
 * cooked-path gate is real); the cooked path itself arrives in Sprint 02.
 */
#ifndef MAT_CLI_H
#define MAT_CLI_H

#include "config.h"

/*
 * Parse argv into *cfg. files_out must have capacity for at least argc entries;
 * it receives the operand list and cfg->files points into it.
 * Returns 0 on success, -1 on a usage error (message already emitted).
 */
int mat_cli_parse(int argc, char **argv, struct config *cfg,
                  const char **files_out);

void mat_print_usage(void);
void mat_print_version(void);

#endif /* MAT_CLI_H */
