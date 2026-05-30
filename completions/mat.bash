# bash completion for mat
_mat() {
    local cur prev
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    case "$prev" in
        --style) COMPREPLY=( $(compgen -W "plain default full numbers grid header header-filesize rule snip" -- "$cur") ); return ;;
        --color|--decorations|--paging) COMPREPLY=( $(compgen -W "auto never always" -- "$cur") ); return ;;
        --wrap) COMPREPLY=( $(compgen -W "auto never character word" -- "$cur") ); return ;;
        --binary) COMPREPLY=( $(compgen -W "no-printing as-text" -- "$cur") ); return ;;
        --strip-ansi) COMPREPLY=( $(compgen -W "auto never always" -- "$cur") ); return ;;
        --theme) COMPREPLY=( $(compgen -W "$(mat --list-themes 2>/dev/null)" -- "$cur") ); return ;;
        -l|--language) COMPREPLY=( $(compgen -W "$(mat -L 2>/dev/null)" -- "$cur") ); return ;;
        --tabs|--terminal-width|--squeeze-limit|-r|--line-range|-H|--highlight-line) return ;;
        -m|--map-syntax|--ignored-suffix|--file-name|--fallback-syntax) return ;;
    esac

    if [[ "$cur" == -* ]]; then
        local opts="-n -b -s -e -t -v -A -E -T -u -p -S -P -d -r -H -l -L -m
--pretty --chop-long-lines --style --color --decorations --wrap --tabs
--terminal-width --no-paging --paging --line-range --highlight-line
--squeeze-limit --binary --strip-ansi --theme --list-themes --language
--list-languages --map-syntax --ignored-suffix --file-name --fallback-syntax
--detect-syntax --diff --diagnostic --no-config --config-file --config-dir
--generate-config-file --help --version"
        COMPREPLY=( $(compgen -W "$opts" -- "$cur") )
        return
    fi
    COMPREPLY=( $(compgen -f -- "$cur") )
}
complete -F _mat mat
