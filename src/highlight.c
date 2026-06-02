#include "highlight.h"

#include <stdlib.h>
#include <string.h>

#define HL_LONG_LINE (16 * 1024) /* past this a line renders unstyled */

typedef int (*lex_fn)(struct mat_hl *h, const unsigned char *d, size_t len,
                      struct mat_span *out, int cap);

struct wordset {
    const char *const *words;
    int n;
};

struct mat_hl {
    lex_fn lex;
    int state;         /* 0=normal, 1=block-comment, 2=multi-line-string */
    unsigned char tqc; /* triple-quote character (' or ") for Python */
    struct wordset keywords;
    struct wordset types;
};

#include <stdio.h>

/* Named themes: each maps token class -> ANSI SGR (empty == default). */
static const char *const theme_dark[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[35m",
    [MT_TYPE] = "\x1b[33m",
    [MT_STRING] = "\x1b[32m",
    [MT_NUMBER] = "\x1b[36m",
    [MT_COMMENT] = "\x1b[90m",
    [MT_FUNCTION] = "\x1b[34m",
    [MT_OPERATOR] = "\x1b[36m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[35m",
    [MT_CONSTANT] = "\x1b[33m",
};

static const char *const theme_light[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[35m",
    [MT_TYPE] = "\x1b[34m",
    [MT_STRING] = "\x1b[31m",
    [MT_NUMBER] = "\x1b[36m",
    [MT_COMMENT] = "\x1b[37m",
    [MT_FUNCTION] = "\x1b[34m",
    [MT_OPERATOR] = "\x1b[36m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[35m",
    [MT_CONSTANT] = "\x1b[34m",
};

static const char *const theme_monokai[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;197m",
    [MT_TYPE] = "\x1b[38;5;81m",
    [MT_STRING] = "\x1b[38;5;186m",
    [MT_NUMBER] = "\x1b[38;5;141m",
    [MT_COMMENT] = "\x1b[38;5;242m",
    [MT_FUNCTION] = "\x1b[38;5;148m",
    [MT_OPERATOR] = "\x1b[38;5;197m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;197m",
    [MT_CONSTANT] = "\x1b[38;5;141m",
};

static const char *const theme_dracula[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;212m",
    [MT_TYPE] = "\x1b[38;5;159m",
    [MT_STRING] = "\x1b[38;5;229m",
    [MT_NUMBER] = "\x1b[38;5;183m",
    [MT_COMMENT] = "\x1b[38;5;103m",
    [MT_FUNCTION] = "\x1b[38;5;120m",
    [MT_OPERATOR] = "\x1b[38;5;212m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;212m",
    [MT_CONSTANT] = "\x1b[38;5;183m",
};

static const char *const theme_solarized_dark[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;142m",
    [MT_TYPE] = "\x1b[38;5;178m",
    [MT_STRING] = "\x1b[38;5;73m",
    [MT_NUMBER] = "\x1b[38;5;73m",
    [MT_COMMENT] = "\x1b[38;5;102m",
    [MT_FUNCTION] = "\x1b[38;5;74m",
    [MT_OPERATOR] = "\x1b[38;5;142m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;166m",
    [MT_CONSTANT] = "\x1b[38;5;73m",
};

static const char *const theme_solarized_light[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;142m",
    [MT_TYPE] = "\x1b[38;5;178m",
    [MT_STRING] = "\x1b[38;5;73m",
    [MT_NUMBER] = "\x1b[38;5;73m",
    [MT_COMMENT] = "\x1b[38;5;247m",
    [MT_FUNCTION] = "\x1b[38;5;74m",
    [MT_OPERATOR] = "\x1b[38;5;142m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;166m",
    [MT_CONSTANT] = "\x1b[38;5;73m",
};

static const char *const theme_nord[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;146m",
    [MT_TYPE] = "\x1b[38;5;146m",
    [MT_STRING] = "\x1b[38;5;151m",
    [MT_NUMBER] = "\x1b[38;5;248m",
    [MT_COMMENT] = "\x1b[38;5;103m",
    [MT_FUNCTION] = "\x1b[38;5;152m",
    [MT_OPERATOR] = "\x1b[38;5;146m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;146m",
    [MT_CONSTANT] = "\x1b[38;5;254m",
};

static const char *const theme_gruvbox[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;203m",
    [MT_TYPE] = "\x1b[38;5;221m",
    [MT_STRING] = "\x1b[38;5;185m",
    [MT_NUMBER] = "\x1b[38;5;181m",
    [MT_COMMENT] = "\x1b[38;5;244m",
    [MT_FUNCTION] = "\x1b[38;5;185m",
    [MT_OPERATOR] = "\x1b[38;5;223m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;150m",
    [MT_CONSTANT] = "\x1b[38;5;181m",
};

static const char *const theme_onedark[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;176m",
    [MT_TYPE] = "\x1b[38;5;186m",
    [MT_STRING] = "\x1b[38;5;150m",
    [MT_NUMBER] = "\x1b[38;5;180m",
    [MT_COMMENT] = "\x1b[38;5;102m",
    [MT_FUNCTION] = "\x1b[38;5;111m",
    [MT_OPERATOR] = "\x1b[38;5;116m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;176m",
    [MT_CONSTANT] = "\x1b[38;5;180m",
};

/* Catppuccin Mocha (the default "catppuccin"). Palette: catppuccin.com */
static const char *const theme_catppuccin[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;183m",  /* #CBA6F7 mauve */
    [MT_TYPE] = "\x1b[38;5;223m",     /* #F9E2AF yellow */
    [MT_STRING] = "\x1b[38;5;151m",   /* #A6E3A1 green */
    [MT_NUMBER] = "\x1b[38;5;209m",   /* #FAB387 peach */
    [MT_COMMENT] = "\x1b[38;5;243m",  /* #6C7086 overlay0 */
    [MT_FUNCTION] = "\x1b[38;5;111m", /* #89B4FA blue */
    [MT_OPERATOR] = "\x1b[38;5;116m", /* #89DCEB sky */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;225m",  /* #F5C2E7 pink */
    [MT_CONSTANT] = "\x1b[38;5;209m", /* #FAB387 peach */
};

/* Catppuccin Latte (light). Palette: catppuccin.com/palette */
static const char *const theme_catppuccin_latte[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;133m", /* #8839EF mauve */
    [MT_TYPE] = "\x1b[38;5;136m",    /* #DF8E1D yellow */
    [MT_STRING] = "\x1b[38;5;65m",   /* #40A02B green */
    [MT_NUMBER] = "\x1b[38;5;166m",  /* #FE640B peach */
    [MT_COMMENT] = "\x1b[38;5;247m", /* #9CA0B0 overlay0 */
    [MT_FUNCTION] = "\x1b[38;5;32m", /* #1E66F5 blue */
    [MT_OPERATOR] = "\x1b[38;5;74m", /* #04A5E5 sky */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;168m", /* #EA76CB pink */
    [MT_CONSTANT] = "\x1b[38;5;166m",
};

/* Catppuccin Frappe (mid-dark). Palette: catppuccin.com/palette */
static const char *const theme_catppuccin_frappe[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;176m",  /* #CA9EE6 mauve */
    [MT_TYPE] = "\x1b[38;5;186m",     /* #E5C890 yellow */
    [MT_STRING] = "\x1b[38;5;108m",   /* #A6D189 green */
    [MT_NUMBER] = "\x1b[38;5;209m",   /* #EF9F76 peach */
    [MT_COMMENT] = "\x1b[38;5;244m",  /* #737994 overlay0 */
    [MT_FUNCTION] = "\x1b[38;5;111m", /* #8CAAEE blue */
    [MT_OPERATOR] = "\x1b[38;5;117m", /* #99D1DB sky */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;218m", /* #F4B8E4 pink */
    [MT_CONSTANT] = "\x1b[38;5;209m",
};

/* Catppuccin Macchiato (dark). Palette: catppuccin.com/palette */
static const char *const theme_catppuccin_macchiato[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;183m",  /* #C6A0F6 mauve */
    [MT_TYPE] = "\x1b[38;5;222m",     /* #EED49F yellow */
    [MT_STRING] = "\x1b[38;5;150m",   /* #A6DA95 green */
    [MT_NUMBER] = "\x1b[38;5;209m",   /* #F5A97F peach */
    [MT_COMMENT] = "\x1b[38;5;243m",  /* #6E738D overlay0 */
    [MT_FUNCTION] = "\x1b[38;5;111m", /* #8AADF4 blue */
    [MT_OPERATOR] = "\x1b[38;5;117m", /* #91D7E3 sky */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;218m", /* #F5BDE6 pink */
    [MT_CONSTANT] = "\x1b[38;5;209m",
};

/* Gruvbox Light (morhetz/gruvbox). Same accents, light bg. */
static const char *const theme_gruvbox_light[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;124m", /* #9D0006 dark red */
    [MT_TYPE] = "\x1b[38;5;136m",    /* #B57614 dark yellow */
    [MT_STRING] = "\x1b[38;5;64m",   /* #79740E dark green */
    [MT_NUMBER] = "\x1b[38;5;132m",  /* #8F3F71 dark purple */
    [MT_COMMENT] = "\x1b[38;5;246m", /* #928374 grey */
    [MT_FUNCTION] = "\x1b[38;5;64m", /* #79740E */
    [MT_OPERATOR] = "\x1b[38;5;94m", /* #7C6F64 dark fg4 */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;66m", /* #427B58 dark aqua */
    [MT_CONSTANT] = "\x1b[38;5;132m",
};

/* One Light (Atom). From atom/one-light-syntax. */
static const char *const theme_onelight[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;133m", /* #A626A4 hue-3 purple */
    [MT_TYPE] = "\x1b[38;5;136m",    /* #C18401 hue-6-2 orange */
    [MT_STRING] = "\x1b[38;5;65m",   /* #50A14F hue-4 green */
    [MT_NUMBER] = "\x1b[38;5;166m",  /* #986801 hue-6 orange */
    [MT_COMMENT] = "\x1b[38;5;247m", /* #A0A1A7 mono-3 */
    [MT_FUNCTION] = "\x1b[38;5;32m", /* #4078F2 hue-2 blue */
    [MT_OPERATOR] = "\x1b[38;5;30m", /* #0184BC hue-1 cyan */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;133m",
    [MT_CONSTANT] = "\x1b[38;5;166m",
};

/* Night Owl (Sarah Drasner). From sdras/night-owl-vscode-theme. */
static const char *const theme_nightowl[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;176m", /* #C792EA purple */
    [MT_TYPE] = "\x1b[38;5;179m",    /* #FFCB6B yellow */
    [MT_STRING] = "\x1b[38;5;186m",  /* #ECC48D light sand */
    [MT_NUMBER] = "\x1b[38;5;209m",  /* #F78C6C orange */
    [MT_COMMENT] = "\x1b[38;5;60m",  /* #637777 muted teal */
    [MT_FUNCTION] = "\x1b[38;5;75m", /* #82AAFF blue */
    [MT_OPERATOR] = "\x1b[38;5;176m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;204m", /* #FF5874 red */
    [MT_CONSTANT] = "\x1b[38;5;209m",
};

static const char *const theme_github_dark[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;204m",
    [MT_TYPE] = "\x1b[38;5;215m",
    [MT_STRING] = "\x1b[38;5;116m",
    [MT_NUMBER] = "\x1b[38;5;116m",
    [MT_COMMENT] = "\x1b[38;5;243m",
    [MT_FUNCTION] = "\x1b[38;5;183m",
    [MT_OPERATOR] = "\x1b[38;5;204m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;204m",
    [MT_CONSTANT] = "\x1b[38;5;116m",
};

static const char *const theme_github_light[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;161m",
    [MT_TYPE] = "\x1b[38;5;130m",
    [MT_STRING] = "\x1b[38;5;28m",
    [MT_NUMBER] = "\x1b[38;5;28m",
    [MT_COMMENT] = "\x1b[38;5;245m",
    [MT_FUNCTION] = "\x1b[38;5;99m",
    [MT_OPERATOR] = "\x1b[38;5;161m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;161m",
    [MT_CONSTANT] = "\x1b[38;5;28m",
};

static const char *const theme_tokyonight[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;176m",
    [MT_TYPE] = "\x1b[38;5;116m",
    [MT_STRING] = "\x1b[38;5;150m",
    [MT_NUMBER] = "\x1b[38;5;215m",
    [MT_COMMENT] = "\x1b[38;5;60m",
    [MT_FUNCTION] = "\x1b[38;5;75m",
    [MT_OPERATOR] = "\x1b[38;5;116m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;176m",
    [MT_CONSTANT] = "\x1b[38;5;215m",
};

/* Zenburn: classic low-contrast warm dark theme (Jani Nurminen, 2003).
 * Hex refs: fg #DCDCCC, bg #3F3F3F, green #7F9F7F, yellow #F0DFAF. */
static const char *const theme_zenburn[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;179m",  /* #F0DFAF warm yellow */
    [MT_TYPE] = "\x1b[38;5;152m",     /* #8CD0D3 cyan */
    [MT_STRING] = "\x1b[38;5;174m",   /* #CC9393 muted red */
    [MT_NUMBER] = "\x1b[38;5;116m",   /* #8CD0D3 cyan */
    [MT_COMMENT] = "\x1b[38;5;108m",  /* #7F9F7F green-grey */
    [MT_FUNCTION] = "\x1b[38;5;223m", /* #EFEF8F light yellow */
    [MT_OPERATOR] = "\x1b[38;5;179m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;179m",
    [MT_CONSTANT] = "\x1b[38;5;116m",
};

/* Tomorrow Night: Chris Kempson's Base16-derived dark theme.
 * Hex refs: red #CC6666, orange #DE935F, yellow #F0C674, green #B5BD68,
 * cyan #8ABEB7, blue #81A2BE, purple #B294BB, comment #969896. */
static const char *const theme_tomorrow_night[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;139m",  /* #B294BB purple */
    [MT_TYPE] = "\x1b[38;5;179m",     /* #F0C674 yellow */
    [MT_STRING] = "\x1b[38;5;143m",   /* #B5BD68 green */
    [MT_NUMBER] = "\x1b[38;5;173m",   /* #DE935F orange */
    [MT_COMMENT] = "\x1b[38;5;246m",  /* #969896 grey */
    [MT_FUNCTION] = "\x1b[38;5;110m", /* #81A2BE blue */
    [MT_OPERATOR] = "\x1b[38;5;116m", /* #8ABEB7 cyan */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;167m",  /* #CC6666 red */
    [MT_CONSTANT] = "\x1b[38;5;173m", /* #DE935F orange */
};

/* Tomorrow: Chris Kempson's Base16 light theme.
 * Same accent colors, lighter bg/comment. */
static const char *const theme_tomorrow[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;133m", /* #8959A8 purple */
    [MT_TYPE] = "\x1b[38;5;136m",    /* #EAB700 yellow-brown */
    [MT_STRING] = "\x1b[38;5;65m",   /* #718C00 green */
    [MT_NUMBER] = "\x1b[38;5;166m",  /* #F5871F orange */
    [MT_COMMENT] = "\x1b[38;5;247m", /* #8E908C grey */
    [MT_FUNCTION] = "\x1b[38;5;67m", /* #4271AE blue */
    [MT_OPERATOR] = "\x1b[38;5;30m", /* #3E999F cyan */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;131m", /* #C82829 red */
    [MT_CONSTANT] = "\x1b[38;5;166m",
};

/* Material: Google Material Design dark (Mattia Astorino).
 * Hex refs from material-theme. */
static const char *const theme_material[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;176m",  /* #C792EA purple */
    [MT_TYPE] = "\x1b[38;5;222m",     /* #FFCB6B yellow */
    [MT_STRING] = "\x1b[38;5;193m",   /* #C3E88D green */
    [MT_NUMBER] = "\x1b[38;5;209m",   /* #F78C6C orange */
    [MT_COMMENT] = "\x1b[38;5;102m",  /* #546E7A grey */
    [MT_FUNCTION] = "\x1b[38;5;147m", /* #82AAFF blue */
    [MT_OPERATOR] = "\x1b[38;5;153m", /* #89DDFF cyan */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;176m",
    [MT_CONSTANT] = "\x1b[38;5;209m",
};

/* Palenight: Material variant with purple tints.
 * Same accents as Material, slightly shifted. */
static const char *const theme_palenight[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;176m",  /* #C792EA */
    [MT_TYPE] = "\x1b[38;5;222m",     /* #FFCB6B */
    [MT_STRING] = "\x1b[38;5;193m",   /* #C3E88D */
    [MT_NUMBER] = "\x1b[38;5;209m",   /* #F78C6C */
    [MT_COMMENT] = "\x1b[38;5;60m",   /* #676E95 muted purple-grey */
    [MT_FUNCTION] = "\x1b[38;5;147m", /* #82AAFF */
    [MT_OPERATOR] = "\x1b[38;5;153m", /* #89DDFF */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;204m", /* #FF5370 red */
    [MT_CONSTANT] = "\x1b[38;5;209m",
};

/* Synthwave 84: neon retro dark theme (Robb Owen).
 * Hex refs: #FF7EDB pink, #36F9F6 cyan, #FF8B39 orange, #FEDE5D yellow. */
static const char *const theme_synthwave[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;212m", /* #FF7EDB neon pink */
    [MT_TYPE] = "\x1b[38;5;81m",     /* #36F9F6 neon cyan */
    [MT_STRING] = "\x1b[38;5;221m",  /* #FEDE5D neon yellow */
    [MT_NUMBER] = "\x1b[38;5;209m",  /* #FF8B39 orange */
    [MT_COMMENT] = "\x1b[38;5;60m",  /* #848BBD muted */
    [MT_FUNCTION] = "\x1b[38;5;87m", /* #72F1B8 neon green */
    [MT_OPERATOR] = "\x1b[38;5;81m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;212m",
    [MT_CONSTANT] = "\x1b[38;5;209m",
};

/* Kanagawa: wave-inspired dark theme (rebelot, for Neovim).
 * Hex refs: #DCA561 autumn yellow, #7E9CD8 crystal blue, #98BB6C green,
 * #727169 comment. */
static const char *const theme_kanagawa[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;140m",  /* #957FB8 spring violet */
    [MT_TYPE] = "\x1b[38;5;110m",     /* #7E9CD8 crystal blue */
    [MT_STRING] = "\x1b[38;5;143m",   /* #98BB6C autumn green */
    [MT_NUMBER] = "\x1b[38;5;176m",   /* #D27E99 sakura pink */
    [MT_COMMENT] = "\x1b[38;5;102m",  /* #727169 fuji grey */
    [MT_FUNCTION] = "\x1b[38;5;110m", /* #7E9CD8 crystal blue */
    [MT_OPERATOR] = "\x1b[38;5;174m", /* #C0A36E surimi orange */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;174m",  /* #FFA066 */
    [MT_CONSTANT] = "\x1b[38;5;216m", /* #DCA561 */
};

/* Rose Pine: muted low-contrast dark theme.
 * Hex refs: #31748F pine, #C4A7E7 iris, #9CCFD8 foam, #E0DEF4 text. */
static const char *const theme_rosepine[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;67m",   /* #31748F pine */
    [MT_TYPE] = "\x1b[38;5;183m",     /* #C4A7E7 iris */
    [MT_STRING] = "\x1b[38;5;222m",   /* #F6C177 gold */
    [MT_NUMBER] = "\x1b[38;5;183m",   /* #C4A7E7 iris */
    [MT_COMMENT] = "\x1b[38;5;103m",  /* #6E6A86 muted */
    [MT_FUNCTION] = "\x1b[38;5;210m", /* #EBBCBA rose */
    [MT_OPERATOR] = "\x1b[38;5;116m", /* #9CCFD8 foam */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;67m",
    [MT_CONSTANT] = "\x1b[38;5;183m",
};

/* Rose Pine Moon: mid-contrast variant.
 * Same accents as Rose Pine but slightly brighter. */
static const char *const theme_rosepine_moon[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;73m", /* #3E8FB0 pine */
    [MT_TYPE] = "\x1b[38;5;183m",   /* #C4A7E7 iris */
    [MT_STRING] = "\x1b[38;5;222m", /* #F6C177 gold */
    [MT_NUMBER] = "\x1b[38;5;183m",
    [MT_COMMENT] = "\x1b[38;5;103m",  /* #6E6A86 */
    [MT_FUNCTION] = "\x1b[38;5;217m", /* #EA9A97 rose */
    [MT_OPERATOR] = "\x1b[38;5;116m", /* #9CCFD8 foam */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;73m",
    [MT_CONSTANT] = "\x1b[38;5;183m",
};

/* Rose Pine Dawn: light variant.
 * Hex refs: same accents on light bg. */
static const char *const theme_rosepine_dawn[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;30m", /* #286983 pine */
    [MT_TYPE] = "\x1b[38;5;97m",    /* #907AA9 iris */
    [MT_STRING] = "\x1b[38;5;172m", /* #EA9D34 gold */
    [MT_NUMBER] = "\x1b[38;5;97m",
    [MT_COMMENT] = "\x1b[38;5;247m",  /* #9893A5 */
    [MT_FUNCTION] = "\x1b[38;5;131m", /* #D7827E rose */
    [MT_OPERATOR] = "\x1b[38;5;66m",  /* #56949F foam */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;30m",
    [MT_CONSTANT] = "\x1b[38;5;97m",
};

/* Everforest Dark: sakata's nature-inspired dark theme.
 * Hex refs: #A7C080 green, #D699B6 purple, #E67E80 red, #83C092 aqua. */
static const char *const theme_everforest_dark[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;167m",  /* #E67E80 red */
    [MT_TYPE] = "\x1b[38;5;179m",     /* #DBBC7F yellow */
    [MT_STRING] = "\x1b[38;5;143m",   /* #A7C080 green */
    [MT_NUMBER] = "\x1b[38;5;175m",   /* #D699B6 purple */
    [MT_COMMENT] = "\x1b[38;5;102m",  /* #859289 grey */
    [MT_FUNCTION] = "\x1b[38;5;108m", /* #83C092 aqua */
    [MT_OPERATOR] = "\x1b[38;5;173m", /* #E69875 orange */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;167m",
    [MT_CONSTANT] = "\x1b[38;5;175m",
};

/* Everforest Light: light variant. */
static const char *const theme_everforest_light[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;131m",  /* #F85552 -> nearest 256 */
    [MT_TYPE] = "\x1b[38;5;136m",     /* #DFA000 */
    [MT_STRING] = "\x1b[38;5;65m",    /* #8DA101 */
    [MT_NUMBER] = "\x1b[38;5;133m",   /* #DF69BA */
    [MT_COMMENT] = "\x1b[38;5;247m",  /* #939F91 */
    [MT_FUNCTION] = "\x1b[38;5;29m",  /* #35A77C */
    [MT_OPERATOR] = "\x1b[38;5;130m", /* #F57D26 */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;131m",
    [MT_CONSTANT] = "\x1b[38;5;133m",
};

/* Ayu Dark: Ike Ku's modern dark theme (Sublime/VS Code).
 * Hex refs: #FF8F40 orange, #E6B450 yellow, #AAD94C green, #39BAE6 blue. */
static const char *const theme_ayu_dark[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;209m",  /* #FF8F40 orange */
    [MT_TYPE] = "\x1b[38;5;75m",      /* #39BAE6 blue */
    [MT_STRING] = "\x1b[38;5;149m",   /* #AAD94C green */
    [MT_NUMBER] = "\x1b[38;5;176m",   /* #D2A6FF purple */
    [MT_COMMENT] = "\x1b[38;5;242m",  /* #ACB6BF8C faded */
    [MT_FUNCTION] = "\x1b[38;5;179m", /* #E6B450 yellow */
    [MT_OPERATOR] = "\x1b[38;5;209m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;209m",
    [MT_CONSTANT] = "\x1b[38;5;176m",
};

/* Ayu Mirage: muted dark variant. */
static const char *const theme_ayu_mirage[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;209m",  /* #FFAD66 */
    [MT_TYPE] = "\x1b[38;5;75m",      /* #5CCFE6 */
    [MT_STRING] = "\x1b[38;5;149m",   /* #D5FF80 */
    [MT_NUMBER] = "\x1b[38;5;176m",   /* #DFBFFF */
    [MT_COMMENT] = "\x1b[38;5;60m",   /* #B8CFE680 faded */
    [MT_FUNCTION] = "\x1b[38;5;179m", /* #FFD580 */
    [MT_OPERATOR] = "\x1b[38;5;209m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;209m",
    [MT_CONSTANT] = "\x1b[38;5;176m",
};

/* Ayu Light: light variant. */
static const char *const theme_ayu_light[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;166m",  /* #FA8D3E */
    [MT_TYPE] = "\x1b[38;5;32m",      /* #399EE6 */
    [MT_STRING] = "\x1b[38;5;64m",    /* #86B300 */
    [MT_NUMBER] = "\x1b[38;5;97m",    /* #A37ACC */
    [MT_COMMENT] = "\x1b[38;5;247m",  /* #ABB0B6 */
    [MT_FUNCTION] = "\x1b[38;5;136m", /* #F2AE49 */
    [MT_OPERATOR] = "\x1b[38;5;166m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;166m",
    [MT_CONSTANT] = "\x1b[38;5;97m",
};

/* Nightfox (EdenEast/nightfox.nvim). palette/nightfox.lua */
static const char *const theme_nightfox[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;140m",  /* #9D79D6 */
    [MT_TYPE] = "\x1b[38;5;180m",     /* #DBC074 */
    [MT_STRING] = "\x1b[38;5;108m",   /* #81B29A */
    [MT_NUMBER] = "\x1b[38;5;215m",   /* #F4A261 */
    [MT_COMMENT] = "\x1b[38;5;243m",  /* #738091 */
    [MT_FUNCTION] = "\x1b[38;5;110m", /* #86AADC */
    [MT_OPERATOR] = "\x1b[38;5;145m", /* #AEAFB0 */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;176m",  /* #DC8DD8 */
    [MT_CONSTANT] = "\x1b[38;5;216m", /* #F5AF78 */
};

/* Dayfox (nightfox.nvim light variant). palette/dayfox.lua */
static const char *const theme_dayfox[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;62m",  /* #6E33CE */
    [MT_TYPE] = "\x1b[38;5;130m",    /* #AC5402 */
    [MT_STRING] = "\x1b[38;5;65m",   /* #396847 */
    [MT_NUMBER] = "\x1b[38;5;95m",   /* #955F61 */
    [MT_COMMENT] = "\x1b[38;5;243m", /* #837A72 */
    [MT_FUNCTION] = "\x1b[38;5;61m", /* #4863B5 */
    [MT_OPERATOR] = "\x1b[38;5;96m", /* #643F61 */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;133m",  /* #B15CC0 */
    [MT_CONSTANT] = "\x1b[38;5;138m", /* #A47778 */
};

/* Dawnfox (nightfox.nvim warm light). palette/dawnfox.lua */
static const char *const theme_dawnfox[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;103m", /* #907AA9 */
    [MT_TYPE] = "\x1b[38;5;179m",    /* #EA9D34 */
    [MT_STRING] = "\x1b[38;5;66m",   /* #618774 */
    [MT_NUMBER] = "\x1b[38;5;174m",  /* #D7827E */
    [MT_COMMENT] = "\x1b[38;5;247m", /* #9893A5 */
    [MT_FUNCTION] = "\x1b[38;5;23m", /* #295E73 */
    [MT_OPERATOR] = "\x1b[38;5;60m", /* #625C87 */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;169m",  /* #C9709E */
    [MT_CONSTANT] = "\x1b[38;5;167m", /* #CA6E69 */
};

/* Carbonfox (nightfox.nvim IBM Carbon). palette/carbonfox.lua */
static const char *const theme_carbonfox[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;141m",  /* #BE95FF */
    [MT_TYPE] = "\x1b[38;5;37m",      /* #08BDBA */
    [MT_STRING] = "\x1b[38;5;35m",    /* #25BE6A */
    [MT_NUMBER] = "\x1b[38;5;80m",    /* #3DDBD9 */
    [MT_COMMENT] = "\x1b[38;5;242m",  /* #6E6E70 */
    [MT_FUNCTION] = "\x1b[38;5;111m", /* #8CB5FF */
    [MT_OPERATOR] = "\x1b[38;5;146m", /* #A2B0CD */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;211m", /* #FF91C0 */
    [MT_CONSTANT] = "\x1b[38;5;80m", /* #5AE0DE */
};

/* Oxocarbon (nyoom-engineering/oxocarbon.nvim). IBM Carbon palette. */
static const char *const theme_oxocarbon[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;111m",  /* #78A9FF */
    [MT_TYPE] = "\x1b[38;5;111m",     /* #78A9FF */
    [MT_STRING] = "\x1b[38;5;141m",   /* #BE95FF */
    [MT_NUMBER] = "\x1b[38;5;117m",   /* #82CFFF */
    [MT_COMMENT] = "\x1b[38;5;239m",  /* #525252 */
    [MT_FUNCTION] = "\x1b[38;5;80m",  /* #3DDBD9 */
    [MT_OPERATOR] = "\x1b[38;5;111m", /* #78A9FF */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;111m",  /* #78A9FF */
    [MT_CONSTANT] = "\x1b[38;5;254m", /* #DDE1E6 */
};

/* Poimandres (drcmda/poimandres-theme). Minimal palette. */
static const char *const theme_poimandres[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;146m",  /* #A6ACCD */
    [MT_TYPE] = "\x1b[38;5;110m",     /* #91B4D5 */
    [MT_STRING] = "\x1b[38;5;80m",    /* #5DE4C7 */
    [MT_NUMBER] = "\x1b[38;5;80m",    /* #5DE4C7 */
    [MT_COMMENT] = "\x1b[38;5;103m",  /* #767C9D */
    [MT_FUNCTION] = "\x1b[38;5;153m", /* #ADD7FF */
    [MT_OPERATOR] = "\x1b[38;5;110m", /* #91B4D5 */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;146m", /* #A6ACCD */
    [MT_CONSTANT] = "\x1b[38;5;80m", /* #5DE4C7 */
};

/* Moonfly (bluz71/vim-moonfly-colors). moonfly/init.lua */
static const char *const theme_moonfly[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;176m",  /* #CF87E8 */
    [MT_TYPE] = "\x1b[38;5;78m",      /* #36C692 */
    [MT_STRING] = "\x1b[38;5;186m",   /* #C6C684 */
    [MT_NUMBER] = "\x1b[38;5;173m",   /* #DE935F */
    [MT_COMMENT] = "\x1b[38;5;246m",  /* #949494 */
    [MT_FUNCTION] = "\x1b[38;5;111m", /* #74B2FF */
    [MT_OPERATOR] = "\x1b[38;5;167m", /* #E65E72 */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;167m",  /* #E65E72 */
    [MT_CONSTANT] = "\x1b[38;5;141m", /* #AE81FF */
};

/* Nightfly (bluz71/vim-nightfly-colors). nightfly/init.lua */
static const char *const theme_nightfly[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;176m",  /* #C792EA */
    [MT_TYPE] = "\x1b[38;5;43m",      /* #21C7A8 */
    [MT_STRING] = "\x1b[38;5;222m",   /* #ECC48D */
    [MT_NUMBER] = "\x1b[38;5;209m",   /* #F78C6C */
    [MT_COMMENT] = "\x1b[38;5;245m",  /* #7C8F8F */
    [MT_FUNCTION] = "\x1b[38;5;111m", /* #82AAFF */
    [MT_OPERATOR] = "\x1b[38;5;204m", /* #FF5874 */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;204m",  /* #FF5874 */
    [MT_CONSTANT] = "\x1b[38;5;209m", /* #F78C6C */
};

/* Iceberg (cocopon/iceberg.vim). Cohesive blue palette. */
static const char *const theme_iceberg[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;110m",  /* #84A0C6 */
    [MT_TYPE] = "\x1b[38;5;110m",     /* #84A0C6 */
    [MT_STRING] = "\x1b[38;5;109m",   /* #89B8C2 */
    [MT_NUMBER] = "\x1b[38;5;140m",   /* #A093C7 */
    [MT_COMMENT] = "\x1b[38;5;243m",  /* #6B7089 */
    [MT_FUNCTION] = "\x1b[38;5;110m", /* #84A0C6 */
    [MT_OPERATOR] = "\x1b[38;5;110m", /* #84A0C6 */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;144m",  /* #B4BE82 */
    [MT_CONSTANT] = "\x1b[38;5;140m", /* #A093C7 */
};

/* Modus Vivendi (Protesilaos Stavrou, Emacs). WCAG AAA dark. */
static const char *const theme_modus_vivendi[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;147m",  /* #B6A0FF */
    [MT_TYPE] = "\x1b[38;5;39m",      /* #00BCFF */
    [MT_STRING] = "\x1b[38;5;111m",   /* #79A8FF */
    [MT_NUMBER] = "",                 /* fg-main (white) */
    [MT_COMMENT] = "\x1b[38;5;246m",  /* #989898 */
    [MT_FUNCTION] = "\x1b[38;5;218m", /* #FEACD0 */
    [MT_OPERATOR] = "",               /* fg-main */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;210m", /* #FF7F86 */
    [MT_CONSTANT] = "\x1b[38;5;39m", /* #00BCFF */
};

/* Modus Operandi (Protesilaos Stavrou, Emacs). WCAG AAA light. */
static const char *const theme_modus_operandi[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;54m",  /* #721C8F */
    [MT_TYPE] = "\x1b[38;5;24m",     /* #005F87 */
    [MT_STRING] = "\x1b[38;5;62m",   /* #3548CF */
    [MT_NUMBER] = "",                /* fg-main (black) */
    [MT_COMMENT] = "\x1b[38;5;240m", /* #595959 */
    [MT_FUNCTION] = "\x1b[38;5;53m", /* #721045 */
    [MT_OPERATOR] = "",              /* fg-main */
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;124m", /* #A0132F */
    [MT_CONSTANT] = "\x1b[38;5;19m", /* #0000B0 */
};

static const char *const *active_theme = theme_dark;

struct theme_entry {
    const char *name;
    const char *const *table;
};

static const struct theme_entry theme_tbl[] = {
    {"ayu-dark", theme_ayu_dark},
    {"ayu-light", theme_ayu_light},
    {"ayu-mirage", theme_ayu_mirage},
    {"carbonfox", theme_carbonfox},
    {"catppuccin", theme_catppuccin},
    {"catppuccin-frappe", theme_catppuccin_frappe},
    {"catppuccin-latte", theme_catppuccin_latte},
    {"catppuccin-macchiato", theme_catppuccin_macchiato},
    {"dark", theme_dark},
    {"dawnfox", theme_dawnfox},
    {"dayfox", theme_dayfox},
    {"dracula", theme_dracula},
    {"everforest-dark", theme_everforest_dark},
    {"everforest-light", theme_everforest_light},
    {"github-dark", theme_github_dark},
    {"github-light", theme_github_light},
    {"gruvbox", theme_gruvbox},
    {"gruvbox-light", theme_gruvbox_light},
    {"iceberg", theme_iceberg},
    {"kanagawa", theme_kanagawa},
    {"light", theme_light},
    {"material", theme_material},
    {"modus-operandi", theme_modus_operandi},
    {"modus-vivendi", theme_modus_vivendi},
    {"monokai", theme_monokai},
    {"moonfly", theme_moonfly},
    {"nightfly", theme_nightfly},
    {"nightfox", theme_nightfox},
    {"nightowl", theme_nightowl},
    {"nord", theme_nord},
    {"onedark", theme_onedark},
    {"onelight", theme_onelight},
    {"oxocarbon", theme_oxocarbon},
    {"palenight", theme_palenight},
    {"poimandres", theme_poimandres},
    {"rosepine", theme_rosepine},
    {"rosepine-dawn", theme_rosepine_dawn},
    {"rosepine-moon", theme_rosepine_moon},
    {"solarized-dark", theme_solarized_dark},
    {"solarized-light", theme_solarized_light},
    {"synthwave", theme_synthwave},
    {"tokyonight", theme_tokyonight},
    {"tomorrow", theme_tomorrow},
    {"tomorrow-night", theme_tomorrow_night},
    {"zenburn", theme_zenburn},
};

#define THEME_TBL_N (sizeof theme_tbl / sizeof theme_tbl[0])

static int theme_cmp(const void *a, const void *b)
{
    return strcmp((const char *)a, ((const struct theme_entry *)b)->name);
}

int mat_theme_set(const char *name)
{
    if (name == NULL) {
        active_theme = theme_dark;
        return 0;
    }
    const struct theme_entry *e =
        bsearch(name, theme_tbl, THEME_TBL_N, sizeof theme_tbl[0], theme_cmp);
    if (e == NULL)
        return -1;
    active_theme = e->table;
    return 0;
}

const char *mat_theme_sgr(enum mat_tok tok)
{
    if (tok < 0 || tok >= MT_NTOKENS)
        return "";
    return active_theme[tok];
}

void mat_theme_list(void)
{
    printf("ayu-dark\nayu-light\nayu-mirage\n"
           "carbonfox\ncatppuccin\ncatppuccin-frappe\n"
           "catppuccin-latte\ncatppuccin-macchiato\n"
           "dark\ndawnfox\ndayfox\ndracula\n"
           "everforest-dark\neverforest-light\n"
           "github-dark\ngithub-light\ngruvbox\ngruvbox-light\n"
           "iceberg\nkanagawa\nlight\nmaterial\n"
           "modus-operandi\nmodus-vivendi\nmoonfly\nmonokai\n"
           "nightfly\nnightfox\nnightowl\nnord\n"
           "onedark\nonelight\noxocarbon\npalenight\n"
           "poimandres\nrosepine\nrosepine-dawn\nrosepine-moon\n"
           "solarized-dark\nsolarized-light\nsynthwave\n"
           "tokyonight\ntomorrow\ntomorrow-night\nzenburn\n");
}

int mat_theme_count(void)
{
    return 45;
}

static int emit(struct mat_span *out, int cap, int n, size_t start, size_t len,
                enum mat_tok tok)
{
    if (len == 0 || n >= cap)
        return n;
    out[n].start = (unsigned)start;
    out[n].len = (unsigned)len;
    out[n].tok = tok;
    return n + 1;
}

static int is_digit(unsigned char c)
{
    return c >= '0' && c <= '9';
}

static int is_word(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int ws_cmp(const void *key, const void *elem)
{
    const char *k = (const char *)key;
    const char *const *e = (const char *const *)elem;
    return strcmp(k, *e);
}

static int ws_has(const struct wordset *ws, const char *w, size_t wl)
{
    char buf[128];
    if (wl >= sizeof buf)
        return 0;
    memcpy(buf, w, wl);
    buf[wl] = '\0';
    return bsearch(buf, ws->words, (size_t)ws->n, sizeof ws->words[0],
                   ws_cmp) != NULL;
}

static int is_alnum(unsigned char c)
{
    return is_word(c) || is_digit(c);
}

/* ---- C-family: C, C++, Java, JavaScript, Go, Rust ---- */

enum { HL_NORMAL = 0, HL_BLOCK_COMMENT = 1 };

static int lex_cfamily(struct mat_hl *h, const unsigned char *d, size_t len,
                       struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_BLOCK_COMMENT) {
        size_t s = 0;
        while (i + 1 < len) {
            if (d[i] == '*' && d[i + 1] == '/') {
                i += 2;
                h->state = HL_NORMAL;
                break;
            }
            i++;
        }
        if (h->state == HL_BLOCK_COMMENT)
            i = len;
        n = emit(out, cap, n, s, i, MT_COMMENT);
        if (h->state == HL_BLOCK_COMMENT)
            return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '/' && i + 1 < len && d[i + 1] == '/') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '/' && i + 1 < len && d[i + 1] == '*') {
            h->state = HL_BLOCK_COMMENT;
            i += 2;
            while (i + 1 < len) {
                if (d[i] == '*' && d[i + 1] == '/') {
                    i += 2;
                    h->state = HL_NORMAL;
                    break;
                }
                i++;
            }
            if (h->state == HL_BLOCK_COMMENT)
                i = len;
            n = emit(out, cap, n, s, i - s, MT_COMMENT);
        } else if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_PREPROC);
            return n;
        } else if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len) {
                if (d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == q) {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c) ||
                   (c == '.' && i + 1 < len && is_digit(d[i + 1]))) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.' || d[i] == 'x' ||
                               d[i] == 'X' || d[i] == '+' || d[i] == '-'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && is_alnum(d[i]))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            else if (i < len && d[i] == '(')
                t = MT_FUNCTION;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
            n = emit(out, cap, n, s, 1,
                     (c == '(' || c == ')' || c == '{' || c == '}' ||
                      c == '[' || c == ']' || c == ';')
                         ? MT_PUNCT
                         : MT_TEXT);
        }
    }
    return n;
}

/* ---- Python ---- */

enum { HL_PY_TRIPLE = 2 };

static int lex_python(struct mat_hl *h, const unsigned char *d, size_t len,
                      struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_PY_TRIPLE) {
        unsigned char tq = h->tqc;
        size_t s = 0;
        while (i + 2 < len) {
            if (d[i] == tq && d[i + 1] == tq && d[i + 2] == tq) {
                i += 3;
                h->state = HL_NORMAL;
                break;
            }
            i++;
        }
        if (h->state == HL_PY_TRIPLE)
            i = len;
        n = emit(out, cap, n, s, i, MT_STRING);
        if (h->state == HL_PY_TRIPLE)
            return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if ((c == '"' || c == '\'') && i + 2 < len && d[i + 1] == c &&
            d[i + 2] == c) {
            h->state = HL_PY_TRIPLE;
            h->tqc = c;
            i += 3;
            while (i + 2 < len) {
                if (d[i] == c && d[i + 1] == c && d[i + 2] == c) {
                    i += 3;
                    h->state = HL_NORMAL;
                    break;
                }
                i++;
            }
            if (h->state == HL_PY_TRIPLE)
                i = len;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len) {
                if (d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == q) {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c) ||
                   (c == '.' && i + 1 < len && is_digit(d[i + 1]))) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.' || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (c == '@') {
            i++;
            while (i < len && is_alnum(d[i]))
                i++;
            n = emit(out, cap, n, s, i - s, MT_PREPROC);
        } else if (is_word(c)) {
            i++;
            while (i < len && is_alnum(d[i]))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            else if (i < len && d[i] == '(')
                t = MT_FUNCTION;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Shell (Bash/Zsh) ---- */

static int lex_shell(struct mat_hl *h, const unsigned char *d, size_t len,
                     struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len) {
                if (q == '"' && d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == q) {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '$') {
            i++;
            if (i < len && d[i] == '{') {
                while (i < len && d[i] != '}')
                    i++;
                if (i < len)
                    i++;
            } else {
                while (i < len && is_alnum(d[i]))
                    i++;
            }
            n = emit(out, cap, n, s, i - s, MT_PREPROC);
        } else if (is_digit(c)) {
            i++;
            while (i < len && is_digit(d[i]))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '-'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- JSON ---- */

static int lex_json(struct mat_hl *h, const unsigned char *d, size_t len,
                    struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '"') {
            i++;
            while (i < len) {
                if (d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                unsigned char e = d[i++];
                if (e == '"')
                    break;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c) ||
                   (c == '-' && i + 1 < len && is_digit(d[i + 1]))) {
            i++;
            while (i < len && (is_digit(d[i]) || d[i] == '.' || d[i] == 'e' ||
                               d[i] == 'E' || d[i] == '+' || d[i] == '-'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && is_word(d[i]))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if ((wl == 4 && memcmp(d + s, "true", 4) == 0) ||
                (wl == 5 && memcmp(d + s, "false", 5) == 0) ||
                (wl == 4 && memcmp(d + s, "null", 4) == 0))
                t = MT_CONSTANT;
            n = emit(out, cap, n, s, wl, t);
        } else if (c == '{' || c == '}' || c == '[' || c == ']') {
            i++;
            n = emit(out, cap, n, s, 1, MT_PUNCT);
        } else if (c == ':' || c == ',') {
            i++;
            n = emit(out, cap, n, s, 1, MT_OPERATOR);
        } else {
            i++;
            while (i < len) {
                unsigned char x = d[i];
                if (x == '"' || is_digit(x) || is_word(x) || x == '{' ||
                    x == '}' || x == '[' || x == ']' || x == ':' || x == ',' ||
                    (x == '-' && i + 1 < len && is_digit(d[i + 1])))
                    break;
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_TEXT);
        }
    }
    return n;
}

/* ---- Fortran (free-form; ! comments, case-insensitive keywords) ---- */

static int ci_match(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z')
            ca = (char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z')
            cb = (char)(cb + 32);
        if (ca != cb)
            return 0;
    }
    return 1;
}

static int ws_has_ci(const struct wordset *ws, const char *w, size_t wl)
{
    char buf[128];
    if (wl >= sizeof buf)
        return 0;
    for (size_t i = 0; i < wl; i++) {
        char c = w[i];
        if (c >= 'A' && c <= 'Z')
            c = (char)(c + 32);
        buf[i] = c;
    }
    buf[wl] = '\0';
    return bsearch(buf, ws->words, (size_t)ws->n, sizeof ws->words[0],
                   ws_cmp) != NULL;
}

static int lex_fortran(struct mat_hl *h, const unsigned char *d, size_t len,
                       struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '!') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '\'' || c == '"') {
            unsigned char q = c;
            i++;
            while (i < len) {
                if (d[i] == q) {
                    i++;
                    if (i < len && d[i] == q) {
                        i++;
                        continue;
                    }
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c) ||
                   (c == '.' && i + 1 < len && is_digit(d[i + 1]))) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.' || d[i] == '+' ||
                               d[i] == '-' || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has_ci(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has_ci(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            else if (i < len && d[i] == '(')
                t = MT_FUNCTION;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Markdown ---- */

static int lex_markdown(struct mat_hl *h, const unsigned char *d, size_t len,
                        struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_BLOCK_COMMENT) {
        size_t s = 0;
        if (len >= 3 && d[0] == '`' && d[1] == '`' && d[2] == '`') {
            h->state = HL_NORMAL;
            n = emit(out, cap, n, 0, len, MT_PREPROC);
            return n;
        }
        n = emit(out, cap, n, s, len, MT_STRING);
        return n;
    }
    if (len >= 3 && d[0] == '`' && d[1] == '`' && d[2] == '`') {
        h->state = HL_BLOCK_COMMENT;
        n = emit(out, cap, n, 0, len, MT_PREPROC);
        return n;
    }
    if (len > 0 && d[0] == '#') {
        n = emit(out, cap, n, 0, len, MT_KEYWORD);
        return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '`') {
            i++;
            while (i < len && d[i] != '`')
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '[') {
            i++;
            while (i < len && d[i] != ']')
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_FUNCTION);
            if (i < len && d[i] == '(') {
                size_t ls = i;
                i++;
                while (i < len && d[i] != ')')
                    i++;
                if (i < len)
                    i++;
                n = emit(out, cap, n, ls, i - ls, MT_PREPROC);
            }
        } else if ((c == '*' || c == '_') && i + 1 < len && d[i + 1] == c) {
            unsigned char q = c;
            i += 2;
            while (i + 1 < len && !(d[i] == q && d[i + 1] == q))
                i++;
            if (i + 1 < len)
                i += 2;
            n = emit(out, cap, n, s, i - s, MT_KEYWORD);
        } else if (c == '*' || c == '_') {
            i++;
            while (i < len && d[i] != c)
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_TYPE);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- YAML ---- */

static int lex_yaml(struct mat_hl *h, const unsigned char *d, size_t len,
                    struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    if (len >= 3 && d[0] == '-' && d[1] == '-' && d[2] == '-') {
        n = emit(out, cap, n, 0, len, MT_OPERATOR);
        return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len && d[i] != q)
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == ':' && (i + 1 >= len || d[i + 1] == ' ')) {
            n = emit(out, cap, n, s, 1, MT_OPERATOR);
            i++;
        } else if (c == '-' && (i + 1 >= len || d[i + 1] == ' ')) {
            n = emit(out, cap, n, s, 1, MT_OPERATOR);
            i++;
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_' || d[i] == '-'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (i < len && d[i] == ':')
                t = MT_KEYWORD;
            else if ((wl == 4 && ci_match("true", (const char *)d + s, 4)) ||
                     (wl == 5 && ci_match("false", (const char *)d + s, 5)) ||
                     (wl == 4 && ci_match("null", (const char *)d + s, 4)))
                t = MT_CONSTANT;
            n = emit(out, cap, n, s, wl, t);
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_digit(d[i]) || d[i] == '.' || d[i] == '-'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- TOML ---- */

static int lex_toml(struct mat_hl *h, const unsigned char *d, size_t len,
                    struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '[') {
            i++;
            while (i < len && d[i] != ']')
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_KEYWORD);
        } else if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len && d[i] != q)
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '=') {
            n = emit(out, cap, n, s, 1, MT_OPERATOR);
            i++;
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.' || d[i] == '-' ||
                               d[i] == ':' || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_' || d[i] == '-'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if ((wl == 4 && memcmp(d + s, "true", 4) == 0) ||
                (wl == 5 && memcmp(d + s, "false", 5) == 0))
                t = MT_CONSTANT;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- HTML ---- */

static int lex_html(struct mat_hl *h, const unsigned char *d, size_t len,
                    struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_BLOCK_COMMENT) {
        size_t s = 0;
        while (i + 2 < len) {
            if (d[i] == '-' && d[i + 1] == '-' && d[i + 2] == '>') {
                i += 3;
                h->state = HL_NORMAL;
                break;
            }
            i++;
        }
        if (h->state == HL_BLOCK_COMMENT)
            i = len;
        n = emit(out, cap, n, s, i, MT_COMMENT);
        if (h->state == HL_BLOCK_COMMENT)
            return n;
    }
    while (i < len) {
        size_t s = i;
        if (d[i] == '<' && i + 3 < len && d[i + 1] == '!' && d[i + 2] == '-' &&
            d[i + 3] == '-') {
            h->state = HL_BLOCK_COMMENT;
            i += 4;
            while (i + 2 < len) {
                if (d[i] == '-' && d[i + 1] == '-' && d[i + 2] == '>') {
                    i += 3;
                    h->state = HL_NORMAL;
                    break;
                }
                i++;
            }
            if (h->state == HL_BLOCK_COMMENT)
                i = len;
            n = emit(out, cap, n, s, i - s, MT_COMMENT);
        } else if (d[i] == '<') {
            i++;
            if (i < len && d[i] == '/')
                i++;
            size_t ts = i;
            while (i < len && is_alnum(d[i]))
                i++;
            if (i > ts)
                n = emit(out, cap, n, s, i - s, MT_KEYWORD);
            while (i < len && d[i] != '>') {
                if (d[i] == '"' || d[i] == '\'') {
                    size_t qs = i;
                    unsigned char q = d[i++];
                    while (i < len && d[i] != q)
                        i++;
                    if (i < len)
                        i++;
                    n = emit(out, cap, n, qs, i - qs, MT_STRING);
                } else if (is_word(d[i])) {
                    size_t as = i;
                    while (i < len && (is_alnum(d[i]) || d[i] == '-'))
                        i++;
                    n = emit(out, cap, n, as, i - as, MT_TYPE);
                } else {
                    i++;
                }
            }
            if (i < len) {
                n = emit(out, cap, n, i, 1, MT_KEYWORD);
                i++;
            }
        } else if (d[i] == '&') {
            i++;
            while (i < len && d[i] != ';')
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_CONSTANT);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- CSS ---- */

static int lex_css(struct mat_hl *h, const unsigned char *d, size_t len,
                   struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_BLOCK_COMMENT) {
        size_t s = 0;
        while (i + 1 < len) {
            if (d[i] == '*' && d[i + 1] == '/') {
                i += 2;
                h->state = HL_NORMAL;
                break;
            }
            i++;
        }
        if (h->state == HL_BLOCK_COMMENT)
            i = len;
        n = emit(out, cap, n, s, i, MT_COMMENT);
        if (h->state == HL_BLOCK_COMMENT)
            return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '/' && i + 1 < len && d[i + 1] == '*') {
            h->state = HL_BLOCK_COMMENT;
            i += 2;
            while (i + 1 < len) {
                if (d[i] == '*' && d[i + 1] == '/') {
                    i += 2;
                    h->state = HL_NORMAL;
                    break;
                }
                i++;
            }
            if (h->state == HL_BLOCK_COMMENT)
                i = len;
            n = emit(out, cap, n, s, i - s, MT_COMMENT);
        } else if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len && d[i] != q)
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '#' || c == '.') {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '-' || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_FUNCTION);
        } else if (c == '@') {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '-'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_PREPROC);
        } else if (c == ':') {
            n = emit(out, cap, n, s, 1, MT_OPERATOR);
            i++;
        } else if (c == '{' || c == '}' || c == ';') {
            n = emit(out, cap, n, s, 1, MT_PUNCT);
            i++;
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.' || d[i] == '%'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c) || c == '-') {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '-'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_TEXT);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- SQL ---- */

static int lex_sql(struct mat_hl *h, const unsigned char *d, size_t len,
                   struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_BLOCK_COMMENT) {
        size_t s = 0;
        while (i + 1 < len) {
            if (d[i] == '*' && d[i + 1] == '/') {
                i += 2;
                h->state = HL_NORMAL;
                break;
            }
            i++;
        }
        if (h->state == HL_BLOCK_COMMENT)
            i = len;
        n = emit(out, cap, n, s, i, MT_COMMENT);
        if (h->state == HL_BLOCK_COMMENT)
            return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '-' && i + 1 < len && d[i + 1] == '-') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '/' && i + 1 < len && d[i + 1] == '*') {
            h->state = HL_BLOCK_COMMENT;
            i += 2;
            while (i + 1 < len) {
                if (d[i] == '*' && d[i + 1] == '/') {
                    i += 2;
                    h->state = HL_NORMAL;
                    break;
                }
                i++;
            }
            if (h->state == HL_BLOCK_COMMENT)
                i = len;
            n = emit(out, cap, n, s, i - s, MT_COMMENT);
        } else if (c == '\'') {
            i++;
            while (i < len) {
                if (d[i] == '\'' && i + 1 < len && d[i + 1] == '\'') {
                    i += 2;
                    continue;
                }
                if (d[i] == '\'') {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_digit(d[i]) || d[i] == '.'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has_ci(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has_ci(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            else if ((wl == 4 && ci_match("null", (const char *)d + s, 4)) ||
                     (wl == 4 && ci_match("true", (const char *)d + s, 4)) ||
                     (wl == 5 && ci_match("false", (const char *)d + s, 5)))
                t = MT_CONSTANT;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Ruby ---- */

static int lex_ruby(struct mat_hl *h, const unsigned char *d, size_t len,
                    struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len) {
                if (q == '"' && d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == q) {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == ':' && i + 1 < len && is_word(d[i + 1])) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_CONSTANT);
        } else if (c == '@') {
            i++;
            if (i < len && d[i] == '@')
                i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_PREPROC);
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.' || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_' || d[i] == '?' ||
                               d[i] == '!'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            else if (i < len && (d[i] == '(' || d[i] == ' '))
                t = MT_FUNCTION;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Lua ---- */

static int lex_lua(struct mat_hl *h, const unsigned char *d, size_t len,
                   struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_BLOCK_COMMENT) {
        size_t s = 0;
        while (i + 1 < len) {
            if (d[i] == ']' && d[i + 1] == ']') {
                i += 2;
                h->state = HL_NORMAL;
                break;
            }
            i++;
        }
        if (h->state == HL_BLOCK_COMMENT)
            i = len;
        n = emit(out, cap, n, s, i, MT_COMMENT);
        if (h->state == HL_BLOCK_COMMENT)
            return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '-' && i + 1 < len && d[i + 1] == '-') {
            if (i + 3 < len && d[i + 2] == '[' && d[i + 3] == '[') {
                h->state = HL_BLOCK_COMMENT;
                i += 4;
                while (i + 1 < len) {
                    if (d[i] == ']' && d[i + 1] == ']') {
                        i += 2;
                        h->state = HL_NORMAL;
                        break;
                    }
                    i++;
                }
                if (h->state == HL_BLOCK_COMMENT)
                    i = len;
            } else {
                i = len;
            }
            n = emit(out, cap, n, s, i - s, MT_COMMENT);
        } else if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len) {
                if (d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == q) {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            else if (i < len && (d[i] == '(' || d[i] == '.'))
                t = MT_FUNCTION;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Makefile ---- */

static int lex_makefile(struct mat_hl *h, const unsigned char *d, size_t len,
                        struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    if (len > 0 && d[0] == '\t') {
        n = emit(out, cap, n, 0, len, MT_STRING);
        return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '$') {
            i++;
            if (i < len && (d[i] == '(' || d[i] == '{')) {
                unsigned char close = (unsigned char)(d[i] == '(' ? ')' : '}');
                while (i < len && d[i] != close)
                    i++;
                if (i < len)
                    i++;
            } else {
                if (i < len)
                    i++;
            }
            n = emit(out, cap, n, s, i - s, MT_PREPROC);
        } else if (c == ':' || c == '=' || c == '?') {
            i++;
            if (i < len && d[i] == '=')
                i++;
            n = emit(out, cap, n, s, i - s, MT_OPERATOR);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_' || d[i] == '-' ||
                               d[i] == '.'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_TEXT);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Diff ---- */

static int lex_diff(struct mat_hl *h, const unsigned char *d, size_t len,
                    struct mat_span *out, int cap)
{
    (void)h;
    if (len == 0)
        return 0;
    enum mat_tok t;
    if (d[0] == '+')
        t = MT_STRING;
    else if (d[0] == '-')
        t = MT_KEYWORD;
    else if (d[0] == '@')
        t = MT_PREPROC;
    else if ((len >= 4 && memcmp(d, "diff", 4) == 0) ||
             (len >= 5 && memcmp(d, "index", 5) == 0))
        t = MT_FUNCTION;
    else
        t = MT_TEXT;
    return emit(out, cap, 0, 0, len, t);
}

/* ---- keyword tables ---- */

static const char *const c_kw[] = {
    "_Alignas", "_Alignof", "_Noreturn", "_Static_assert",
    "auto",     "break",    "case",      "continue",
    "default",  "do",       "else",      "enum",
    "extern",   "for",      "goto",      "if",
    "inline",   "register", "restrict",  "return",
    "sizeof",   "static",   "struct",    "switch",
    "typedef",  "union",    "volatile",  "while",
};
static const char *const c_ty[] = {
    "FILE",     "NULL",    "bool",     "char",    "double",   "float",
    "int",      "int16_t", "int32_t",  "int64_t", "int8_t",   "long",
    "short",    "signed",  "size_t",   "ssize_t", "uint16_t", "uint32_t",
    "uint64_t", "uint8_t", "unsigned", "void",
};
static const char *const py_kw[] = {
    "and",      "as",       "assert", "async", "await",  "break",  "class",
    "continue", "def",      "del",    "elif",  "else",   "except", "finally",
    "for",      "from",     "global", "if",    "import", "in",     "is",
    "lambda",   "nonlocal", "not",    "or",    "pass",   "raise",  "return",
    "try",      "while",    "with",   "yield",
};
static const char *const py_ty[] = {
    "False", "None", "True", "bool", "bytes", "dict",  "float",
    "int",   "list", "self", "set",  "str",   "tuple", "type",
};
static const char *const sh_kw[] = {
    "break",    "case", "continue", "declare", "do",       "done",
    "elif",     "else", "esac",     "export",  "fi",       "for",
    "function", "if",   "in",       "local",   "readonly", "return",
    "select",   "then", "until",    "while",
};
static const char *const js_kw[] = {
    "async", "await",    "break",    "case",       "catch",  "class",
    "const", "continue", "debugger", "default",    "delete", "do",
    "else",  "export",   "extends",  "finally",    "for",    "function",
    "if",    "import",   "in",       "instanceof", "let",    "new",
    "of",    "return",   "super",    "switch",     "this",   "throw",
    "try",   "typeof",   "var",      "void",       "while",  "with",
    "yield",
};
static const char *const js_ty[] = {
    "Array", "Boolean", "Infinity", "Map",  "NaN",  "Number",    "Object",
    "Set",   "String",  "false",    "null", "true", "undefined",
};
static const char *const go_kw[] = {
    "break",  "case",        "chan", "const",   "continue", "default", "defer",
    "else",   "fallthrough", "for",  "func",    "go",       "goto",    "if",
    "import", "interface",   "map",  "package", "range",    "return",  "select",
    "struct", "switch",      "type", "var",
};
static const char *const go_ty[] = {
    "bool",    "byte",    "complex128", "complex64", "error",  "false",
    "float32", "float64", "int",        "int16",     "int32",  "int64",
    "int8",    "iota",    "nil",        "rune",      "string", "true",
    "uint",    "uint16",  "uint32",     "uint64",    "uint8",  "uintptr",
};
static const char *const rs_kw[] = {
    "Self",   "as",    "async", "await", "break",  "const", "continue",
    "crate",  "dyn",   "else",  "enum",  "extern", "fn",    "for",
    "if",     "impl",  "in",    "let",   "loop",   "match", "mod",
    "move",   "mut",   "pub",   "ref",   "return", "self",  "static",
    "struct", "super", "trait", "type",  "unsafe", "use",   "where",
    "while",  "yield",
};
static const char *const rs_ty[] = {
    "Box",    "Err",  "None", "Ok",   "Option", "Result", "Some",
    "String", "Vec",  "bool", "char", "f32",    "f64",    "false",
    "i128",   "i16",  "i32",  "i64",  "i8",     "isize",  "str",
    "true",   "u128", "u16",  "u32",  "u64",    "u8",     "usize",
};

static const char *const fortran_kw[] = {
    "abstract",   "allocate", "associate", "block",    "call",     "case",
    "class",      "common",   "contains",  "continue", "cycle",    "data",
    "deallocate", "do",       "elemental", "else",     "elseif",   "end",
    "enddo",      "endif",    "exit",      "forall",   "function", "goto",
    "if",         "implicit", "in",        "inout",    "intent",   "interface",
    "module",     "none",     "optional",  "out",      "program",  "pure",
    "recursive",  "return",   "save",      "select",   "stop",     "subroutine",
    "then",       "type",     "use",       "where",    "while",
};
static const char *const fortran_ty[] = {
    "allocatable", "character", "complex", "dimension", "double",
    "integer",     "kind",      "logical", "parameter", "pointer",
    "precision",   "real",      "target",
};
static const char *const sql_kw[] = {
    "all",      "alter",    "and",    "as",         "begin",      "between",
    "by",       "case",     "commit", "constraint", "create",     "default",
    "delete",   "distinct", "drop",   "else",       "end",        "exists",
    "foreign",  "from",     "group",  "having",     "in",         "index",
    "inner",    "insert",   "into",   "is",         "join",       "key",
    "left",     "like",     "limit",  "not",        "offset",     "on",
    "or",       "order",    "outer",  "primary",    "references", "right",
    "rollback", "select",   "set",    "table",      "then",       "union",
    "update",   "values",   "view",   "when",       "where",      "with",
};
static const char *const sql_ty[] = {
    "bigint",   "blob",  "boolean",   "char",    "date",    "decimal",
    "double",   "float", "int",       "integer", "numeric", "serial",
    "smallint", "text",  "timestamp", "uuid",    "varchar",
};
static const char *const ruby_kw[] = {
    "begin",  "case",   "class",  "def",  "do",    "else",    "elsif",
    "end",    "ensure", "extend", "for",  "if",    "include", "lambda",
    "module", "print",  "proc",   "puts", "raise", "require", "rescue",
    "return", "unless", "until",  "when", "while", "yield",
};
static const char *const ruby_ty[] = {
    "Array",  "Class",  "Float", "Hash", "Integer", "Proc",
    "String", "Symbol", "false", "nil",  "self",    "true",
};
static const char *const lua_kw[] = {
    "and",      "break",  "do",   "else",  "elseif", "end", "for",
    "function", "goto",   "if",   "in",    "local",  "not", "or",
    "repeat",   "return", "then", "until", "while",
};
static const char *const lua_ty[] = {
    "false",
    "nil",
    "true",
};
static const char *const cs_kw[] = {
    "abstract", "as",        "async",     "await",     "base",     "bool",
    "break",    "case",      "catch",     "class",     "const",    "continue",
    "default",  "delegate",  "do",        "else",      "enum",     "event",
    "explicit", "extern",    "finally",   "for",       "foreach",  "goto",
    "if",       "implicit",  "in",        "interface", "internal", "is",
    "lock",     "namespace", "new",       "operator",  "out",      "override",
    "params",   "private",   "protected", "public",    "readonly", "ref",
    "return",   "sealed",    "static",    "struct",    "switch",   "this",
    "throw",    "try",       "typeof",    "using",     "var",      "virtual",
    "void",     "volatile",  "while",     "yield",
};
static const char *const cs_ty[] = {
    "Int32", "String", "bool", "byte", "char",  "decimal", "double",
    "false", "float",  "int",  "long", "null",  "object",  "sbyte",
    "short", "string", "true", "uint", "ulong", "ushort",
};
static const char *const kt_kw[] = {
    "abstract", "as",          "break",    "by",        "class",    "companion",
    "const",    "constructor", "continue", "data",      "do",       "else",
    "enum",     "false",       "finally",  "for",       "fun",      "if",
    "import",   "in",          "init",     "interface", "internal", "is",
    "lateinit", "null",        "object",   "open",      "operator", "out",
    "override", "package",     "private",  "protected", "public",   "return",
    "sealed",   "super",       "suspend",  "this",      "throw",    "true",
    "try",      "typealias",   "val",      "var",       "when",     "while",
};
static const char *const kt_ty[] = {
    "Any", "Boolean", "Byte",    "Char",  "Double", "Float",
    "Int", "Long",    "Nothing", "Short", "String", "Unit",
};
static const char *const scala_kw[] = {
    "abstract",  "case",    "catch",    "class",    "def",     "do",
    "else",      "extends", "false",    "final",    "finally", "for",
    "forSome",   "if",      "implicit", "import",   "lazy",    "match",
    "new",       "null",    "object",   "override", "package", "private",
    "protected", "return",  "sealed",   "super",    "this",    "throw",
    "trait",     "true",    "try",      "type",     "val",     "var",
    "while",     "with",    "yield",
};
static const char *const scala_ty[] = {
    "Any", "AnyRef", "AnyVal",  "Boolean", "Byte",  "Char",   "Double", "Float",
    "Int", "Long",   "Nothing", "Null",    "Short", "String", "Unit",
};
static const char *const swift_kw[] = {
    "associatedtype",
    "break",
    "case",
    "catch",
    "class",
    "continue",
    "default",
    "defer",
    "deinit",
    "do",
    "else",
    "enum",
    "extension",
    "fallthrough",
    "fileprivate",
    "for",
    "func",
    "guard",
    "if",
    "import",
    "in",
    "init",
    "inout",
    "internal",
    "is",
    "let",
    "open",
    "operator",
    "private",
    "protocol",
    "public",
    "repeat",
    "rethrows",
    "return",
    "self",
    "static",
    "struct",
    "subscript",
    "super",
    "switch",
    "throw",
    "throws",
    "try",
    "typealias",
    "var",
    "where",
    "while",
};
static const char *const swift_ty[] = {
    "Any",   "Bool",   "Character", "Double", "Float",  "Int",
    "Int16", "Int32",  "Int64",     "Int8",   "Never",  "Optional",
    "Self",  "String", "UInt",      "UInt16", "UInt32", "UInt64",
    "UInt8", "Void",   "false",     "nil",    "true",
};
static const char *const dart_kw[] = {
    "abstract", "as",        "assert",   "async",      "await",    "break",
    "case",     "catch",     "class",    "const",      "continue", "default",
    "deferred", "do",        "dynamic",  "else",       "enum",     "export",
    "extends",  "extension", "external", "factory",    "final",    "finally",
    "for",      "get",       "if",       "implements", "import",   "in",
    "is",       "late",      "library",  "mixin",      "new",      "on",
    "operator", "part",      "required", "rethrow",    "return",   "sealed",
    "set",      "show",      "static",   "super",      "switch",   "sync",
    "this",     "throw",     "try",      "typedef",    "var",      "void",
    "while",    "with",      "yield",
};
static const char *const dart_ty[] = {
    "Future", "List",   "Map",  "Null",   "Object",  "Set",
    "Stream", "String", "bool", "double", "dynamic", "false",
    "int",    "null",   "num",  "true",   "void",
};
static const char *const perl_kw[] = {
    "chomp",   "chop",   "die",     "do",      "else",   "elsif",
    "eval",    "for",    "foreach", "given",   "goto",   "if",
    "last",    "local",  "my",      "next",    "no",     "our",
    "package", "print",  "redo",    "require", "return", "say",
    "sub",     "unless", "until",   "use",     "when",   "while",
};
static const char *const php_kw[] = {
    "abstract",  "and",        "as",        "break",      "case",
    "catch",     "class",      "clone",     "const",      "continue",
    "declare",   "default",    "do",        "echo",       "else",
    "elseif",    "empty",      "endfor",    "endforeach", "endif",
    "endswitch", "endwhile",   "eval",      "exit",       "extends",
    "final",     "finally",    "fn",        "for",        "foreach",
    "function",  "global",     "goto",      "if",         "implements",
    "include",   "instanceof", "interface", "isset",      "list",
    "match",     "namespace",  "new",       "or",         "print",
    "private",   "protected",  "public",    "readonly",   "require",
    "return",    "static",     "switch",    "this",       "throw",
    "trait",     "try",        "unset",     "use",        "var",
    "while",     "xor",        "yield",
};
static const char *const php_ty[] = {
    "FALSE",  "NULL",  "TRUE",   "array",    "bool",  "callable",
    "false",  "float", "int",    "iterable", "mixed", "null",
    "object", "self",  "string", "true",     "void",
};
static const char *const haskell_kw[] = {
    "case",    "class",  "data",      "default",  "deriving", "do",
    "else",    "forall", "foreign",   "if",       "import",   "in",
    "infix",   "infixl", "infixr",    "instance", "let",      "module",
    "newtype", "of",     "qualified", "then",     "type",     "where",
};
static const char *const haskell_ty[] = {
    "Bool",    "Char", "Double", "Either", "False",   "Float", "IO",     "Int",
    "Integer", "Just", "Left",   "Maybe",  "Nothing", "Right", "String", "True",
};
static const char *const elixir_kw[] = {
    "after",     "alias",   "and",      "case",      "catch", "cond",
    "def",       "defimpl", "defmacro", "defmodule", "defp",  "defprotocol",
    "defstruct", "do",      "else",     "end",       "fn",    "for",
    "if",        "import",  "in",       "not",       "or",    "quote",
    "raise",     "receive", "require",  "rescue",    "try",   "unless",
    "unquote",   "use",     "when",     "with",
};
static const char *const elixir_ty[] = {
    "false",
    "nil",
    "self",
    "true",
};
static const char *const erlang_kw[] = {
    "after", "and",    "andalso", "begin", "case", "catch",
    "end",   "fun",    "if",      "let",   "not",  "of",
    "or",    "orelse", "receive", "try",   "when",
};
static const char *const erlang_ty[] = {
    "false",
    "true",
    "undefined",
};
static const char *const r_kw[] = {
    "break", "else",   "for",     "function", "if",     "in",    "library",
    "next",  "repeat", "require", "return",   "source", "while",
};
static const char *const r_ty[] = {
    "FALSE",       "Inf",      "NA",   "NA_character_", "NA_complex_",
    "NA_integer_", "NA_real_", "NULL", "NaN",           "TRUE",
};
static const char *const zig_kw[] = {
    "addrspace", "align",  "and",      "asm",       "break",       "catch",
    "comptime",  "const",  "continue", "defer",     "else",        "enum",
    "errdefer",  "error",  "export",   "extern",    "fn",          "for",
    "if",        "inline", "noalias",  "nosuspend", "or",          "orelse",
    "packed",    "pub",    "resume",   "return",    "struct",      "suspend",
    "switch",    "test",   "try",      "union",     "unreachable", "var",
    "volatile",  "while",
};
static const char *const zig_ty[] = {
    "anyopaque", "bool", "f128", "f16", "f32", "f64",       "f80",   "false",
    "i128",      "i16",  "i32",  "i64", "i8",  "isize",     "null",  "true",
    "u128",      "u16",  "u32",  "u64", "u8",  "undefined", "usize", "void",
};
static const char *const ocaml_kw[] = {
    "and",      "as",      "assert",  "begin",       "class",   "constraint",
    "do",       "done",    "downto",  "else",        "end",     "exception",
    "external", "for",     "fun",     "function",    "functor", "if",
    "in",       "include", "inherit", "initializer", "lazy",    "let",
    "match",    "method",  "module",  "mutable",     "new",     "object",
    "of",       "open",    "or",      "private",     "rec",     "sig",
    "struct",   "then",    "to",      "try",         "type",    "val",
    "virtual",  "when",    "while",   "with",
};
static const char *const ocaml_ty[] = {
    "None", "Some", "array",  "bool", "char",   "false", "float",
    "int",  "list", "option", "ref",  "string", "true",  "unit",
};
static const char *const clojure_kw[] = {
    "and",     "case",  "catch",   "cond", "def",  "defmacro", "defn",
    "defonce", "do",    "finally", "fn",   "if",   "import",   "in-ns",
    "let",     "loop",  "not",     "ns",   "or",   "recur",    "refer",
    "require", "throw", "try",     "use",  "when",
};
static const char *const clojure_ty[] = {
    "false",
    "nil",
    "true",
};
static const char *const julia_kw[] = {
    "abstract",  "baremodule", "begin",    "break",  "catch",  "const",
    "continue",  "do",         "else",     "elseif", "end",    "export",
    "finally",   "for",        "function", "global", "if",     "import",
    "in",        "let",        "local",    "macro",  "module", "mutable",
    "primitive", "quote",      "return",   "struct", "try",    "type",
    "using",     "where",      "while",
};
static const char *const julia_ty[] = {
    "Any",     "Bool",   "Char",  "Float16", "Float32", "Float64",
    "Int",     "Int128", "Int16", "Int32",   "Int64",   "Int8",
    "Nothing", "String", "UInt",  "UInt128", "UInt16",  "UInt32",
    "UInt64",  "UInt8",  "false", "nothing", "true",
};
static const char *const nim_kw[] = {
    "addr",      "and",     "as",        "asm",      "bind",   "block",
    "break",     "case",    "cast",      "concept",  "const",  "continue",
    "converter", "defer",   "discard",   "distinct", "div",    "do",
    "elif",      "else",    "end",       "enum",     "except", "export",
    "finally",   "for",     "from",      "func",     "if",     "import",
    "in",        "include", "interface", "is",       "isnot",  "iterator",
    "let",       "macro",   "method",    "mixin",    "mod",    "nil",
    "not",       "notin",   "object",    "of",       "or",     "out",
    "proc",      "ptr",     "raise",     "ref",      "return", "shl",
    "shr",       "static",  "template",  "try",      "tuple",  "type",
    "using",     "var",     "when",      "while",    "xor",    "yield",
};
static const char *const nim_ty[] = {
    "array",   "bool",   "char",   "false", "float", "float32",
    "float64", "int",    "int16",  "int32", "int64", "int8",
    "nil",     "seq",    "string", "true",  "uint",  "uint16",
    "uint32",  "uint64", "uint8",  "void",
};
static const char *const groovy_kw[] = {
    "abstract",     "as",     "assert",     "break",    "case",    "catch",
    "class",        "const",  "continue",   "def",      "default", "do",
    "else",         "enum",   "extends",    "final",    "finally", "for",
    "goto",         "if",     "implements", "import",   "in",      "instanceof",
    "interface",    "native", "new",        "package",  "private", "protected",
    "public",       "return", "static",     "strictfp", "super",   "switch",
    "synchronized", "this",   "throw",      "throws",   "trait",   "try",
    "while",
};
static const char *const groovy_ty[] = {
    "List", "Map",  "Set",    "String", "boolean", "byte",
    "char", "def",  "double", "false",  "float",   "int",
    "long", "null", "short",  "true",   "void",
};
static const char *const powershell_kw[] = {
    "begin",   "break",    "catch",        "class",   "continue", "data",
    "define",  "do",       "dynamicparam", "else",    "elseif",   "end",
    "enum",    "exit",     "filter",       "finally", "for",      "foreach",
    "from",    "function", "hidden",       "if",      "in",       "param",
    "process", "return",   "static",       "switch",  "throw",    "trap",
    "try",     "until",    "using",        "while",
};
static const char *const awk_kw[] = {
    "BEGIN",    "END",   "break",    "continue", "delete", "do", "else",
    "exit",     "for",   "function", "getline",  "if",     "in", "next",
    "nextfile", "print", "printf",   "return",   "while",
};
static const char *const fish_kw[] = {
    "and",  "begin",  "break", "builtin",  "case",   "command", "continue",
    "else", "end",    "for",   "function", "if",     "in",      "not",
    "or",   "return", "set",   "status",   "switch", "test",    "while",
};
static const char *const d_kw[] = {
    "abstract",     "alias",     "asm",       "assert",    "body",
    "break",        "case",      "cast",      "catch",     "class",
    "const",        "continue",  "debug",     "default",   "delegate",
    "delete",       "do",        "else",      "enum",      "export",
    "extern",       "final",     "finally",   "for",       "foreach",
    "function",     "goto",      "if",        "immutable", "import",
    "in",           "interface", "invariant", "is",        "lazy",
    "mixin",        "module",    "new",       "nothrow",   "out",
    "override",     "package",   "pragma",    "private",   "protected",
    "public",       "pure",      "ref",       "return",    "scope",
    "shared",       "static",    "struct",    "super",     "switch",
    "synchronized", "template",  "this",      "throw",     "try",
    "typeid",       "typeof",    "union",     "unittest",  "version",
    "void",         "while",     "with",
};
static const char *const d_ty[] = {
    "bool",   "byte",  "cdouble", "cfloat",  "char",   "creal",  "dchar",
    "double", "false", "float",   "idouble", "ifloat", "int",    "ireal",
    "long",   "null",  "real",    "short",   "size_t", "string", "true",
    "ubyte",  "uint",  "ulong",   "ushort",  "wchar",
};
static const char *const fsharp_kw[] = {
    "abstract", "and",     "as",       "assert",    "base",      "begin",
    "class",    "default", "delegate", "do",        "done",      "downcast",
    "downto",   "elif",    "else",     "end",       "exception", "extern",
    "finally",  "for",     "fun",      "function",  "global",    "if",
    "in",       "inherit", "inline",   "interface", "internal",  "lazy",
    "let",      "match",   "member",   "module",    "mutable",   "namespace",
    "new",      "not",     "null",     "of",        "open",      "or",
    "override", "private", "public",   "rec",       "return",    "select",
    "static",   "struct",  "then",     "to",        "try",       "type",
    "upcast",   "use",     "val",      "void",      "when",      "while",
    "with",     "yield",
};
static const char *const fsharp_ty[] = {
    "None",   "Some",   "bool",      "byte",    "char",   "decimal",
    "double", "false",  "float",     "float32", "int",    "int16",
    "int32",  "int64",  "nativeint", "sbyte",   "single", "string",
    "true",   "uint16", "uint32",    "uint64",  "unit",
};
static const char *const glsl_kw[] = {
    "attribute",  "break",  "case",      "const",     "continue", "default",
    "discard",    "do",     "else",      "flat",      "for",      "highp",
    "if",         "in",     "inout",     "invariant", "layout",   "lowp",
    "mediump",    "out",    "precision", "return",    "smooth",   "struct",
    "subroutine", "switch", "uniform",   "varying",   "while",
};
static const char *const glsl_ty[] = {
    "bool",      "bvec2",       "bvec3", "bvec4", "double", "dvec2",
    "dvec3",     "dvec4",       "false", "float", "int",    "ivec2",
    "ivec3",     "ivec4",       "mat2",  "mat3",  "mat4",   "sampler2D",
    "sampler3D", "samplerCube", "true",  "uint",  "uvec2",  "uvec3",
    "uvec4",     "vec2",        "vec3",  "vec4",  "void",
};
static const char *const coffee_kw[] = {
    "and",    "break",      "by",    "case",    "catch",   "class",  "continue",
    "delete", "do",         "else",  "extends", "finally", "for",    "if",
    "in",     "instanceof", "is",    "isnt",    "loop",    "new",    "no",
    "not",    "of",         "or",    "return",  "super",   "switch", "then",
    "this",   "throw",      "try",   "typeof",  "unless",  "until",  "when",
    "while",  "yes",        "yield",
};
static const char *const coffee_ty[] = {
    "Infinity", "NaN", "false", "null", "true", "undefined",
};
static const char *const crystal_kw[] = {
    "abstract",  "alias",
    "as",        "asm",
    "begin",     "break",
    "case",      "class",
    "def",       "do",
    "else",      "elsif",
    "end",       "ensure",
    "enum",      "extend",
    "for",       "fun",
    "if",        "in",
    "include",   "instance_sizeof",
    "is_a?",     "lib",
    "macro",     "module",
    "next",      "nil?",
    "of",        "out",
    "pointerof", "private",
    "protected", "puts",
    "raise",     "require",
    "rescue",    "return",
    "select",    "sizeof",
    "struct",    "super",
    "then",      "type",
    "typeof",    "uninitialized",
    "union",     "unless",
    "until",     "when",
    "while",     "with",
    "yield",
};
static const char *const crystal_ty[] = {
    "Bool",  "Char", "Float32", "Float64", "Int16",  "Int32",  "Int64",
    "Int8",  "Nil",  "String",  "Symbol",  "UInt16", "UInt32", "UInt64",
    "UInt8", "Void", "false",   "nil",     "self",   "true",
};
static const char *const elm_kw[] = {
    "alias", "as",     "case", "else", "exposing", "if",   "import", "in",
    "let",   "module", "of",   "port", "then",     "type", "where",
};
static const char *const elm_ty[] = {
    "Bool", "Char",  "False", "Float",   "Int",    "Just",
    "List", "Maybe", "Never", "Nothing", "String", "True",
};
static const char *const solidity_kw[] = {
    "abstract", "break",    "case",    "catch",    "constant",  "constructor",
    "continue", "contract", "default", "delete",   "do",        "else",
    "emit",     "enum",     "event",   "external", "fallback",  "for",
    "function", "if",       "import",  "indexed",  "interface", "internal",
    "is",       "library",  "mapping", "memory",   "modifier",  "new",
    "override", "payable",  "pragma",  "private",  "public",    "pure",
    "receive",  "require",  "return",  "returns",  "revert",    "storage",
    "struct",   "super",    "this",    "throw",    "try",       "using",
    "view",     "virtual",  "while",
};
static const char *const solidity_ty[] = {
    "address", "bool",   "bytes",   "false",  "int",    "int128", "int16",
    "int256",  "int32",  "int64",   "int8",   "string", "true",   "uint",
    "uint128", "uint16", "uint256", "uint32", "uint64", "uint8",
};
static const char *const ada_kw[] = {
    "abort",   "abs",          "abstract",  "accept",     "access",
    "aliased", "all",          "and",       "array",      "at",
    "begin",   "body",         "case",      "constant",   "declare",
    "delay",   "delta",        "digits",    "do",         "else",
    "elsif",   "end",          "entry",     "exception",  "exit",
    "for",     "function",     "generic",   "goto",       "if",
    "in",      "interface",    "is",        "limited",    "loop",
    "mod",     "new",          "not",       "null",       "of",
    "or",      "others",       "out",       "overriding", "package",
    "pragma",  "private",      "procedure", "protected",  "raise",
    "range",   "record",       "rem",       "renames",    "requeue",
    "return",  "reverse",      "select",    "separate",   "some",
    "subtype", "synchronized", "tagged",    "task",       "terminate",
    "then",    "type",         "until",     "use",        "when",
    "while",   "with",         "xor",
};
static const char *const ada_ty[] = {
    "boolean", "character", "duration", "false",  "float",
    "integer", "natural",   "positive", "string", "true",
};
static const char *const pascal_kw[] = {
    "and",
    "array",
    "begin",
    "case",
    "const",
    "constructor",
    "destructor",
    "div",
    "do",
    "downto",
    "else",
    "end",
    "except",
    "finally",
    "for",
    "function",
    "goto",
    "if",
    "implementation",
    "in",
    "inherited",
    "interface",
    "is",
    "mod",
    "not",
    "object",
    "of",
    "on",
    "operator",
    "or",
    "packed",
    "procedure",
    "program",
    "property",
    "raise",
    "record",
    "repeat",
    "set",
    "shl",
    "shr",
    "then",
    "to",
    "try",
    "type",
    "unit",
    "until",
    "uses",
    "var",
    "while",
    "with",
    "xor",
};
static const char *const pascal_ty[] = {
    "boolean",  "byte",     "cardinal", "char",  "comp",     "currency",
    "double",   "extended", "false",    "int64", "integer",  "longint",
    "longword", "nil",      "pointer",  "real",  "shortint", "single",
    "smallint", "string",   "true",     "word",
};
static const char *const matlab_kw[] = {
    "break",  "case",   "catch",     "classdef", "continue",
    "else",   "elseif", "end",       "for",      "function",
    "global", "if",     "otherwise", "parfor",   "persistent",
    "return", "spmd",   "switch",    "try",      "while",
};
static const char *const matlab_ty[] = {
    "Inf", "NaN", "false", "inf", "nan", "pi", "true",
};
static const char *const protobuf_kw[] = {
    "enum",     "extend",   "extensions", "import",  "map",     "message",
    "oneof",    "option",   "optional",   "package", "public",  "repeated",
    "required", "reserved", "returns",    "rpc",     "service", "stream",
    "syntax",   "to",       "weak",
};
static const char *const protobuf_ty[] = {
    "bool",   "bytes",  "double", "false",    "fixed32",  "fixed64",
    "float",  "int32",  "int64",  "sfixed32", "sfixed64", "sint32",
    "sint64", "string", "true",   "uint32",   "uint64",
};
static const char *const terraform_kw[] = {
    "count",    "data",     "depends_on", "dynamic",  "each",
    "for",      "for_each", "if",         "in",       "lifecycle",
    "locals",   "module",   "output",     "provider", "provisioner",
    "resource", "self",     "terraform",  "variable",
};
static const char *const terraform_ty[] = {
    "any",    "bool",   "false", "list",   "map",  "null",
    "number", "object", "set",   "string", "true", "tuple",
};
static const char *const nix_kw[] = {
    "assert",  "builtins", "else", "if",   "import", "in",
    "inherit", "let",      "rec",  "then", "with",
};
static const char *const nix_ty[] = {
    "false",
    "null",
    "true",
};
static const char *const tcl_kw[] = {
    "after",    "append", "break",   "case",    "catch",     "continue",
    "else",     "elseif", "error",   "eval",    "exec",      "exit",
    "expr",     "for",    "foreach", "format",  "global",    "if",
    "incr",     "info",   "join",    "lappend", "lindex",    "list",
    "llength",  "lrange", "lsearch", "lsort",   "namespace", "open",
    "package",  "proc",   "puts",    "read",    "regexp",    "rename",
    "return",   "set",    "source",  "split",   "string",    "switch",
    "then",     "time",   "trace",   "unset",   "uplevel",   "upvar",
    "variable", "while",
};
static const char *const lisp_kw[] = {
    "and",          "begin",  "case",    "cond",       "define", "defmacro",
    "defun",        "do",     "else",    "if",         "lambda", "let",
    "let*",         "letrec", "or",      "quasiquote", "quote",  "set!",
    "syntax-rules", "unless", "unquote", "when",
};
static const char *const lisp_ty[] = {
    "#f",
    "#t",
    "nil",
    "t",
};
static const char *const batch_kw[] = {
    "call",  "cls",      "cmd",      "color",      "copy",  "del",  "dir",
    "echo",  "else",     "endlocal", "errorlevel", "exist", "exit", "for",
    "goto",  "if",       "md",       "mkdir",      "move",  "not",  "path",
    "pause", "popd",     "pushd",    "rd",         "rem",   "ren",  "rmdir",
    "set",   "setlocal", "shift",    "start",      "title", "type",
};
static const char *const dockerfile_kw[] = {
    "add",        "arg",        "as",      "cmd",     "copy",
    "entrypoint", "env",        "expose",  "from",    "healthcheck",
    "label",      "maintainer", "onbuild", "run",     "shell",
    "stopsignal", "user",       "volume",  "workdir",
};
static const char *const graphql_kw[] = {
    "directive", "enum",      "extend",       "fragment", "implements",
    "input",     "interface", "mutation",     "on",       "query",
    "scalar",    "schema",    "subscription", "type",     "union",
};
static const char *const graphql_ty[] = {
    "Boolean", "Float", "ID", "Int", "String", "false", "null", "true",
};
static const char *const cmake_kw[] = {
    "add_executable",
    "add_library",
    "add_subdirectory",
    "cmake_minimum_required",
    "else",
    "elseif",
    "enable_testing",
    "endforeach",
    "endfunction",
    "endif",
    "endmacro",
    "endwhile",
    "find_package",
    "foreach",
    "function",
    "if",
    "include",
    "install",
    "macro",
    "message",
    "option",
    "project",
    "return",
    "set",
    "target_compile_definitions",
    "target_compile_options",
    "target_include_directories",
    "target_link_libraries",
    "while",
};
static const char *const nginx_kw[] = {
    "access_log",
    "error_log",
    "error_page",
    "events",
    "http",
    "if",
    "include",
    "index",
    "listen",
    "location",
    "proxy_pass",
    "return",
    "rewrite",
    "root",
    "server",
    "server_name",
    "set",
    "ssl_certificate",
    "ssl_certificate_key",
    "upstream",
    "worker_processes",
};
static const char *const viml_kw[] = {
    "augroup",  "autocmd", "call",     "command",     "echo",   "echom",
    "else",     "elseif",  "endfor",   "endfunction", "endif",  "endwhile",
    "execute",  "finish",  "for",      "function",    "if",     "let",
    "map",      "nmap",    "nnoremap", "noremap",     "return", "set",
    "setlocal", "silent",  "source",   "syntax",      "while",
};
static const char *const qml_kw[] = {
    "as",       "break",  "case",   "catch",      "continue", "default",
    "delete",   "do",     "else",   "finally",    "for",      "function",
    "if",       "import", "in",     "instanceof", "new",      "property",
    "readonly", "return", "signal", "switch",     "this",     "throw",
    "try",      "typeof", "var",    "void",       "while",    "with",
};
static const char *const qml_ty[] = {
    "alias", "bool", "color",  "date", "double",    "false", "int", "list",
    "null",  "real", "string", "true", "undefined", "url",   "var", "variant",
};
static const char *const actionscript_kw[] = {
    "break",      "case",    "catch",  "class",      "const",     "continue",
    "default",    "delete",  "do",     "dynamic",    "else",      "extends",
    "final",      "finally", "for",    "function",   "get",       "if",
    "implements", "import",  "in",     "instanceof", "interface", "internal",
    "is",         "native",  "new",    "override",   "package",   "private",
    "protected",  "public",  "return", "set",        "static",    "super",
    "switch",     "this",    "throw",  "try",        "typeof",    "use",
    "var",        "void",    "while",  "with",
};
static const char *const actionscript_ty[] = {
    "Array", "Boolean", "Class",  "Date",   "Function",  "Infinity",
    "NaN",   "Number",  "Object", "String", "XML",       "false",
    "int",   "null",    "true",   "uint",   "undefined",
};
static const char *const applescript_kw[] = {
    "about",       "after",   "and",      "as",    "before", "begin",  "by",
    "considering", "copy",    "div",      "does",  "else",   "end",    "error",
    "every",       "exit",    "first",    "from",  "get",    "global", "if",
    "ignoring",    "in",      "instead",  "is",    "it",     "its",    "last",
    "local",       "me",      "mod",      "my",    "not",    "of",     "on",
    "or",          "prop",    "property", "put",   "ref",    "repeat", "return",
    "run",         "set",     "some",     "tell",  "that",   "the",    "then",
    "through",     "to",      "try",      "until", "where",  "while",  "whose",
    "with",        "without",
};
static const char *const wgsl_kw[] = {
    "bitcast",  "break",      "case",     "const",      "const_assert",
    "continue", "continuing", "default",  "diagnostic", "discard",
    "else",     "enable",     "fn",       "for",        "if",
    "let",      "loop",       "override", "return",     "struct",
    "switch",   "var",        "while",
};
static const char *const wgsl_ty[] = {
    "array",      "atomic", "bool",   "f16",    "f32",  "false",
    "i32",        "mat2x2", "mat3x3", "mat4x4", "ptr",  "sampler",
    "texture_2d", "true",   "u32",    "vec2",   "vec3", "vec4",
};
static const char *const lean_kw[] = {
    "abbrev",    "axiom",     "by",       "calc",       "class",
    "constant",  "def",       "deriving", "do",         "else",
    "end",       "example",   "extends",  "fun",        "have",
    "if",        "import",    "in",       "inductive",  "instance",
    "let",       "match",     "mutual",   "namespace",  "noncomputable",
    "notation",  "opaque",    "open",     "partial",    "private",
    "protected", "return",    "section",  "set_option", "show",
    "sorry",     "structure", "suffices", "tactic",     "then",
    "theorem",   "universe",  "variable", "where",      "with",
};
static const char *const lean_ty[] = {
    "Bool",   "Char", "False",  "Float", "IO",   "Int",  "List",  "Nat",
    "Option", "Prop", "String", "True",  "Type", "Unit", "false", "true",
};
static const char *const puppet_kw[] = {
    "and",     "case",     "class", "default", "define", "else",
    "elsif",   "fail",     "false", "if",      "import", "in",
    "include", "inherits", "node",  "notify",  "or",     "realize",
    "require", "tag",      "true",  "undef",   "unless",
};
static const char *const rego_kw[] = {
    "as",   "default", "else", "false", "import", "not",
    "null", "package", "set",  "some",  "true",   "with",
};
static const char *const jsonnet_kw[] = {
    "assert", "else",   "error",      "false", "for",   "function",
    "if",     "import", "importstr",  "in",    "local", "null",
    "self",   "super",  "tailstrict", "then",  "true",
};

#define WS(arr)                                                                \
    (struct wordset)                                                           \
    {                                                                          \
        arr, (int)(sizeof(arr) / sizeof(arr[0]))                               \
    }

/* ---- Perl / PHP (# or // comments, $vars, strings) ---- */

static int lex_perish(struct mat_hl *h, const unsigned char *d, size_t len,
                      struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_BLOCK_COMMENT) {
        size_t s = 0;
        while (i + 1 < len) {
            if (d[i] == '*' && d[i + 1] == '/') {
                i += 2;
                h->state = HL_NORMAL;
                break;
            }
            i++;
        }
        if (h->state == HL_BLOCK_COMMENT)
            i = len;
        n = emit(out, cap, n, s, i, MT_COMMENT);
        if (h->state == HL_BLOCK_COMMENT)
            return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if ((c == '#' || (c == '/' && i + 1 < len && d[i + 1] == '/')) &&
            h->state == HL_NORMAL) {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '/' && i + 1 < len && d[i + 1] == '*') {
            h->state = HL_BLOCK_COMMENT;
            i += 2;
            while (i + 1 < len) {
                if (d[i] == '*' && d[i + 1] == '/') {
                    i += 2;
                    h->state = HL_NORMAL;
                    break;
                }
                i++;
            }
            if (h->state == HL_BLOCK_COMMENT)
                i = len;
            n = emit(out, cap, n, s, i - s, MT_COMMENT);
        } else if (c == '$' || (c == '@' && i + 1 < len && is_word(d[i + 1]))) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_PREPROC);
        } else if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len) {
                if (d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == q) {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.' || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            else if (i < len && d[i] == '(')
                t = MT_FUNCTION;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Haskell / OCaml (-- line comments, {- -} block) ---- */

static int lex_haskell(struct mat_hl *h, const unsigned char *d, size_t len,
                       struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_BLOCK_COMMENT) {
        size_t s = 0;
        while (i + 1 < len) {
            if (d[i] == '-' && d[i + 1] == '}') {
                i += 2;
                h->state = HL_NORMAL;
                break;
            }
            i++;
        }
        if (h->state == HL_BLOCK_COMMENT)
            i = len;
        n = emit(out, cap, n, s, i, MT_COMMENT);
        if (h->state == HL_BLOCK_COMMENT)
            return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '-' && i + 1 < len && d[i + 1] == '-') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '{' && i + 1 < len && d[i + 1] == '-') {
            h->state = HL_BLOCK_COMMENT;
            i += 2;
            while (i + 1 < len) {
                if (d[i] == '-' && d[i + 1] == '}') {
                    i += 2;
                    h->state = HL_NORMAL;
                    break;
                }
                i++;
            }
            if (h->state == HL_BLOCK_COMMENT)
                i = len;
            n = emit(out, cap, n, s, i - s, MT_COMMENT);
        } else if (c == '"') {
            i++;
            while (i < len) {
                if (d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == '"') {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '\'') {
            i++;
            if (i < len && d[i] == '\\')
                i++;
            if (i < len)
                i++;
            if (i < len && d[i] == '\'')
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_' || d[i] == '\''))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Clojure (; comments, keywords as :word) ---- */

static int lex_clojure(struct mat_hl *h, const unsigned char *d, size_t len,
                       struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == ';') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '"') {
            i++;
            while (i < len) {
                if (d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == '"') {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == ':') {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '-' || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_CONSTANT);
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.' || d[i] == '/'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c) || c == '-' || c == '+' || c == '*' || c == '!' ||
                   c == '?') {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '-' || d[i] == '_' ||
                               d[i] == '!' || d[i] == '?' || d[i] == '*'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- R (# comments, <- assignment) ---- */

static int lex_r(struct mat_hl *h, const unsigned char *d, size_t len,
                 struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len) {
                if (d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == q) {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '<' && i + 1 < len && d[i + 1] == '-') {
            i += 2;
            n = emit(out, cap, n, s, 2, MT_OPERATOR);
        } else if (is_digit(c) ||
                   (c == '.' && i + 1 < len && is_digit(d[i + 1]))) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c) || c == '.') {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_' || d[i] == '.'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            else if (i < len && d[i] == '(')
                t = MT_FUNCTION;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Dockerfile ---- */

static int lex_dockerfile(struct mat_hl *h, const unsigned char *d, size_t len,
                          struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len && d[i] != q)
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '$') {
            i++;
            if (i < len && d[i] == '{') {
                while (i < len && d[i] != '}')
                    i++;
                if (i < len)
                    i++;
            } else {
                while (i < len && is_alnum(d[i]))
                    i++;
            }
            n = emit(out, cap, n, s, i - s, MT_PREPROC);
        } else if (is_word(c)) {
            i++;
            while (i < len && is_alnum(d[i]))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has_ci(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- INI / .conf ---- */

static int lex_ini(struct mat_hl *h, const unsigned char *d, size_t len,
                   struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    if (len > 0 && (d[0] == ';' || d[0] == '#')) {
        n = emit(out, cap, n, 0, len, MT_COMMENT);
        return n;
    }
    if (len > 0 && d[0] == '[') {
        n = emit(out, cap, n, 0, len, MT_KEYWORD);
        return n;
    }
    while (i < len) {
        if (d[i] == '=') {
            n = emit(out, cap, n, 0, i, MT_TYPE);
            n = emit(out, cap, n, i, 1, MT_OPERATOR);
            if (i + 1 < len)
                n = emit(out, cap, n, i + 1, len - i - 1, MT_STRING);
            return n;
        }
        i++;
    }
    n = emit(out, cap, n, 0, len, MT_TEXT);
    return n;
}

/* ---- LaTeX ---- */

static int lex_latex(struct mat_hl *h, const unsigned char *d, size_t len,
                     struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '%') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '\\') {
            i++;
            while (i < len && is_word(d[i]))
                i++;
            n = emit(out, cap, n, s, i - s, MT_KEYWORD);
        } else if (c == '{' || c == '}') {
            i++;
            n = emit(out, cap, n, s, 1, MT_PUNCT);
        } else if (c == '$') {
            i++;
            while (i < len && d[i] != '$')
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '[' || c == ']') {
            i++;
            n = emit(out, cap, n, s, 1, MT_OPERATOR);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Pascal ({ } block comments, // line comments) ---- */

static int lex_pascal(struct mat_hl *h, const unsigned char *d, size_t len,
                      struct mat_span *out, int cap)
{
    int n = 0;
    size_t i = 0;
    if (h->state == HL_BLOCK_COMMENT) {
        size_t s = 0;
        while (i < len) {
            if (d[i] == '}') {
                i++;
                h->state = HL_NORMAL;
                break;
            }
            i++;
        }
        if (h->state == HL_BLOCK_COMMENT)
            i = len;
        n = emit(out, cap, n, s, i, MT_COMMENT);
        if (h->state == HL_BLOCK_COMMENT)
            return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '/' && i + 1 < len && d[i + 1] == '/') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '{') {
            h->state = HL_BLOCK_COMMENT;
            i++;
            while (i < len) {
                if (d[i] == '}') {
                    i++;
                    h->state = HL_NORMAL;
                    break;
                }
                i++;
            }
            if (h->state == HL_BLOCK_COMMENT)
                i = len;
            n = emit(out, cap, n, s, i - s, MT_COMMENT);
        } else if (c == '\'') {
            i++;
            while (i < len) {
                if (d[i] == '\'' && i + 1 < len && d[i + 1] == '\'') {
                    i += 2;
                    continue;
                }
                if (d[i] == '\'') {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c) || (c == '$' && i + 1 < len)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has_ci(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has_ci(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            else if (i < len && d[i] == '(')
                t = MT_FUNCTION;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- MATLAB (% comments) ---- */

static int lex_matlab(struct mat_hl *h, const unsigned char *d, size_t len,
                      struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '%') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '\'') {
            i++;
            while (i < len && d[i] != '\'')
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '"') {
            i++;
            while (i < len && d[i] != '"')
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c) ||
                   (c == '.' && i + 1 < len && is_digit(d[i + 1]))) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.' || d[i] == '+' ||
                               d[i] == '-'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            else if (ws_has(&h->types, (const char *)d + s, wl))
                t = MT_TYPE;
            else if (i < len && d[i] == '(')
                t = MT_FUNCTION;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Assembly (; or # comments) ---- */

static int lex_asm(struct mat_hl *h, const unsigned char *d, size_t len,
                   struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == ';' || c == '#') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '"' || c == '\'') {
            unsigned char q = c;
            i++;
            while (i < len && d[i] != q)
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '.' && i + 1 < len && is_word(d[i + 1])) {
            i++;
            while (i < len && is_alnum(d[i]))
                i++;
            n = emit(out, cap, n, s, i - s, MT_PREPROC);
        } else if (is_digit(c) || (c == '0' && i + 1 < len &&
                                   (d[i + 1] == 'x' || d[i + 1] == 'b'))) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (i < len && d[i] == ':')
                t = MT_FUNCTION;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Batch File (REM and :: comments, %var%) ---- */

static int lex_batch(struct mat_hl *h, const unsigned char *d, size_t len,
                     struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    if (len >= 3 && (ci_match("rem", (const char *)d, 3)) &&
        (len == 3 || d[3] == ' ' || d[3] == '\t')) {
        n = emit(out, cap, n, 0, len, MT_COMMENT);
        return n;
    }
    if (len >= 2 && d[0] == ':' && d[1] == ':') {
        n = emit(out, cap, n, 0, len, MT_COMMENT);
        return n;
    }
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '%') {
            i++;
            if (i < len && d[i] == '%')
                i++;
            while (i < len && d[i] != '%' && d[i] != ' ')
                i++;
            if (i < len && d[i] == '%')
                i++;
            n = emit(out, cap, n, s, i - s, MT_PREPROC);
        } else if (c == '"') {
            i++;
            while (i < len && d[i] != '"')
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has_ci(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            n = emit(out, cap, n, s, wl, t);
        } else if (c == ':' && i + 1 < len && is_word(d[i + 1])) {
            i++;
            while (i < len && is_alnum(d[i]))
                i++;
            n = emit(out, cap, n, s, i - s, MT_FUNCTION);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- VimL (" comments) ---- */

static int lex_viml(struct mat_hl *h, const unsigned char *d, size_t len,
                    struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '"') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '\'') {
            i++;
            while (i < len && d[i] != '\'')
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_' || d[i] == ':' ||
                               d[i] == '#'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if (ws_has(&h->keywords, (const char *)d + s, wl))
                t = MT_KEYWORD;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Groff/troff/manpage (.command directives) ---- */

static int lex_groff(struct mat_hl *h, const unsigned char *d, size_t len,
                     struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    if (len > 0 && d[0] == '.') {
        size_t i = 1;
        while (i < len && is_word(d[i]))
            i++;
        n = emit(out, cap, n, 0, i, MT_KEYWORD);
        if (i < len)
            n = emit(out, cap, n, i, len - i, MT_TEXT);
        return n;
    }
    if (len > 0 && d[0] == '\\') {
        n = emit(out, cap, n, 0, len, MT_PREPROC);
        return n;
    }
    n = emit(out, cap, n, 0, len, MT_TEXT);
    return n;
}

/* ---- BibTeX (@type{key, field=value}) ---- */

static int lex_bibtex(struct mat_hl *h, const unsigned char *d, size_t len,
                      struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '%') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '@') {
            i++;
            while (i < len && is_word(d[i]))
                i++;
            n = emit(out, cap, n, s, i - s, MT_KEYWORD);
        } else if (c == '"') {
            i++;
            while (i < len && d[i] != '"')
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '{' || c == '}') {
            i++;
            n = emit(out, cap, n, s, 1, MT_PUNCT);
        } else if (c == '=') {
            i++;
            n = emit(out, cap, n, s, 1, MT_OPERATOR);
        } else if (is_digit(c)) {
            i++;
            while (i < len && is_digit(d[i]))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_' || d[i] == '-'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_TEXT);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Git files (# comments, pick/reword/fixup commands for rebase) ---- */

static int lex_gitcommit(struct mat_hl *h, const unsigned char *d, size_t len,
                         struct mat_span *out, int cap)
{
    (void)h;
    if (len > 0 && d[0] == '#')
        return emit(out, cap, 0, 0, len, MT_COMMENT);
    return emit(out, cap, 0, 0, len, MT_TEXT);
}

static const char *const git_rebase_kw[] = {
    "break", "drop", "edit",  "exec",   "fixup",  "label",
    "merge", "pick", "reset", "reword", "squash",
};

static int lex_gitrebase(struct mat_hl *h, const unsigned char *d, size_t len,
                         struct mat_span *out, int cap)
{
    (void)h;
    if (len == 0)
        return 0;
    if (d[0] == '#')
        return emit(out, cap, 0, 0, len, MT_COMMENT);
    int n = 0;
    size_t i = 0;
    if (is_word(d[0])) {
        while (i < len && is_word(d[i]))
            i++;
        size_t wl = i;
        enum mat_tok t = MT_TEXT;
        if (ws_has(&h->keywords, (const char *)d, wl))
            t = MT_KEYWORD;
        n = emit(out, cap, n, 0, wl, t);
        while (i < len && d[i] == ' ')
            i++;
        if (i < len) {
            size_t hs = i;
            while (i < len && is_alnum(d[i]))
                i++;
            if (i > hs)
                n = emit(out, cap, n, hs, i - hs, MT_CONSTANT);
        }
        if (i < len)
            n = emit(out, cap, n, i, len - i, MT_TEXT);
    } else {
        n = emit(out, cap, n, 0, len, MT_TEXT);
    }
    return n;
}

/* ---- SSH/system config (# comments, Keyword Value) ---- */

static int lex_sshconfig(struct mat_hl *h, const unsigned char *d, size_t len,
                         struct mat_span *out, int cap)
{
    (void)h;
    if (len > 0 && (d[0] == '#' || d[0] == ';'))
        return emit(out, cap, 0, 0, len, MT_COMMENT);
    int n = 0;
    size_t i = 0;
    while (i < len && !is_word(d[i]) && d[i] != '#')
        i++;
    if (i < len && is_word(d[i])) {
        size_t s = i;
        while (i < len && is_word(d[i]))
            i++;
        n = emit(out, cap, n, s, i - s, MT_KEYWORD);
    }
    while (i < len && (d[i] == ' ' || d[i] == '\t' || d[i] == '='))
        i++;
    if (i < len)
        n = emit(out, cap, n, i, len - i, MT_STRING);
    return n;
}

/* ---- /etc files: colon-delimited (passwd, group), whitespace (fstab,
 *      hosts), crontab ---- */

static int lex_colonfile(struct mat_hl *h, const unsigned char *d, size_t len,
                         struct mat_span *out, int cap)
{
    (void)h;
    if (len > 0 && d[0] == '#')
        return emit(out, cap, 0, 0, len, MT_COMMENT);
    int n = 0, field = 0;
    size_t s = 0;
    for (size_t i = 0; i <= len; i++) {
        if (i == len || d[i] == ':') {
            enum mat_tok t = (field == 0) ? MT_KEYWORD : MT_TEXT;
            if (i > s)
                n = emit(out, cap, n, s, i - s, t);
            if (i < len)
                n = emit(out, cap, n, i, 1, MT_OPERATOR);
            s = i + 1;
            field++;
        }
    }
    return n;
}

/* ---- Strace output: syscall(args) = result ---- */

static int lex_strace(struct mat_hl *h, const unsigned char *d, size_t len,
                      struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len && (is_alnum(d[i]) || d[i] == '_'))
        i++;
    if (i > 0 && i < len && d[i] == '(')
        n = emit(out, cap, n, 0, i, MT_FUNCTION);
    else if (i > 0)
        n = emit(out, cap, n, 0, i, MT_TEXT);
    while (i < len) {
        size_t s = i;
        if (d[i] == '"') {
            i++;
            while (i < len) {
                if (d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == '"') {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (d[i] == '=' && i + 1 < len && d[i + 1] == ' ') {
            n = emit(out, cap, n, i, 1, MT_OPERATOR);
            i++;
        } else if (is_digit(d[i]) ||
                   (d[i] == '-' && i + 1 < len && is_digit(d[i + 1])) ||
                   (d[i] == '0' && i + 1 < len && d[i + 1] == 'x')) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '.'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Log/syslog (timestamp + level coloring) ---- */

static int lex_log(struct mat_hl *h, const unsigned char *d, size_t len,
                   struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len &&
           (is_digit(d[i]) || d[i] == '-' || d[i] == ':' || d[i] == '.' ||
            d[i] == 'T' || d[i] == 'Z' || d[i] == '+'))
        i++;
    if (i > 4)
        n = emit(out, cap, n, 0, i, MT_NUMBER);
    while (i < len) {
        size_t s = i;
        if (is_word(d[i])) {
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if ((wl == 5 && ci_match("ERROR", (const char *)d + s, 5)) ||
                (wl == 5 && ci_match("FATAL", (const char *)d + s, 5)) ||
                (wl == 4 && ci_match("FAIL", (const char *)d + s, 4)) ||
                (wl == 8 && ci_match("CRITICAL", (const char *)d + s, 8)))
                t = MT_KEYWORD;
            else if ((wl == 4 && ci_match("WARN", (const char *)d + s, 4)) ||
                     (wl == 7 && ci_match("WARNING", (const char *)d + s, 7)))
                t = MT_TYPE;
            else if ((wl == 4 && ci_match("INFO", (const char *)d + s, 4)))
                t = MT_FUNCTION;
            else if ((wl == 5 && ci_match("DEBUG", (const char *)d + s, 5)) ||
                     (wl == 5 && ci_match("TRACE", (const char *)d + s, 5)))
                t = MT_COMMENT;
            n = emit(out, cap, n, s, wl, t);
        } else if (d[i] == '"') {
            i++;
            while (i < len && d[i] != '"')
                i++;
            if (i < len)
                i++;
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Todo.txt (x completed, (A) priority, +project, @context) ---- */

static int lex_todotxt(struct mat_hl *h, const unsigned char *d, size_t len,
                       struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    if (len >= 2 && d[0] == 'x' && d[1] == ' ')
        return emit(out, cap, 0, 0, len, MT_COMMENT);
    size_t i = 0;
    if (len >= 3 && d[0] == '(' && d[2] == ')') {
        n = emit(out, cap, n, 0, 3, MT_KEYWORD);
        i = 3;
    }
    while (i < len) {
        size_t s = i;
        if (d[i] == '+' || d[i] == '@') {
            i++;
            while (i < len && !is_digit(d[i]) && d[i] != ' ' && d[i] != '\t')
                i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_' || d[i] == '-'))
                i++;
            n = emit(out, cap, n, s, i - s,
                     d[s] == '+' ? MT_FUNCTION : MT_PREPROC);
        } else if (is_digit(d[i]) && (i == 0 || d[i - 1] == ' ') &&
                   i + 10 <= len && d[i + 4] == '-') {
            while (i < len && (is_digit(d[i]) || d[i] == '-'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- VimHelp (*tags*, |links|, > code) ---- */

static int lex_vimhelp(struct mat_hl *h, const unsigned char *d, size_t len,
                       struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    if (len > 0 && d[0] == '>') {
        n = emit(out, cap, n, 0, len, MT_STRING);
        return n;
    }
    while (i < len) {
        size_t s = i;
        if (d[i] == '*') {
            i++;
            while (i < len && d[i] != '*' && d[i] != ' ')
                i++;
            if (i < len && d[i] == '*')
                i++;
            n = emit(out, cap, n, s, i - s, MT_KEYWORD);
        } else if (d[i] == '|') {
            i++;
            while (i < len && d[i] != '|' && d[i] != ' ')
                i++;
            if (i < len && d[i] == '|')
                i++;
            n = emit(out, cap, n, s, i - s, MT_FUNCTION);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Verilog/SystemVerilog (C-family with HDL keywords) ---- */

static const char *const verilog_kw[] = {
    "always",      "and",         "assign",    "automatic",    "begin",
    "buf",         "case",        "casex",     "casez",        "default",
    "defparam",    "disable",     "else",      "end",          "endcase",
    "endfunction", "endgenerate", "endmodule", "endprimitive", "endspecify",
    "endtable",    "endtask",     "event",     "for",          "forever",
    "fork",        "function",    "generate",  "genvar",       "if",
    "initial",     "inout",       "input",     "join",         "localparam",
    "macromodule", "module",      "nand",      "negedge",      "nor",
    "not",         "or",          "output",    "parameter",    "posedge",
    "primitive",   "reg",         "repeat",    "specify",      "table",
    "task",        "while",       "wire",      "xnor",         "xor",
};
static const char *const verilog_ty[] = {
    "integer", "real", "realtime", "supply0", "supply1", "time", "tri",
    "tri0",    "tri1", "triand",   "trior",   "wand",    "wor",
};
static const char *const sv_kw[] = {
    "always",      "always_comb", "always_ff",    "always_latch", "and",
    "assert",      "assign",      "automatic",    "begin",        "break",
    "case",        "class",       "clocking",     "constraint",   "continue",
    "covergroup",  "coverpoint",  "cross",        "default",      "do",
    "else",        "end",         "endcase",      "endclass",     "endclocking",
    "endfunction", "endgenerate", "endinterface", "endmodule",    "endpackage",
    "endproperty", "endsequence", "endtask",      "enum",         "extends",
    "extern",      "final",       "for",          "foreach",      "forever",
    "fork",        "function",    "generate",     "if",           "import",
    "initial",     "input",       "interface",    "join",         "local",
    "localparam",  "module",      "negedge",      "new",          "output",
    "package",     "parameter",   "posedge",      "priority",     "program",
    "property",    "protected",   "pure",         "rand",         "ref",
    "repeat",      "return",      "sequence",     "static",       "struct",
    "super",       "task",        "this",         "typedef",      "union",
    "unique",      "var",         "virtual",      "void",         "while",
    "wire",        "with",
};
static const char *const sv_ty[] = {
    "bit",  "byte",     "chandle", "int",      "integer",   "logic",  "longint",
    "real", "realtime", "reg",     "shortint", "shortreal", "string", "time",
};

/* ---- Crontab (timing fields + command) ---- */

static int lex_crontab(struct mat_hl *h, const unsigned char *d, size_t len,
                       struct mat_span *out, int cap)
{
    (void)h;
    if (len > 0 && d[0] == '#')
        return emit(out, cap, 0, 0, len, MT_COMMENT);
    int n = 0;
    size_t i = 0;
    int field = 0;
    while (i < len && field < 5) {
        while (i < len && (d[i] == ' ' || d[i] == '\t'))
            i++;
        size_t s = i;
        while (i < len && d[i] != ' ' && d[i] != '\t')
            i++;
        if (i > s)
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        field++;
    }
    while (i < len && (d[i] == ' ' || d[i] == '\t'))
        i++;
    if (i < len)
        n = emit(out, cap, n, i, len - i, MT_TEXT);
    return n;
}

/* ---- HTTP Request/Response (METHOD URL, Header: value) ---- */

static int lex_http(struct mat_hl *h, const unsigned char *d, size_t len,
                    struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    if (len >= 4 && (memcmp(d, "GET ", 4) == 0 || memcmp(d, "PUT ", 4) == 0 ||
                     memcmp(d, "POST", 4) == 0 || memcmp(d, "HEAD", 4) == 0 ||
                     memcmp(d, "DELE", 4) == 0 || memcmp(d, "PATC", 4) == 0 ||
                     memcmp(d, "OPTI", 4) == 0 || memcmp(d, "HTTP", 4) == 0)) {
        while (i < len && d[i] != ' ' && d[i] != '\t')
            i++;
        n = emit(out, cap, n, 0, i, MT_KEYWORD);
        if (i < len)
            n = emit(out, cap, n, i, len - i, MT_TEXT);
        return n;
    }
    while (i < len && d[i] != ':' && d[i] != ' ')
        i++;
    if (i < len && d[i] == ':') {
        n = emit(out, cap, n, 0, i, MT_TYPE);
        n = emit(out, cap, n, i, 1, MT_OPERATOR);
        if (i + 1 < len)
            n = emit(out, cap, n, i + 1, len - i - 1, MT_STRING);
        return n;
    }
    return emit(out, cap, 0, 0, len, MT_TEXT);
}

/* ---- Ninja build (# comments, rule/build keywords) ---- */

static const char *const ninja_kw[] = {
    "build", "default", "include", "pool", "rule", "subninja",
};

/* ---- NSIS (; or # comments, !directives, Section) ---- */

static const char *const nsis_kw[] = {
    "call",        "clearerrors", "createdirectory",
    "delete",      "detailprint", "exec",
    "execwait",    "file",        "function",
    "functionend", "goto",        "iferrors",
    "intcmp",      "messagebox",  "quit",
    "return",      "rmdir",       "section",
    "sectionend",  "setoutpath",  "strcmp",
    "strcpy",      "var",
};

/* ---- JQ (# comments, .field, pipes) ---- */

static int lex_jq(struct mat_hl *h, const unsigned char *d, size_t len,
                  struct mat_span *out, int cap)
{
    (void)h;
    int n = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        unsigned char c = d[i];
        if (c == '#') {
            n = emit(out, cap, n, s, len - s, MT_COMMENT);
            return n;
        }
        if (c == '"') {
            i++;
            while (i < len) {
                if (d[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (d[i] == '"') {
                    i++;
                    break;
                }
                i++;
            }
            n = emit(out, cap, n, s, i - s, MT_STRING);
        } else if (c == '.') {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_FUNCTION);
        } else if (c == '|') {
            n = emit(out, cap, n, s, 1, MT_OPERATOR);
            i++;
        } else if (is_digit(c)) {
            i++;
            while (i < len && (is_digit(d[i]) || d[i] == '.'))
                i++;
            n = emit(out, cap, n, s, i - s, MT_NUMBER);
        } else if (is_word(c)) {
            i++;
            while (i < len && (is_alnum(d[i]) || d[i] == '_'))
                i++;
            size_t wl = i - s;
            enum mat_tok t = MT_TEXT;
            if ((wl == 2 && memcmp(d + s, "if", 2) == 0) ||
                (wl == 4 && memcmp(d + s, "then", 4) == 0) ||
                (wl == 4 && memcmp(d + s, "else", 4) == 0) ||
                (wl == 3 && memcmp(d + s, "end", 3) == 0) ||
                (wl == 3 && memcmp(d + s, "def", 3) == 0) ||
                (wl == 2 && memcmp(d + s, "as", 2) == 0) ||
                (wl == 3 && memcmp(d + s, "try", 3) == 0) ||
                (wl == 5 && memcmp(d + s, "catch", 5) == 0) ||
                (wl == 6 && memcmp(d + s, "reduce", 6) == 0) ||
                (wl == 7 && memcmp(d + s, "foreach", 7) == 0) ||
                (wl == 6 && memcmp(d + s, "import", 6) == 0))
                t = MT_KEYWORD;
            else if ((wl == 4 && memcmp(d + s, "null", 4) == 0) ||
                     (wl == 4 && memcmp(d + s, "true", 4) == 0) ||
                     (wl == 5 && memcmp(d + s, "false", 5) == 0))
                t = MT_CONSTANT;
            n = emit(out, cap, n, s, wl, t);
        } else {
            i++;
        }
    }
    return n;
}

/* ---- Literate Haskell (> prefixed code lines) ---- */

static int lex_lhaskell(struct mat_hl *h, const unsigned char *d, size_t len,
                        struct mat_span *out, int cap)
{
    if (len >= 2 && d[0] == '>' && d[1] == ' ') {
        int n = emit(out, cap, 0, 0, 2, MT_OPERATOR);
        struct mat_span sub[256];
        int sn = lex_haskell(h, d + 2, len - 2, sub, 256);
        for (int i = 0; i < sn && n < cap; i++) {
            out[n] = sub[i];
            out[n].start += 2;
            n++;
        }
        return n;
    }
    return emit(out, cap, 0, 0, len, MT_COMMENT);
}

/* ---- dispatch ---- */

struct lang_entry {
    const char *name;
    lex_fn lex;
    struct wordset kw, ty;
};

static const struct lang_entry lang_tbl[] = {
    {"ARM Assembly", lex_asm, {NULL, 0}, {NULL, 0}},
    {"ASP", lex_html, {NULL, 0}, {NULL, 0}},
    {"AWK", lex_shell, WS(awk_kw), {NULL, 0}},
    {"ActionScript", lex_cfamily, WS(actionscript_kw), WS(actionscript_ty)},
    {"Ada", lex_haskell, WS(ada_kw), WS(ada_ty)},
    {"Apache Conf", lex_python, WS(nginx_kw), {NULL, 0}},
    {"AppleScript", lex_haskell, WS(applescript_kw), {NULL, 0}},
    {"AsciiDoc", lex_markdown, {NULL, 0}, {NULL, 0}},
    {"AsciiDoc (Asciidoctor)", lex_markdown, {NULL, 0}, {NULL, 0}},
    {"Assembly", lex_asm, {NULL, 0}, {NULL, 0}},
    {"Assembly (x86_64)", lex_asm, {NULL, 0}, {NULL, 0}},
    {"Authorized Keys", lex_sshconfig, {NULL, 0}, {NULL, 0}},
    {"Bash", lex_shell, WS(sh_kw), {NULL, 0}},
    {"Batch File", lex_batch, WS(batch_kw), {NULL, 0}},
    {"BibTeX", lex_bibtex, {NULL, 0}, {NULL, 0}},
    {"Bourne Again Shell (bash)", lex_shell, WS(sh_kw), {NULL, 0}},
    {"C", lex_cfamily, WS(c_kw), WS(c_ty)},
    {"C#", lex_cfamily, WS(cs_kw), WS(cs_ty)},
    {"C++", lex_cfamily, WS(c_kw), WS(c_ty)},
    {"CFML", lex_html, {NULL, 0}, {NULL, 0}},
    {"CMake", lex_python, WS(cmake_kw), {NULL, 0}},
    {"CMake C Header", lex_python, WS(cmake_kw), {NULL, 0}},
    {"CMake C++ Header", lex_python, WS(cmake_kw), {NULL, 0}},
    {"CMakeCache", lex_python, WS(cmake_kw), {NULL, 0}},
    {"CSS", lex_css, {NULL, 0}, {NULL, 0}},
    {"CSV", lex_colonfile, {NULL, 0}, {NULL, 0}},
    {"Cabal", lex_sshconfig, {NULL, 0}, {NULL, 0}},
    {"Clojure", lex_clojure, WS(clojure_kw), WS(clojure_ty)},
    {"CoffeeScript", lex_python, WS(coffee_kw), WS(coffee_ty)},
    {"Comma Separated Values", lex_colonfile, {NULL, 0}, {NULL, 0}},
    {"CpuInfo", lex_sshconfig, {NULL, 0}, {NULL, 0}},
    {"Crontab", lex_crontab, {NULL, 0}, {NULL, 0}},
    {"Crystal", lex_ruby, WS(crystal_kw), WS(crystal_ty)},
    {"D", lex_cfamily, WS(d_kw), WS(d_ty)},
    {"Dart", lex_cfamily, WS(dart_kw), WS(dart_ty)},
    {"Diff", lex_diff, {NULL, 0}, {NULL, 0}},
    {"Dockerfile", lex_dockerfile, WS(dockerfile_kw), {NULL, 0}},
    {"DotENV", lex_ini, {NULL, 0}, {NULL, 0}},
    {"Elixir", lex_python, WS(elixir_kw), WS(elixir_ty)},
    {"Elm", lex_haskell, WS(elm_kw), WS(elm_ty)},
    {"Email", lex_http, {NULL, 0}, {NULL, 0}},
    {"Erlang", lex_python, WS(erlang_kw), WS(erlang_ty)},
    {"F#", lex_haskell, WS(fsharp_kw), WS(fsharp_ty)},
    {"Fish", lex_shell, WS(fish_kw), {NULL, 0}},
    {"Fortran", lex_fortran, WS(fortran_kw), WS(fortran_ty)},
    {"GLSL", lex_cfamily, WS(glsl_kw), WS(glsl_ty)},
    {"Git Attributes", lex_gitcommit, {NULL, 0}, {NULL, 0}},
    {"Git Commit", lex_gitcommit, {NULL, 0}, {NULL, 0}},
    {"Git Config", lex_ini, {NULL, 0}, {NULL, 0}},
    {"Git Ignore", lex_gitcommit, {NULL, 0}, {NULL, 0}},
    {"Git Link", lex_gitcommit, {NULL, 0}, {NULL, 0}},
    {"Git Log", lex_gitcommit, {NULL, 0}, {NULL, 0}},
    {"Git Mailmap", lex_gitcommit, {NULL, 0}, {NULL, 0}},
    {"Git Rebase Todo", lex_gitrebase, WS(git_rebase_kw), {NULL, 0}},
    {"Go", lex_cfamily, WS(go_kw), WS(go_ty)},
    {"GraphQL", lex_python, WS(graphql_kw), WS(graphql_ty)},
    {"Graphviz", lex_cfamily, {NULL, 0}, {NULL, 0}},
    {"Graphviz (DOT)", lex_cfamily, {NULL, 0}, {NULL, 0}},
    {"Groff", lex_groff, {NULL, 0}, {NULL, 0}},
    {"Groff/troff", lex_groff, {NULL, 0}, {NULL, 0}},
    {"Groovy", lex_cfamily, WS(groovy_kw), WS(groovy_ty)},
    {"HTML", lex_html, {NULL, 0}, {NULL, 0}},
    {"HTML (ASP)", lex_html, {NULL, 0}, {NULL, 0}},
    {"HTML (EEx)", lex_html, {NULL, 0}, {NULL, 0}},
    {"HTML (Erlang)", lex_html, {NULL, 0}, {NULL, 0}},
    {"HTML (Jinja2)", lex_html, {NULL, 0}, {NULL, 0}},
    {"HTML (Rails)", lex_html, {NULL, 0}, {NULL, 0}},
    {"HTML (Tcl)", lex_html, {NULL, 0}, {NULL, 0}},
    {"HTML (Twig)", lex_html, {NULL, 0}, {NULL, 0}},
    {"HTTP Request and Response", lex_http, {NULL, 0}, {NULL, 0}},
    {"Haskell", lex_haskell, WS(haskell_kw), WS(haskell_ty)},
    {"INI", lex_ini, {NULL, 0}, {NULL, 0}},
    {"JQ", lex_jq, {NULL, 0}, {NULL, 0}},
    {"JSON", lex_json, {NULL, 0}, {NULL, 0}},
    {"JSX", lex_cfamily, WS(js_kw), WS(js_ty)},
    {"Java", lex_cfamily, WS(c_kw), WS(c_ty)},
    {"Java Properties", lex_ini, {NULL, 0}, {NULL, 0}},
    {"Java Server Page (JSP)", lex_html, {NULL, 0}, {NULL, 0}},
    {"JavaScript", lex_cfamily, WS(js_kw), WS(js_ty)},
    {"JavaScript (Babel)", lex_cfamily, WS(js_kw), WS(js_ty)},
    {"JavaScript (Rails)", lex_cfamily, WS(js_kw), WS(js_ty)},
    {"Jinja2", lex_html, {NULL, 0}, {NULL, 0}},
    {"Julia", lex_python, WS(julia_kw), WS(julia_ty)},
    {"Known Hosts", lex_sshconfig, {NULL, 0}, {NULL, 0}},
    {"Kotlin", lex_cfamily, WS(kt_kw), WS(kt_ty)},
    {"LLVM", lex_asm, {NULL, 0}, {NULL, 0}},
    {"LaTeX", lex_latex, {NULL, 0}, {NULL, 0}},
    {"Lean", lex_haskell, WS(lean_kw), WS(lean_ty)},
    {"Less", lex_css, {NULL, 0}, {NULL, 0}},
    {"Lisp", lex_clojure, WS(lisp_kw), WS(lisp_ty)},
    {"Literate Haskell", lex_lhaskell, WS(haskell_kw), WS(haskell_ty)},
    {"LiveScript", lex_python, WS(coffee_kw), WS(coffee_ty)},
    {"Lua", lex_lua, WS(lua_kw), WS(lua_ty)},
    {"MATLAB", lex_matlab, WS(matlab_kw), WS(matlab_ty)},
    {"Makefile", lex_makefile, {NULL, 0}, {NULL, 0}},
    {"Manpage", lex_groff, {NULL, 0}, {NULL, 0}},
    {"Markdown", lex_markdown, {NULL, 0}, {NULL, 0}},
    {"MediaWiki", lex_markdown, {NULL, 0}, {NULL, 0}},
    {"MemInfo", lex_sshconfig, {NULL, 0}, {NULL, 0}},
    {"NAnt Build File", lex_html, {NULL, 0}, {NULL, 0}},
    {"NSIS", lex_perish, WS(nsis_kw), {NULL, 0}},
    {"Nim", lex_cfamily, WS(nim_kw), WS(nim_ty)},
    {"Ninja", lex_python, WS(ninja_kw), {NULL, 0}},
    {"Nix", lex_python, WS(nix_kw), WS(nix_ty)},
    {"OCaml", lex_haskell, WS(ocaml_kw), WS(ocaml_ty)},
    {"OCamllex", lex_haskell, WS(ocaml_kw), WS(ocaml_ty)},
    {"OCamlyacc", lex_haskell, WS(ocaml_kw), WS(ocaml_ty)},
    {"Objective-C", lex_cfamily, WS(c_kw), WS(c_ty)},
    {"Objective-C++", lex_cfamily, WS(c_kw), WS(c_ty)},
    {"PHP", lex_perish, WS(php_kw), WS(php_ty)},
    {"Pascal", lex_pascal, WS(pascal_kw), WS(pascal_ty)},
    {"Perl", lex_perish, WS(perl_kw), {NULL, 0}},
    {"PowerShell", lex_shell, WS(powershell_kw), {NULL, 0}},
    {"Protobuf", lex_cfamily, WS(protobuf_kw), WS(protobuf_ty)},
    {"Protocol Buffer (TEXT)", lex_cfamily, WS(protobuf_kw), WS(protobuf_ty)},
    {"Puppet", lex_python, WS(puppet_kw), {NULL, 0}},
    {"PureScript", lex_haskell, WS(haskell_kw), WS(haskell_ty)},
    {"Python", lex_python, WS(py_kw), WS(py_ty)},
    {"QML", lex_cfamily, WS(qml_kw), WS(qml_ty)},
    {"R", lex_r, WS(r_kw), WS(r_ty)},
    {"Racket", lex_clojure, WS(lisp_kw), WS(lisp_ty)},
    {"Rd (R Documentation)", lex_latex, {NULL, 0}, {NULL, 0}},
    {"Rego", lex_python, WS(rego_kw), {NULL, 0}},
    {"Regular Expression", lex_cfamily, {NULL, 0}, {NULL, 0}},
    {"Requirements.txt", lex_ini, {NULL, 0}, {NULL, 0}},
    {"Robot Framework", lex_python, {NULL, 0}, {NULL, 0}},
    {"Ruby", lex_ruby, WS(ruby_kw), WS(ruby_ty)},
    {"Ruby Haml", lex_ruby, WS(ruby_kw), WS(ruby_ty)},
    {"Ruby Slim", lex_ruby, WS(ruby_kw), WS(ruby_ty)},
    {"Ruby on Rails", lex_ruby, WS(ruby_kw), WS(ruby_ty)},
    {"Rust", lex_cfamily, WS(rs_kw), WS(rs_ty)},
    {"SCSS", lex_css, {NULL, 0}, {NULL, 0}},
    {"SML", lex_haskell, WS(ocaml_kw), WS(ocaml_ty)},
    {"SQL", lex_sql, WS(sql_kw), WS(sql_ty)},
    {"SQL (Rails)", lex_sql, WS(sql_kw), WS(sql_ty)},
    {"SSH Config", lex_sshconfig, {NULL, 0}, {NULL, 0}},
    {"SSHD Config", lex_sshconfig, {NULL, 0}, {NULL, 0}},
    {"Salt State (SLS)", lex_yaml, {NULL, 0}, {NULL, 0}},
    {"Sass", lex_css, {NULL, 0}, {NULL, 0}},
    {"Scala", lex_cfamily, WS(scala_kw), WS(scala_ty)},
    {"Solidity", lex_cfamily, WS(solidity_kw), WS(solidity_ty)},
    {"Strace", lex_strace, {NULL, 0}, {NULL, 0}},
    {"Stylus", lex_css, {NULL, 0}, {NULL, 0}},
    {"Svelte", lex_html, {NULL, 0}, {NULL, 0}},
    {"Swift", lex_cfamily, WS(swift_kw), WS(swift_ty)},
    {"SystemVerilog", lex_cfamily, WS(sv_kw), WS(sv_ty)},
    {"TOML", lex_toml, {NULL, 0}, {NULL, 0}},
    {"Tcl", lex_shell, WS(tcl_kw), {NULL, 0}},
    {"TeX", lex_latex, {NULL, 0}, {NULL, 0}},
    {"Terraform", lex_cfamily, WS(terraform_kw), WS(terraform_ty)},
    {"Textile", lex_markdown, {NULL, 0}, {NULL, 0}},
    {"Todo.txt", lex_todotxt, {NULL, 0}, {NULL, 0}},
    {"TypeScript", lex_cfamily, WS(js_kw), WS(js_ty)},
    {"TypeScriptReact", lex_cfamily, WS(js_kw), WS(js_ty)},
    {"Verilog", lex_cfamily, WS(verilog_kw), WS(verilog_ty)},
    {"VimHelp", lex_vimhelp, {NULL, 0}, {NULL, 0}},
    {"VimL", lex_viml, WS(viml_kw), {NULL, 0}},
    {"Vue", lex_html, {NULL, 0}, {NULL, 0}},
    {"Vue Component", lex_html, {NULL, 0}, {NULL, 0}},
    {"Vyper", lex_python, WS(rego_kw), {NULL, 0}},
    {"WGSL", lex_cfamily, WS(wgsl_kw), WS(wgsl_ty)},
    {"XML", lex_html, {NULL, 0}, {NULL, 0}},
    {"YAML", lex_yaml, {NULL, 0}, {NULL, 0}},
    {"Zig", lex_cfamily, WS(zig_kw), WS(zig_ty)},
    {"Zsh", lex_shell, WS(sh_kw), {NULL, 0}},
    {"fstab", lex_sshconfig, {NULL, 0}, {NULL, 0}},
    {"gnuplot", lex_python, {NULL, 0}, {NULL, 0}},
    {"group", lex_colonfile, {NULL, 0}, {NULL, 0}},
    {"hosts", lex_sshconfig, {NULL, 0}, {NULL, 0}},
    {"jsonnet", lex_cfamily, WS(jsonnet_kw), {NULL, 0}},
    {"log", lex_log, {NULL, 0}, {NULL, 0}},
    {"nginx", lex_python, WS(nginx_kw), {NULL, 0}},
    {"orgmode", lex_markdown, {NULL, 0}, {NULL, 0}},
    {"passwd", lex_colonfile, {NULL, 0}, {NULL, 0}},
    {"reStructuredText", lex_markdown, {NULL, 0}, {NULL, 0}},
    {"resolv", lex_sshconfig, {NULL, 0}, {NULL, 0}},
    {"syslog", lex_log, {NULL, 0}, {NULL, 0}},
    {"varlink", lex_cfamily, {NULL, 0}, {NULL, 0}},
};

#define LANG_TBL_N (sizeof lang_tbl / sizeof lang_tbl[0])

static int lang_cmp(const void *a, const void *b)
{
    return strcmp((const char *)a, ((const struct lang_entry *)b)->name);
}

struct mat_hl *mat_hl_open(const char *syntax)
{
    if (syntax == NULL)
        return NULL;
    const struct lang_entry *e =
        bsearch(syntax, lang_tbl, LANG_TBL_N, sizeof lang_tbl[0], lang_cmp);
    if (e == NULL)
        return NULL;
    struct mat_hl *h = calloc(1, sizeof *h);
    if (h != NULL) {
        h->lex = e->lex;
        h->keywords = e->kw;
        h->types = e->ty;
    }
    return h;
}

void mat_hl_close(struct mat_hl *h)
{
    free(h);
}

static const char *const all_langs[] = {
    "ActionScript",
    "Ada",
    "Apache Conf",
    "AppleScript",
    "AsciiDoc",
    "Assembly",
    "AWK",
    "Bash",
    "Batch File",
    "BibTeX",
    "C",
    "C#",
    "C++",
    "Cabal",
    "CFML",
    "Clojure",
    "CMake",
    "CMakeCache",
    "CoffeeScript",
    "Crontab",
    "Crystal",
    "CSS",
    "CSV",
    "D",
    "Dart",
    "Diff",
    "Dockerfile",
    "DotENV",
    "Elixir",
    "Elm",
    "Email",
    "Erlang",
    "F#",
    "Fish",
    "Fortran",
    "Git Commit",
    "Git Config",
    "Git Rebase Todo",
    "GLSL",
    "gnuplot",
    "Go",
    "GraphQL",
    "Graphviz",
    "Groff",
    "Groovy",
    "Haskell",
    "HTML",
    "HTTP",
    "INI",
    "Java",
    "Java Properties",
    "JavaScript",
    "Jinja2",
    "JQ",
    "JSON",
    "jsonnet",
    "JSX",
    "Julia",
    "Kotlin",
    "LaTeX",
    "Lean",
    "Less",
    "Lisp",
    "Literate Haskell",
    "LiveScript",
    "LLVM",
    "log",
    "Lua",
    "Makefile",
    "Manpage",
    "Markdown",
    "MATLAB",
    "MediaWiki",
    "nginx",
    "Nim",
    "Ninja",
    "Nix",
    "NSIS",
    "Objective-C",
    "Objective-C++",
    "OCaml",
    "OCamllex",
    "orgmode",
    "Pascal",
    "Perl",
    "PHP",
    "PowerShell",
    "Protobuf",
    "Puppet",
    "PureScript",
    "Python",
    "QML",
    "R",
    "Racket",
    "Rd",
    "Rego",
    "reStructuredText",
    "Robot Framework",
    "Ruby",
    "Rust",
    "Salt State",
    "Sass",
    "Scala",
    "SCSS",
    "SML",
    "Solidity",
    "SQL",
    "SSH Config",
    "Strace",
    "Svelte",
    "Swift",
    "SystemVerilog",
    "Tcl",
    "Terraform",
    "Textile",
    "Todo.txt",
    "TOML",
    "TypeScript",
    "TypeScriptReact",
    "varlink",
    "Verilog",
    "VimHelp",
    "VimL",
    "Vue",
    "Vyper",
    "WGSL",
    "XML",
    "YAML",
    "Zig",
    "Zsh",
};

void mat_hl_list_languages(void)
{
    for (size_t i = 0; i < sizeof all_langs / sizeof all_langs[0]; i++)
        printf("%s\n", all_langs[i]);
}

int mat_hl_language_count(void)
{
    return (int)(sizeof all_langs / sizeof all_langs[0]);
}

int mat_hl_line(struct mat_hl *h, const unsigned char *d, size_t len,
                struct mat_span *out, int cap)
{
    if (cap < 1)
        return 0;
    if (len >= HL_LONG_LINE) { /* long-line guard: one unstyled span */
        out[0].start = 0;
        out[0].len = (unsigned)len;
        out[0].tok = MT_TEXT;
        return 1;
    }
    return h->lex(h, d, len, out, cap);
}
