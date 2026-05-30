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
        '--style[decoration components]:components:(plain default full numbers grid header header-filesize rule snip)' \
        '--color[when to colorize]:when:(auto never always)' \
        '--decorations[when to decorate]:when:(auto never always)' \
        '--wrap[wrap mode]:mode:(auto never character word)' \
        '--tabs[tab width]:n:' \
        '--terminal-width[columns]:n:' \
        '--no-config[ignore config files]' \
        '--config-file[print config path]' \
        '--config-dir[print config dir]' \
        '--generate-config-file[print config template]' \
        '--help[show help]' \
        '--version[show version]' \
        '*:file:_files'
}
_mat "$@"
