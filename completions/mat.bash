# bash completion for mat
_mat() {
    local cur prev
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    case "$prev" in
        --style) COMPREPLY=( $(compgen -W "plain default full numbers grid header header-filesize rule snip" -- "$cur") ); return ;;
        --color|--decorations) COMPREPLY=( $(compgen -W "auto never always" -- "$cur") ); return ;;
        --wrap) COMPREPLY=( $(compgen -W "auto never character word" -- "$cur") ); return ;;
        --tabs|--terminal-width) return ;;
    esac

    if [[ "$cur" == -* ]]; then
        local opts="-n -b -s -e -t -v -A -E -T -u -p -S --pretty --chop-long-lines \
--style --color --decorations --wrap --tabs --terminal-width \
--no-config --config-file --config-dir --generate-config-file --help --version"
        COMPREPLY=( $(compgen -W "$opts" -- "$cur") )
        return
    fi
    COMPREPLY=( $(compgen -f -- "$cur") )
}
complete -F _mat mat
