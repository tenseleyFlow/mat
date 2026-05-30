#compdef mat
# zsh completion for mat
_mat() {
    _arguments -s \
        '(-n)-n[number all lines]' \
        '(-b)-b[number nonempty lines]' \
        '(-s)-s[squeeze blank lines]' \
        '(-v)-v[show nonprinting]' \
        '(-e)-e[show ends (-vE)]' \
        '(-t)-t[show tabs (-vT)]' \
        '(-A)-A[show all (-vET)]' \
        '(-E)-E[show line ends]' \
        '(-T)-T[show tabs]' \
        '(-p --pretty)'{-p,--pretty}'[full decoration frame]' \
        '(-S --chop-long-lines)'{-S,--chop-long-lines}'[do not wrap]' \
        '(-P --no-paging)'{-P,--no-paging}'[disable pager]' \
        '(-d --diff)'{-d,--diff}'[show git change markers]' \
        '(-L --list-languages)'{-L,--list-languages}'[print supported languages]' \
        '*-r[line range]:range:' \
        '*-H[highlight line range]:range:' \
        '*-l[force syntax]:language:' \
        '*-m[map glob to syntax]:glob\:syntax:' \
        '--style[decoration components]:components:(plain default full numbers grid header header-filesize rule snip)' \
        '--color[when to colorize]:when:(auto never always)' \
        '--decorations[when to decorate]:when:(auto never always)' \
        '--wrap[wrap mode]:mode:(auto never character word)' \
        '--paging[when to page]:when:(auto never always)' \
        '--tabs[tab width]:n:' \
        '--terminal-width[columns]:n:' \
        '--line-range[print only range]:range:' \
        '--highlight-line[emphasize range]:range:' \
        '--squeeze-limit[max blank lines under -s]:n:' \
        '--binary[binary file handling]:mode:(no-printing as-text)' \
        '--strip-ansi[strip input escapes]:when:(auto never always)' \
        '--theme[color theme]:theme:' \
        '--list-themes[print available themes]' \
        '--language[force syntax]:language:' \
        '--list-languages[print supported languages]' \
        '--map-syntax[map glob to syntax]:glob\:syntax:' \
        '--ignored-suffix[strip before detection]:suffix:' \
        '--file-name[display name for stdin]:name:' \
        '--fallback-syntax[when detection fails]:syntax:' \
        '--detect-syntax[print detected syntax]' \
        '--diff[show git change markers]' \
        '--diagnostic[print build info]' \
        '--no-config[ignore config files]' \
        '--config-file[print config path]' \
        '--config-dir[print config dir]' \
        '--generate-config-file[print config template]' \
        '--help[show help]' \
        '--version[show version]' \
        '*:file:_files'
}
_mat "$@"
