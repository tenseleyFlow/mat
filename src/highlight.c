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
    int state; /* 0=normal, 1=block-comment, 2=multi-line-string */
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

static const char *const theme_catppuccin[MT_NTOKENS] = {
    [MT_TEXT] = "",
    [MT_KEYWORD] = "\x1b[38;5;183m",
    [MT_TYPE] = "\x1b[38;5;223m",
    [MT_STRING] = "\x1b[38;5;151m",
    [MT_NUMBER] = "\x1b[38;5;223m",
    [MT_COMMENT] = "\x1b[38;5;247m",
    [MT_FUNCTION] = "\x1b[38;5;153m",
    [MT_OPERATOR] = "\x1b[38;5;153m",
    [MT_PUNCT] = "",
    [MT_PREPROC] = "\x1b[38;5;225m",
    [MT_CONSTANT] = "\x1b[38;5;223m",
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

static const char *const *active_theme = theme_dark;

int mat_theme_set(const char *name)
{
    if (name == NULL || strcmp(name, "dark") == 0)
        active_theme = theme_dark;
    else if (strcmp(name, "light") == 0)
        active_theme = theme_light;
    else if (strcmp(name, "monokai") == 0)
        active_theme = theme_monokai;
    else if (strcmp(name, "dracula") == 0)
        active_theme = theme_dracula;
    else if (strcmp(name, "solarized-dark") == 0)
        active_theme = theme_solarized_dark;
    else if (strcmp(name, "solarized-light") == 0)
        active_theme = theme_solarized_light;
    else if (strcmp(name, "nord") == 0)
        active_theme = theme_nord;
    else if (strcmp(name, "gruvbox") == 0)
        active_theme = theme_gruvbox;
    else if (strcmp(name, "onedark") == 0)
        active_theme = theme_onedark;
    else if (strcmp(name, "catppuccin") == 0)
        active_theme = theme_catppuccin;
    else if (strcmp(name, "github-dark") == 0)
        active_theme = theme_github_dark;
    else if (strcmp(name, "github-light") == 0)
        active_theme = theme_github_light;
    else if (strcmp(name, "tokyonight") == 0)
        active_theme = theme_tokyonight;
    else if (strcmp(name, "zenburn") == 0)
        active_theme = theme_zenburn;
    else if (strcmp(name, "tomorrow-night") == 0)
        active_theme = theme_tomorrow_night;
    else if (strcmp(name, "tomorrow") == 0)
        active_theme = theme_tomorrow;
    else if (strcmp(name, "material") == 0)
        active_theme = theme_material;
    else if (strcmp(name, "palenight") == 0)
        active_theme = theme_palenight;
    else if (strcmp(name, "synthwave") == 0)
        active_theme = theme_synthwave;
    else if (strcmp(name, "kanagawa") == 0)
        active_theme = theme_kanagawa;
    else if (strcmp(name, "rosepine") == 0)
        active_theme = theme_rosepine;
    else if (strcmp(name, "rosepine-moon") == 0)
        active_theme = theme_rosepine_moon;
    else if (strcmp(name, "rosepine-dawn") == 0)
        active_theme = theme_rosepine_dawn;
    else if (strcmp(name, "everforest-dark") == 0)
        active_theme = theme_everforest_dark;
    else if (strcmp(name, "everforest-light") == 0)
        active_theme = theme_everforest_light;
    else if (strcmp(name, "ayu-dark") == 0)
        active_theme = theme_ayu_dark;
    else if (strcmp(name, "ayu-mirage") == 0)
        active_theme = theme_ayu_mirage;
    else if (strcmp(name, "ayu-light") == 0)
        active_theme = theme_ayu_light;
    else
        return -1;
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
    printf("ayu-dark\nayu-light\nayu-mirage\ncatppuccin\ndark\n"
           "dracula\neverforest-dark\neverforest-light\ngithub-dark\n"
           "github-light\ngruvbox\nkanagawa\nlight\nmaterial\n"
           "monokai\nnord\nonedark\npalenight\nrosepine\n"
           "rosepine-dawn\nrosepine-moon\nsolarized-dark\n"
           "solarized-light\nsynthwave\ntokyonight\ntomorrow\n"
           "tomorrow-night\nzenburn\n");
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

static int ws_has(const struct wordset *ws, const char *w, size_t wl)
{
    for (int i = 0; i < ws->n; i++)
        if (strlen(ws->words[i]) == wl && memcmp(ws->words[i], w, wl) == 0)
            return 1;
    return 0;
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
        size_t s = 0;
        while (i + 2 < len) {
            if (d[i] == '"' && d[i + 1] == '"' && d[i + 2] == '"') {
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
    for (int i = 0; i < ws->n; i++)
        if (strlen(ws->words[i]) == wl && ci_match(ws->words[i], w, wl))
            return 1;
    return 0;
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
    else if (len >= 4 &&
             (memcmp(d, "diff", 4) == 0 || memcmp(d, "index", 5) == 0))
        t = MT_FUNCTION;
    else
        t = MT_TEXT;
    return emit(out, cap, 0, 0, len, t);
}

/* ---- keyword tables ---- */

static const char *const c_kw[] = {
    "auto",     "break",     "case",           "continue", "default",
    "do",       "else",      "enum",           "extern",   "for",
    "goto",     "if",        "inline",         "register", "restrict",
    "return",   "sizeof",    "static",         "struct",   "switch",
    "typedef",  "union",     "volatile",       "while",    "_Alignas",
    "_Alignof", "_Noreturn", "_Static_assert",
};
static const char *const c_ty[] = {
    "void",     "char",     "short",    "int",     "long",    "float",
    "double",   "signed",   "unsigned", "bool",    "size_t",  "ssize_t",
    "int8_t",   "int16_t",  "int32_t",  "int64_t", "uint8_t", "uint16_t",
    "uint32_t", "uint64_t", "FILE",     "NULL",
};
static const char *const py_kw[] = {
    "and",      "as",       "assert", "async", "await",  "break",  "class",
    "continue", "def",      "del",    "elif",  "else",   "except", "finally",
    "for",      "from",     "global", "if",    "import", "in",     "is",
    "lambda",   "nonlocal", "not",    "or",    "pass",   "raise",  "return",
    "try",      "while",    "with",   "yield",
};
static const char *const py_ty[] = {
    "True", "False", "None",  "int",  "float", "str",  "list",
    "dict", "set",   "tuple", "bool", "bytes", "type", "self",
};
static const char *const sh_kw[] = {
    "if",       "then",   "else",     "elif",    "fi",    "for",
    "while",    "do",     "done",     "case",    "esac",  "in",
    "function", "select", "until",    "return",  "break", "continue",
    "local",    "export", "readonly", "declare",
};
static const char *const js_kw[] = {
    "break",    "case",       "catch",  "class",    "const", "continue",
    "debugger", "default",    "delete", "do",       "else",  "export",
    "extends",  "finally",    "for",    "function", "if",    "import",
    "in",       "instanceof", "let",    "new",      "of",    "return",
    "super",    "switch",     "this",   "throw",    "try",   "typeof",
    "var",      "void",       "while",  "with",     "yield", "async",
    "await",
};
static const char *const js_ty[] = {
    "true",   "false",   "null",   "undefined", "NaN", "Infinity", "Number",
    "String", "Boolean", "Object", "Array",     "Map", "Set",
};
static const char *const go_kw[] = {
    "break",  "case",        "chan", "const",   "continue", "default", "defer",
    "else",   "fallthrough", "for",  "func",    "go",       "goto",    "if",
    "import", "interface",   "map",  "package", "range",    "return",  "select",
    "struct", "switch",      "type", "var",
};
static const char *const go_ty[] = {
    "bool",    "byte",    "complex64", "complex128", "error",  "float32",
    "float64", "int",     "int8",      "int16",      "int32",  "int64",
    "rune",    "string",  "uint",      "uint8",      "uint16", "uint32",
    "uint64",  "uintptr", "true",      "false",      "nil",    "iota",
};
static const char *const rs_kw[] = {
    "as",     "async", "await", "break",  "const",  "continue", "crate",
    "dyn",    "else",  "enum",  "extern", "fn",     "for",      "if",
    "impl",   "in",    "let",   "loop",   "match",  "mod",      "move",
    "mut",    "pub",   "ref",   "return", "self",   "Self",     "static",
    "struct", "super", "trait", "type",   "unsafe", "use",      "where",
    "while",  "yield",
};
static const char *const rs_ty[] = {
    "bool",   "char", "f32",   "f64",    "i8",  "i16",  "i32",
    "i64",    "i128", "isize", "str",    "u8",  "u16",  "u32",
    "u64",    "u128", "usize", "String", "Vec", "Box",  "Option",
    "Result", "Some", "None",  "Ok",     "Err", "true", "false",
};

static const char *const fortran_kw[] = {
    "program",   "end",      "subroutine", "function",   "module",
    "use",       "implicit", "none",       "call",       "return",
    "if",        "then",     "else",       "elseif",     "endif",
    "do",        "while",    "enddo",      "select",     "case",
    "where",     "forall",   "continue",   "stop",       "exit",
    "cycle",     "goto",     "allocate",   "deallocate", "contains",
    "interface", "type",     "class",      "associate",  "block",
    "data",      "save",     "common",     "intent",     "in",
    "out",       "inout",    "optional",   "recursive",  "pure",
    "elemental", "abstract",
};
static const char *const fortran_ty[] = {
    "integer",   "real",    "double",    "precision", "complex",
    "character", "logical", "dimension", "parameter", "allocatable",
    "pointer",   "target",  "kind",
};
static const char *const sql_kw[] = {
    "select",  "from",    "where",      "and",        "or",       "not",
    "insert",  "into",    "values",     "update",     "set",      "delete",
    "create",  "drop",    "alter",      "table",      "index",    "view",
    "join",    "inner",   "outer",      "left",       "right",    "on",
    "group",   "by",      "order",      "having",     "limit",    "offset",
    "union",   "all",     "distinct",   "as",         "exists",   "in",
    "between", "like",    "is",         "case",       "when",     "then",
    "else",    "end",     "begin",      "commit",     "rollback", "primary",
    "key",     "foreign", "references", "constraint", "default",  "with",
};
static const char *const sql_ty[] = {
    "int",     "integer", "bigint", "smallint",  "varchar", "char",
    "text",    "boolean", "date",   "timestamp", "float",   "double",
    "decimal", "numeric", "blob",   "serial",    "uuid",
};
static const char *const ruby_kw[] = {
    "def",     "end",    "class",  "module", "if",     "unless", "elsif",
    "else",    "case",   "when",   "while",  "until",  "for",    "do",
    "begin",   "rescue", "ensure", "raise",  "return", "yield",  "require",
    "include", "extend", "puts",   "print",  "lambda", "proc",
};
static const char *const ruby_ty[] = {
    "true",  "false", "nil",  "self",   "String", "Integer",
    "Float", "Array", "Hash", "Symbol", "Proc",   "Class",
};
static const char *const lua_kw[] = {
    "and",      "break",  "do",   "else",  "elseif", "end", "for",
    "function", "goto",   "if",   "in",    "local",  "not", "or",
    "repeat",   "return", "then", "until", "while",
};
static const char *const lua_ty[] = {
    "true",
    "false",
    "nil",
};
static const char *const cs_kw[] = {
    "abstract",  "as",        "base",     "bool",     "break",    "case",
    "catch",     "class",     "const",    "continue", "default",  "delegate",
    "do",        "else",      "enum",     "event",    "explicit", "extern",
    "finally",   "for",       "foreach",  "goto",     "if",       "implicit",
    "in",        "interface", "internal", "is",       "lock",     "namespace",
    "new",       "operator",  "out",      "override", "params",   "private",
    "protected", "public",    "readonly", "ref",      "return",   "sealed",
    "static",    "struct",    "switch",   "this",     "throw",    "try",
    "typeof",    "using",     "var",      "virtual",  "void",     "volatile",
    "while",     "yield",     "async",    "await",
};
static const char *const cs_ty[] = {
    "int",   "long",   "short",  "byte",  "float",  "double", "decimal",
    "char",  "string", "object", "bool",  "uint",   "ulong",  "ushort",
    "sbyte", "null",   "true",   "false", "String", "Int32",
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
    "Any",    "Bool",   "Character", "Double", "Float",  "Int",
    "Int8",   "Int16",  "Int32",     "Int64",  "Never",  "Optional",
    "Self",   "String", "UInt",      "UInt8",  "UInt16", "UInt32",
    "UInt64", "Void",   "true",      "false",  "nil",
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
    "bool",   "double", "dynamic", "int",   "num",  "Object",
    "String", "void",   "List",    "Map",   "Set",  "Future",
    "Stream", "null",   "true",    "false", "Null",
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
    "array", "bool",  "callable", "float", "int",    "iterable",
    "mixed", "null",  "object",   "self",  "string", "void",
    "true",  "false", "NULL",     "TRUE",  "FALSE",
};
static const char *const haskell_kw[] = {
    "case",    "class",  "data",      "default",  "deriving", "do",
    "else",    "forall", "foreign",   "if",       "import",   "in",
    "infix",   "infixl", "infixr",    "instance", "let",      "module",
    "newtype", "of",     "qualified", "then",     "type",     "where",
};
static const char *const haskell_ty[] = {
    "Bool", "Char",    "Double", "Either", "Float", "IO",
    "Int",  "Integer", "Maybe",  "String", "True",  "False",
    "Just", "Nothing", "Left",   "Right",
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
    "true",
    "false",
    "nil",
    "self",
};
static const char *const erlang_kw[] = {
    "after", "and",    "andalso", "begin", "case", "catch",
    "end",   "fun",    "if",      "let",   "not",  "of",
    "or",    "orelse", "receive", "try",   "when",
};
static const char *const erlang_ty[] = {
    "true",
    "false",
    "undefined",
};
static const char *const r_kw[] = {
    "break",  "else",   "for",   "function", "if",      "in",     "next",
    "repeat", "return", "while", "library",  "require", "source",
};
static const char *const r_ty[] = {
    "TRUE", "FALSE",       "NULL",     "NA",          "Inf",
    "NaN",  "NA_integer_", "NA_real_", "NA_complex_", "NA_character_",
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
    "bool", "f16",   "f32",  "f64",       "f80",  "f128", "i8",    "i16",
    "i32",  "i64",   "i128", "isize",     "u8",   "u16",  "u32",   "u64",
    "u128", "usize", "void", "anyopaque", "null", "true", "false", "undefined",
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
    "int",   "float",  "bool", "char", "string", "unit", "list",
    "array", "option", "ref",  "true", "false",  "None", "Some",
};
static const char *const clojure_kw[] = {
    "def",     "defn", "defmacro", "defonce", "fn",    "if",    "do",
    "let",     "loop", "recur",    "throw",   "try",   "catch", "finally",
    "cond",    "case", "when",     "and",     "or",    "not",   "ns",
    "require", "use",  "import",   "in-ns",   "refer",
};
static const char *const clojure_ty[] = {
    "nil",
    "true",
    "false",
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
    "Any",     "Bool",    "Char",  "Float16", "Float32", "Float64",
    "Int",     "Int8",    "Int16", "Int32",   "Int64",   "Int128",
    "Nothing", "String",  "UInt",  "UInt8",   "UInt16",  "UInt32",
    "UInt64",  "UInt128", "true",  "false",   "nothing",
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
    "int",     "int8",   "int16",  "int32",  "int64", "uint",
    "uint8",   "uint16", "uint32", "uint64", "float", "float32",
    "float64", "bool",   "char",   "string", "seq",   "array",
    "void",    "true",   "false",  "nil",
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
    "boolean", "byte",   "char", "double", "float", "int",
    "long",    "short",  "void", "def",    "null",  "true",
    "false",   "String", "List", "Map",    "Set",
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
    "bool",   "byte",   "cdouble", "cfloat", "char",  "creal",  "dchar",
    "double", "float",  "idouble", "ifloat", "int",   "ireal",  "long",
    "real",   "short",  "ubyte",   "uint",   "ulong", "ushort", "wchar",
    "string", "size_t", "null",    "true",   "false",
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
    "bool",    "byte",   "char",   "decimal", "double", "float",
    "float32", "int",    "int16",  "int32",   "int64",  "nativeint",
    "sbyte",   "single", "string", "uint16",  "uint32", "uint64",
    "unit",    "true",   "false",  "None",    "Some",
};
static const char *const glsl_kw[] = {
    "attribute",  "break",  "case",      "const",     "continue", "default",
    "discard",    "do",     "else",      "flat",      "for",      "highp",
    "if",         "in",     "inout",     "invariant", "layout",   "lowp",
    "mediump",    "out",    "precision", "return",    "smooth",   "struct",
    "subroutine", "switch", "uniform",   "varying",   "while",
};
static const char *const glsl_ty[] = {
    "bool",        "bvec2", "bvec3", "bvec4", "double",    "dvec2",
    "dvec3",       "dvec4", "float", "int",   "ivec2",     "ivec3",
    "ivec4",       "mat2",  "mat3",  "mat4",  "sampler2D", "sampler3D",
    "samplerCube", "uint",  "uvec2", "uvec3", "uvec4",     "vec2",
    "vec3",        "vec4",  "void",  "true",  "false",
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
    "true", "false", "null", "undefined", "NaN", "Infinity",
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
    "Bool",   "Char", "Float32", "Float64", "Int8",  "Int16",  "Int32",
    "Int64",  "Nil",  "String",  "Symbol",  "UInt8", "UInt16", "UInt32",
    "UInt64", "Void", "true",    "false",   "nil",   "self",
};
static const char *const elm_kw[] = {
    "alias", "as",     "case", "else", "exposing", "if",   "import", "in",
    "let",   "module", "of",   "port", "then",     "type", "where",
};
static const char *const elm_ty[] = {
    "Bool",  "Char",   "Float", "Int",   "List", "Maybe",
    "Never", "String", "True",  "False", "Just", "Nothing",
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
    "address", "bool",   "bytes",   "int",     "int8", "int16", "int32",
    "int64",   "int128", "int256",  "string",  "uint", "uint8", "uint16",
    "uint32",  "uint64", "uint128", "uint256", "true", "false",
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
    "Boolean", "Character", "Duration", "Float", "Integer",
    "Natural", "Positive",  "String",   "True",  "False",
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
    "boolean", "byte",     "cardinal", "char",    "comp",     "currency",
    "double",  "extended", "int64",    "integer", "longint",  "longword",
    "pointer", "real",     "shortint", "single",  "smallint", "string",
    "word",    "true",     "false",    "nil",
};
static const char *const matlab_kw[] = {
    "break",  "case",   "catch",     "classdef", "continue",
    "else",   "elseif", "end",       "for",      "function",
    "global", "if",     "otherwise", "parfor",   "persistent",
    "return", "spmd",   "switch",    "try",      "while",
};
static const char *const matlab_ty[] = {
    "true", "false", "inf", "Inf", "nan", "NaN", "pi",
};
static const char *const protobuf_kw[] = {
    "enum",     "extend",   "extensions", "import",  "message",  "oneof",
    "option",   "optional", "package",    "public",  "repeated", "required",
    "reserved", "returns",  "rpc",        "service", "stream",   "syntax",
    "to",       "weak",     "map",
};
static const char *const protobuf_ty[] = {
    "bool",   "bytes",  "double",   "fixed32",  "fixed64", "float",
    "int32",  "int64",  "sfixed32", "sfixed64", "sint32",  "sint64",
    "string", "uint32", "uint64",   "true",     "false",
};
static const char *const terraform_kw[] = {
    "data",      "dynamic",  "for_each", "lifecycle",   "locals",
    "module",    "output",   "provider", "provisioner", "resource",
    "terraform", "variable", "for",      "if",          "in",
    "each",      "self",     "count",    "depends_on",
};
static const char *const terraform_ty[] = {
    "bool",   "list",  "map", "number", "object", "set",
    "string", "tuple", "any", "true",   "false",  "null",
};
static const char *const nix_kw[] = {
    "assert", "else", "if",   "in",     "inherit",  "let",
    "rec",    "then", "with", "import", "builtins",
};
static const char *const nix_ty[] = {
    "true",
    "false",
    "null",
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
    "and",          "begin",   "case", "cond",       "define", "defmacro",
    "defun",        "do",      "else", "if",         "lambda", "let",
    "let*",         "letrec",  "or",   "quasiquote", "quote",  "set!",
    "syntax-rules", "unquote", "when", "unless",
};
static const char *const lisp_ty[] = {
    "nil",
    "t",
    "#t",
    "#f",
};
static const char *const batch_kw[] = {
    "call",  "cls",      "cmd",      "color",      "copy",  "del",  "dir",
    "echo",  "else",     "endlocal", "errorlevel", "exist", "exit", "for",
    "goto",  "if",       "md",       "mkdir",      "move",  "not",  "path",
    "pause", "popd",     "pushd",    "rd",         "rem",   "ren",  "rmdir",
    "set",   "setlocal", "shift",    "start",      "title", "type",
};
static const char *const graphql_kw[] = {
    "directive", "enum",      "extend",       "fragment", "implements",
    "input",     "interface", "mutation",     "on",       "query",
    "scalar",    "schema",    "subscription", "type",     "union",
};
static const char *const graphql_ty[] = {
    "Boolean", "Float", "ID", "Int", "String", "true", "false", "null",
};
static const char *const cmake_kw[] = {
    "add_executable",
    "add_library",
    "add_subdirectory",
    "cmake_minimum_required",
    "else",
    "elseif",
    "enable_testing",
    "endif",
    "endforeach",
    "endfunction",
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
    "server",
    "location",
    "listen",
    "root",
    "index",
    "proxy_pass",
    "upstream",
    "include",
    "return",
    "rewrite",
    "if",
    "set",
    "error_page",
    "access_log",
    "error_log",
    "worker_processes",
    "events",
    "http",
    "server_name",
    "ssl_certificate",
    "ssl_certificate_key",
};
static const char *const viml_kw[] = {
    "augroup",  "autocmd", "call",     "command",  "echo",        "echom",
    "else",     "elseif",  "endif",    "endfor",   "endfunction", "endwhile",
    "execute",  "finish",  "for",      "function", "if",          "let",
    "map",      "nmap",    "nnoremap", "noremap",  "return",      "set",
    "setlocal", "silent",  "source",   "syntax",   "while",
};
static const char *const qml_kw[] = {
    "as",       "break",  "case",   "catch",      "continue", "default",
    "delete",   "do",     "else",   "finally",    "for",      "function",
    "if",       "import", "in",     "instanceof", "new",      "property",
    "readonly", "return", "signal", "switch",     "this",     "throw",
    "try",      "typeof", "var",    "void",       "while",    "with",
};
static const char *const qml_ty[] = {
    "alias", "bool",  "color",  "date",      "double", "int",
    "list",  "real",  "string", "url",       "var",    "variant",
    "true",  "false", "null",   "undefined",
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
    "Array",  "Boolean", "Class",     "Date", "Function", "int",
    "Number", "Object",  "String",    "uint", "XML",      "null",
    "true",   "false",   "undefined", "NaN",  "Infinity",
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
    "array",  "atomic", "bool",   "f16",  "f32",     "i32",
    "mat2x2", "mat3x3", "mat4x4", "ptr",  "sampler", "texture_2d",
    "u32",    "vec2",   "vec3",   "vec4", "true",    "false",
};
static const char *const lean_kw[] = {
    "abbrev",    "axiom",     "by",       "calc",       "class",
    "constant",  "def",       "deriving", "do",         "else",
    "end",       "example",   "extends",  "fun",        "have",
    "if",        "import",    "in",       "inductive",  "instance",
    "let",       "match",     "mutual",   "namespace",  "noncomputable",
    "notation",  "open",      "opaque",   "partial",    "private",
    "protected", "return",    "section",  "set_option", "show",
    "sorry",     "structure", "suffices", "tactic",     "then",
    "theorem",   "universe",  "variable", "where",      "with",
};
static const char *const lean_ty[] = {
    "Bool", "Char",   "Float", "Int",  "IO",   "List",  "Nat",  "Option",
    "Prop", "String", "Type",  "Unit", "True", "False", "true", "false",
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
            if (ci_match("FROM", (const char *)d + s, wl) ||
                ci_match("RUN", (const char *)d + s, wl) ||
                ci_match("CMD", (const char *)d + s, wl) ||
                ci_match("COPY", (const char *)d + s, wl) ||
                ci_match("ADD", (const char *)d + s, wl) ||
                ci_match("ENV", (const char *)d + s, wl) ||
                ci_match("EXPOSE", (const char *)d + s, wl) ||
                ci_match("ENTRYPOINT", (const char *)d + s, wl) ||
                ci_match("WORKDIR", (const char *)d + s, wl) ||
                ci_match("ARG", (const char *)d + s, wl) ||
                ci_match("LABEL", (const char *)d + s, wl) ||
                ci_match("VOLUME", (const char *)d + s, wl) ||
                ci_match("USER", (const char *)d + s, wl) ||
                ci_match("HEALTHCHECK", (const char *)d + s, wl) ||
                ci_match("SHELL", (const char *)d + s, wl) ||
                ci_match("STOPSIGNAL", (const char *)d + s, wl) ||
                ci_match("ONBUILD", (const char *)d + s, wl) ||
                ci_match("MAINTAINER", (const char *)d + s, wl) ||
                ci_match("AS", (const char *)d + s, wl))
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
    "pick",  "reword", "edit",  "squash", "fixup", "exec",
    "break", "drop",   "label", "reset",  "merge",
};

static int lex_gitrebase(struct mat_hl *h, const unsigned char *d, size_t len,
                         struct mat_span *out, int cap)
{
    (void)h;
    if (len > 0 && d[0] == '#')
        return emit(out, cap, 0, 0, len, MT_COMMENT);
    int n = 0;
    size_t i = 0;
    if (is_word(d[0])) {
        while (i < len && is_word(d[i]))
            i++;
        size_t wl = i;
        enum mat_tok t = MT_TEXT;
        for (size_t k = 0; k < sizeof git_rebase_kw / sizeof git_rebase_kw[0];
             k++)
            if (strlen(git_rebase_kw[k]) == wl &&
                memcmp(d, git_rebase_kw[k], wl) == 0)
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
    "integer", "real",  "realtime", "time", "supply0", "supply1", "tri",
    "triand",  "trior", "tri0",     "tri1", "wand",    "wor",
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
    "localparam",  "module",      "new",          "output",       "package",
    "parameter",   "posedge",     "negedge",      "priority",     "program",
    "property",    "protected",   "pure",         "rand",         "ref",
    "repeat",      "return",      "sequence",     "static",       "struct",
    "super",       "task",        "this",         "typedef",      "union",
    "unique",      "var",         "virtual",      "void",         "while",
    "wire",        "with",
};
static const char *const sv_ty[] = {
    "bit",      "byte", "int",      "integer",   "logic",  "longint", "real",
    "realtime", "reg",  "shortint", "shortreal", "string", "time",    "chandle",
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
    "Section",
    "SectionEnd",
    "Function",
    "FunctionEnd",
    "Goto",
    "Call",
    "Quit",
    "Return",
    "MessageBox",
    "DetailPrint",
    "SetOutPath",
    "File",
    "CreateDirectory",
    "Delete",
    "RMDir",
    "ExecWait",
    "Exec",
    "StrCpy",
    "StrCmp",
    "IntCmp",
    "IfErrors",
    "ClearErrors",
    "Var",
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

struct mat_hl *mat_hl_open(const char *syntax)
{
    if (syntax == NULL)
        return NULL;
    lex_fn lex = NULL;
    struct wordset kw = {NULL, 0}, ty = {NULL, 0};
    if (strcmp(syntax, "JSON") == 0) {
        lex = lex_json;
    } else if (strcmp(syntax, "C") == 0) {
        lex = lex_cfamily;
        kw = WS(c_kw);
        ty = WS(c_ty);
    } else if (strcmp(syntax, "C++") == 0) {
        lex = lex_cfamily;
        kw = WS(c_kw);
        ty = WS(c_ty);
    } else if (strcmp(syntax, "Java") == 0) {
        lex = lex_cfamily;
        kw = WS(c_kw);
        ty = WS(c_ty);
    } else if (strcmp(syntax, "JavaScript") == 0 ||
               strcmp(syntax, "TypeScript") == 0) {
        lex = lex_cfamily;
        kw = WS(js_kw);
        ty = WS(js_ty);
    } else if (strcmp(syntax, "Go") == 0) {
        lex = lex_cfamily;
        kw = WS(go_kw);
        ty = WS(go_ty);
    } else if (strcmp(syntax, "Rust") == 0) {
        lex = lex_cfamily;
        kw = WS(rs_kw);
        ty = WS(rs_ty);
    } else if (strcmp(syntax, "Python") == 0) {
        lex = lex_python;
        kw = WS(py_kw);
        ty = WS(py_ty);
    } else if (strcmp(syntax, "Bash") == 0 || strcmp(syntax, "Zsh") == 0) {
        lex = lex_shell;
        kw = WS(sh_kw);
    } else if (strcmp(syntax, "Fortran") == 0) {
        lex = lex_fortran;
        kw = WS(fortran_kw);
        ty = WS(fortran_ty);
    } else if (strcmp(syntax, "Markdown") == 0) {
        lex = lex_markdown;
    } else if (strcmp(syntax, "YAML") == 0) {
        lex = lex_yaml;
    } else if (strcmp(syntax, "TOML") == 0) {
        lex = lex_toml;
    } else if (strcmp(syntax, "HTML") == 0) {
        lex = lex_html;
    } else if (strcmp(syntax, "CSS") == 0 || strcmp(syntax, "SCSS") == 0) {
        lex = lex_css;
    } else if (strcmp(syntax, "SQL") == 0) {
        lex = lex_sql;
        kw = WS(sql_kw);
        ty = WS(sql_ty);
    } else if (strcmp(syntax, "Ruby") == 0) {
        lex = lex_ruby;
        kw = WS(ruby_kw);
        ty = WS(ruby_ty);
    } else if (strcmp(syntax, "Lua") == 0) {
        lex = lex_lua;
        kw = WS(lua_kw);
        ty = WS(lua_ty);
    } else if (strcmp(syntax, "Makefile") == 0) {
        lex = lex_makefile;
    } else if (strcmp(syntax, "Diff") == 0) {
        lex = lex_diff;
    } else if (strcmp(syntax, "C#") == 0) {
        lex = lex_cfamily;
        kw = WS(cs_kw);
        ty = WS(cs_ty);
    } else if (strcmp(syntax, "Kotlin") == 0) {
        lex = lex_cfamily;
        kw = WS(kt_kw);
        ty = WS(kt_ty);
    } else if (strcmp(syntax, "Scala") == 0) {
        lex = lex_cfamily;
        kw = WS(scala_kw);
        ty = WS(scala_ty);
    } else if (strcmp(syntax, "Swift") == 0) {
        lex = lex_cfamily;
        kw = WS(swift_kw);
        ty = WS(swift_ty);
    } else if (strcmp(syntax, "Dart") == 0) {
        lex = lex_cfamily;
        kw = WS(dart_kw);
        ty = WS(dart_ty);
    } else if (strcmp(syntax, "Zig") == 0) {
        lex = lex_cfamily;
        kw = WS(zig_kw);
        ty = WS(zig_ty);
    } else if (strcmp(syntax, "Nim") == 0) {
        lex = lex_cfamily;
        kw = WS(nim_kw);
        ty = WS(nim_ty);
    } else if (strcmp(syntax, "Groovy") == 0) {
        lex = lex_cfamily;
        kw = WS(groovy_kw);
        ty = WS(groovy_ty);
    } else if (strcmp(syntax, "Perl") == 0) {
        lex = lex_perish;
        kw = WS(perl_kw);
    } else if (strcmp(syntax, "PHP") == 0) {
        lex = lex_perish;
        kw = WS(php_kw);
        ty = WS(php_ty);
    } else if (strcmp(syntax, "Haskell") == 0) {
        lex = lex_haskell;
        kw = WS(haskell_kw);
        ty = WS(haskell_ty);
    } else if (strcmp(syntax, "OCaml") == 0) {
        lex = lex_haskell;
        kw = WS(ocaml_kw);
        ty = WS(ocaml_ty);
    } else if (strcmp(syntax, "Elixir") == 0) {
        lex = lex_python;
        kw = WS(elixir_kw);
        ty = WS(elixir_ty);
    } else if (strcmp(syntax, "Erlang") == 0) {
        lex = lex_python;
        kw = WS(erlang_kw);
        ty = WS(erlang_ty);
    } else if (strcmp(syntax, "R") == 0) {
        lex = lex_r;
        kw = WS(r_kw);
        ty = WS(r_ty);
    } else if (strcmp(syntax, "Clojure") == 0) {
        lex = lex_clojure;
        kw = WS(clojure_kw);
        ty = WS(clojure_ty);
    } else if (strcmp(syntax, "Julia") == 0) {
        lex = lex_python;
        kw = WS(julia_kw);
        ty = WS(julia_ty);
    } else if (strcmp(syntax, "Dockerfile") == 0) {
        lex = lex_dockerfile;
    } else if (strcmp(syntax, "INI") == 0) {
        lex = lex_ini;
    } else if (strcmp(syntax, "LaTeX") == 0) {
        lex = lex_latex;
    } else if (strcmp(syntax, "PowerShell") == 0) {
        lex = lex_shell;
        kw = WS(powershell_kw);
    } else if (strcmp(syntax, "AWK") == 0) {
        lex = lex_shell;
        kw = WS(awk_kw);
    } else if (strcmp(syntax, "Fish") == 0) {
        lex = lex_shell;
        kw = WS(fish_kw);
    } else if (strcmp(syntax, "Objective-C") == 0 ||
               strcmp(syntax, "Objective-C++") == 0) {
        lex = lex_cfamily;
        kw = WS(c_kw);
        ty = WS(c_ty);
    } else if (strcmp(syntax, "D") == 0) {
        lex = lex_cfamily;
        kw = WS(d_kw);
        ty = WS(d_ty);
    } else if (strcmp(syntax, "F#") == 0) {
        lex = lex_haskell;
        kw = WS(fsharp_kw);
        ty = WS(fsharp_ty);
    } else if (strcmp(syntax, "GLSL") == 0) {
        lex = lex_cfamily;
        kw = WS(glsl_kw);
        ty = WS(glsl_ty);
    } else if (strcmp(syntax, "CoffeeScript") == 0) {
        lex = lex_python;
        kw = WS(coffee_kw);
        ty = WS(coffee_ty);
    } else if (strcmp(syntax, "Crystal") == 0) {
        lex = lex_ruby;
        kw = WS(crystal_kw);
        ty = WS(crystal_ty);
    } else if (strcmp(syntax, "Elm") == 0) {
        lex = lex_haskell;
        kw = WS(elm_kw);
        ty = WS(elm_ty);
    } else if (strcmp(syntax, "Solidity") == 0) {
        lex = lex_cfamily;
        kw = WS(solidity_kw);
        ty = WS(solidity_ty);
    } else if (strcmp(syntax, "Ada") == 0) {
        lex = lex_haskell;
        kw = WS(ada_kw);
        ty = WS(ada_ty);
    } else if (strcmp(syntax, "Pascal") == 0) {
        lex = lex_pascal;
        kw = WS(pascal_kw);
        ty = WS(pascal_ty);
    } else if (strcmp(syntax, "MATLAB") == 0) {
        lex = lex_matlab;
        kw = WS(matlab_kw);
        ty = WS(matlab_ty);
    } else if (strcmp(syntax, "Protobuf") == 0) {
        lex = lex_cfamily;
        kw = WS(protobuf_kw);
        ty = WS(protobuf_ty);
    } else if (strcmp(syntax, "Terraform") == 0) {
        lex = lex_cfamily;
        kw = WS(terraform_kw);
        ty = WS(terraform_ty);
    } else if (strcmp(syntax, "Nix") == 0) {
        lex = lex_python;
        kw = WS(nix_kw);
        ty = WS(nix_ty);
    } else if (strcmp(syntax, "Tcl") == 0) {
        lex = lex_shell;
        kw = WS(tcl_kw);
    } else if (strcmp(syntax, "Lisp") == 0 || strcmp(syntax, "Racket") == 0) {
        lex = lex_clojure;
        kw = WS(lisp_kw);
        ty = WS(lisp_ty);
    } else if (strcmp(syntax, "Assembly") == 0) {
        lex = lex_asm;
    } else if (strcmp(syntax, "Batch File") == 0) {
        lex = lex_batch;
        kw = WS(batch_kw);
    } else if (strcmp(syntax, "XML") == 0) {
        lex = lex_html;
    } else if (strcmp(syntax, "Graphviz") == 0) {
        lex = lex_cfamily;
    } else if (strcmp(syntax, "PureScript") == 0) {
        lex = lex_haskell;
        kw = WS(haskell_kw);
        ty = WS(haskell_ty);
    } else if (strcmp(syntax, "SML") == 0) {
        lex = lex_haskell;
        kw = WS(ocaml_kw);
        ty = WS(ocaml_ty);
    } else if (strcmp(syntax, "GraphQL") == 0) {
        lex = lex_python;
        kw = WS(graphql_kw);
        ty = WS(graphql_ty);
    } else if (strcmp(syntax, "CMake") == 0) {
        lex = lex_python;
        kw = WS(cmake_kw);
    } else if (strcmp(syntax, "nginx") == 0 ||
               strcmp(syntax, "Apache Conf") == 0) {
        lex = lex_python;
        kw = WS(nginx_kw);
    } else if (strcmp(syntax, "VimL") == 0) {
        lex = lex_viml;
        kw = WS(viml_kw);
    } else if (strcmp(syntax, "Sass") == 0 || strcmp(syntax, "Less") == 0 ||
               strcmp(syntax, "Stylus") == 0) {
        lex = lex_css;
    } else if (strcmp(syntax, "jsonnet") == 0) {
        lex = lex_cfamily;
        kw = WS(jsonnet_kw);
    } else if (strcmp(syntax, "Puppet") == 0) {
        lex = lex_python;
        kw = WS(puppet_kw);
    } else if (strcmp(syntax, "QML") == 0) {
        lex = lex_cfamily;
        kw = WS(qml_kw);
        ty = WS(qml_ty);
    } else if (strcmp(syntax, "LLVM") == 0) {
        lex = lex_asm;
    } else if (strcmp(syntax, "gnuplot") == 0) {
        lex = lex_python;
    } else if (strcmp(syntax, "ActionScript") == 0) {
        lex = lex_cfamily;
        kw = WS(actionscript_kw);
        ty = WS(actionscript_ty);
    } else if (strcmp(syntax, "AppleScript") == 0) {
        lex = lex_haskell;
        kw = WS(applescript_kw);
    } else if (strcmp(syntax, "WGSL") == 0) {
        lex = lex_cfamily;
        kw = WS(wgsl_kw);
        ty = WS(wgsl_ty);
    } else if (strcmp(syntax, "Rego") == 0) {
        lex = lex_python;
        kw = WS(rego_kw);
    } else if (strcmp(syntax, "Vyper") == 0) {
        lex = lex_python;
        kw = WS(rego_kw);
    } else if (strcmp(syntax, "Java Properties") == 0 ||
               strcmp(syntax, "DotENV") == 0 ||
               strcmp(syntax, "Requirements.txt") == 0) {
        lex = lex_ini;
    } else if (strcmp(syntax, "TypeScriptReact") == 0 ||
               strcmp(syntax, "JSX") == 0) {
        lex = lex_cfamily;
        kw = WS(js_kw);
        ty = WS(js_ty);
    } else if (strcmp(syntax, "Lean") == 0) {
        lex = lex_haskell;
        kw = WS(lean_kw);
        ty = WS(lean_ty);
    } else if (strcmp(syntax, "Groff") == 0 || strcmp(syntax, "Manpage") == 0) {
        lex = lex_groff;
    } else if (strcmp(syntax, "BibTeX") == 0) {
        lex = lex_bibtex;
    } else if (strcmp(syntax, "Svelte") == 0 || strcmp(syntax, "Vue") == 0) {
        lex = lex_html;
    } else if (strcmp(syntax, "Jinja2") == 0) {
        lex = lex_html;
    } else if (strcmp(syntax, "AsciiDoc") == 0 ||
               strcmp(syntax, "reStructuredText") == 0 ||
               strcmp(syntax, "MediaWiki") == 0 ||
               strcmp(syntax, "orgmode") == 0) {
        lex = lex_markdown;
    }
    /* Group 1: aliases of languages we already handle. */
    else if (strcmp(syntax, "Assembly (x86_64)") == 0 ||
             strcmp(syntax, "ARM Assembly") == 0) {
        lex = lex_asm;
    } else if (strcmp(syntax, "Bourne Again Shell (bash)") == 0) {
        lex = lex_shell;
        kw = WS(sh_kw);
    } else if (strcmp(syntax, "AsciiDoc (Asciidoctor)") == 0) {
        lex = lex_markdown;
    } else if (strcmp(syntax, "Graphviz (DOT)") == 0) {
        lex = lex_cfamily;
    } else if (strcmp(syntax, "Groff/troff") == 0) {
        lex = lex_groff;
    } else if (strcmp(syntax, "Vue Component") == 0) {
        lex = lex_html;
    } else if (strcmp(syntax, "Protocol Buffer (TEXT)") == 0) {
        lex = lex_cfamily;
        kw = WS(protobuf_kw);
        ty = WS(protobuf_ty);
    } else if (strcmp(syntax, "JavaScript (Babel)") == 0 ||
               strcmp(syntax, "JavaScript (Rails)") == 0) {
        lex = lex_cfamily;
        kw = WS(js_kw);
        ty = WS(js_ty);
    } else if (strcmp(syntax, "Ruby on Rails") == 0 ||
               strcmp(syntax, "Ruby Haml") == 0 ||
               strcmp(syntax, "Ruby Slim") == 0) {
        lex = lex_ruby;
        kw = WS(ruby_kw);
        ty = WS(ruby_ty);
    } else if (strcmp(syntax, "SQL (Rails)") == 0) {
        lex = lex_sql;
        kw = WS(sql_kw);
        ty = WS(sql_ty);
    } else if (strcmp(syntax, "TeX") == 0) {
        lex = lex_latex;
    }
    /* Group 3: Git-related files. */
    else if (strcmp(syntax, "Git Commit") == 0 ||
             strcmp(syntax, "Git Attributes") == 0 ||
             strcmp(syntax, "Git Ignore") == 0 ||
             strcmp(syntax, "Git Mailmap") == 0 ||
             strcmp(syntax, "Git Link") == 0 ||
             strcmp(syntax, "Git Log") == 0) {
        lex = lex_gitcommit;
    } else if (strcmp(syntax, "Git Config") == 0) {
        lex = lex_ini;
    } else if (strcmp(syntax, "Git Rebase Todo") == 0) {
        lex = lex_gitrebase;
    }
    /* Group 4: System config files. */
    else if (strcmp(syntax, "SSH Config") == 0 ||
             strcmp(syntax, "SSHD Config") == 0 ||
             strcmp(syntax, "Authorized Keys") == 0 ||
             strcmp(syntax, "Known Hosts") == 0 ||
             strcmp(syntax, "hosts") == 0 || strcmp(syntax, "resolv") == 0) {
        lex = lex_sshconfig;
    } else if (strcmp(syntax, "fstab") == 0) {
        lex = lex_sshconfig;
    } else if (strcmp(syntax, "passwd") == 0 || strcmp(syntax, "group") == 0) {
        lex = lex_colonfile;
    } else if (strcmp(syntax, "Crontab") == 0) {
        lex = lex_crontab;
    } else if (strcmp(syntax, "CpuInfo") == 0 ||
               strcmp(syntax, "MemInfo") == 0) {
        lex = lex_sshconfig;
    }
    /* Group 5: Niche languages/formats. */
    else if (strcmp(syntax, "Verilog") == 0) {
        lex = lex_cfamily;
        kw = WS(verilog_kw);
        ty = WS(verilog_ty);
    } else if (strcmp(syntax, "SystemVerilog") == 0) {
        lex = lex_cfamily;
        kw = WS(sv_kw);
        ty = WS(sv_ty);
    } else if (strcmp(syntax, "Strace") == 0) {
        lex = lex_strace;
    } else if (strcmp(syntax, "log") == 0 || strcmp(syntax, "syslog") == 0) {
        lex = lex_log;
    } else if (strcmp(syntax, "Todo.txt") == 0) {
        lex = lex_todotxt;
    } else if (strcmp(syntax, "VimHelp") == 0) {
        lex = lex_vimhelp;
    } else if (strcmp(syntax, "HTTP Request and Response") == 0) {
        lex = lex_http;
    } else if (strcmp(syntax, "Ninja") == 0) {
        lex = lex_python;
        kw = WS(ninja_kw);
    } else if (strcmp(syntax, "NSIS") == 0) {
        lex = lex_perish;
        kw = WS(nsis_kw);
    } else if (strcmp(syntax, "JQ") == 0) {
        lex = lex_jq;
    } else if (strcmp(syntax, "Literate Haskell") == 0) {
        lex = lex_lhaskell;
        kw = WS(haskell_kw);
        ty = WS(haskell_ty);
    } else if (strcmp(syntax, "LiveScript") == 0) {
        lex = lex_python;
        kw = WS(coffee_kw);
        ty = WS(coffee_ty);
    } else if (strcmp(syntax, "Cabal") == 0) {
        lex = lex_sshconfig;
    } else if (strcmp(syntax, "CMakeCache") == 0 ||
               strcmp(syntax, "CMake C Header") == 0 ||
               strcmp(syntax, "CMake C++ Header") == 0) {
        lex = lex_python;
        kw = WS(cmake_kw);
    } else if (strcmp(syntax, "CFML") == 0) {
        lex = lex_html;
    } else if (strcmp(syntax, "OCamllex") == 0 ||
               strcmp(syntax, "OCamlyacc") == 0) {
        lex = lex_haskell;
        kw = WS(ocaml_kw);
        ty = WS(ocaml_ty);
    } else if (strcmp(syntax, "Rd (R Documentation)") == 0) {
        lex = lex_latex;
    } else if (strcmp(syntax, "Robot Framework") == 0) {
        lex = lex_python;
    } else if (strcmp(syntax, "Salt State (SLS)") == 0) {
        lex = lex_yaml;
    } else if (strcmp(syntax, "Textile") == 0) {
        lex = lex_markdown;
    } else if (strcmp(syntax, "Email") == 0) {
        lex = lex_http;
    } else if (strcmp(syntax, "Comma Separated Values") == 0 ||
               strcmp(syntax, "CSV") == 0) {
        lex = lex_colonfile;
    } else if (strcmp(syntax, "varlink") == 0) {
        lex = lex_cfamily;
    } else if (strcmp(syntax, "Regular Expression") == 0) {
        lex = lex_cfamily;
    }
    /* Group 2: HTML template variants. */
    else if (strcmp(syntax, "HTML (ASP)") == 0 ||
             strcmp(syntax, "HTML (EEx)") == 0 ||
             strcmp(syntax, "HTML (Erlang)") == 0 ||
             strcmp(syntax, "HTML (Jinja2)") == 0 ||
             strcmp(syntax, "HTML (Rails)") == 0 ||
             strcmp(syntax, "HTML (Tcl)") == 0 ||
             strcmp(syntax, "HTML (Twig)") == 0 ||
             strcmp(syntax, "Java Server Page (JSP)") == 0 ||
             strcmp(syntax, "ASP") == 0 ||
             strcmp(syntax, "NAnt Build File") == 0) {
        lex = lex_html;
    }
    if (lex == NULL)
        return NULL;
    struct mat_hl *h = calloc(1, sizeof *h);
    if (h != NULL) {
        h->lex = lex;
        h->keywords = kw;
        h->types = ty;
    }
    return h;
}

void mat_hl_close(struct mat_hl *h)
{
    free(h);
}

void mat_hl_list_languages(void)
{
    static const char *const langs[] = {
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
        "Clojure",
        "CMake",
        "CoffeeScript",
        "Crystal",
        "CSS",
        "D",
        "Dart",
        "Diff",
        "Dockerfile",
        "DotENV",
        "Elixir",
        "Elm",
        "Erlang",
        "F#",
        "Fish",
        "Fortran",
        "GLSL",
        "gnuplot",
        "Go",
        "GraphQL",
        "Graphviz",
        "Groff",
        "Groovy",
        "Haskell",
        "HTML",
        "INI",
        "Java",
        "Java Properties",
        "JavaScript",
        "Jinja2",
        "JSON",
        "jsonnet",
        "JSX",
        "Julia",
        "Kotlin",
        "LaTeX",
        "Lean",
        "Less",
        "Lisp",
        "LLVM",
        "Lua",
        "Makefile",
        "Manpage",
        "Markdown",
        "MATLAB",
        "MediaWiki",
        "nginx",
        "Nim",
        "Nix",
        "Objective-C",
        "Objective-C++",
        "OCaml",
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
        "Rego",
        "reStructuredText",
        "Ruby",
        "Rust",
        "Sass",
        "Scala",
        "SCSS",
        "SML",
        "Solidity",
        "SQL",
        "Svelte",
        "Swift",
        "Tcl",
        "Terraform",
        "TOML",
        "TypeScript",
        "TypeScriptReact",
        "VimL",
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
        "Cabal",
        "CFML",
        "CMakeCache",
        "Crontab",
        "CSV",
        "Email",
        "Git Commit",
        "Git Config",
        "Git Rebase Todo",
        "HTTP",
        "JQ",
        "Literate Haskell",
        "LiveScript",
        "log",
        "Ninja",
        "NSIS",
        "OCamllex",
        "Rd",
        "Robot Framework",
        "Salt State",
        "SSH Config",
        "Strace",
        "SystemVerilog",
        "Textile",
        "Todo.txt",
        "varlink",
    };
    for (size_t i = 0; i < sizeof langs / sizeof langs[0]; i++)
        printf("%s\n", langs[i]);
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
