# fish completion for mat
complete -c mat -s n -d 'number all lines'
complete -c mat -s b -d 'number nonempty lines'
complete -c mat -s s -d 'squeeze blank lines'
complete -c mat -s v -d 'show nonprinting'
complete -c mat -s e -d 'show ends (-vE)'
complete -c mat -s t -d 'show tabs (-vT)'
complete -c mat -s A -d 'show all (-vET)'
complete -c mat -s E -d 'show line ends'
complete -c mat -s T -d 'show tabs'
complete -c mat -s u -d 'ignored (always unbuffered)'
complete -c mat -s p -l pretty -d 'full decoration frame'
complete -c mat -s S -l chop-long-lines -d 'do not wrap'
complete -c mat -s P -l no-paging -d 'disable pager'
complete -c mat -s d -l diff -d 'show git change markers'
complete -c mat -s L -l list-languages -d 'print supported languages'
complete -c mat -s r -l line-range -d 'print only range' -x
complete -c mat -s H -l highlight-line -d 'emphasize range' -x
complete -c mat -s l -l language -d 'force syntax' -x
complete -c mat -s m -l map-syntax -d 'map glob to syntax' -x
complete -c mat -l style -d 'decoration components' -x -a 'plain default full numbers grid header header-filesize rule snip'
complete -c mat -l color -x -a 'auto never always'
complete -c mat -l decorations -x -a 'auto never always'
complete -c mat -l wrap -x -a 'auto never character word'
complete -c mat -l paging -x -a 'auto never always'
complete -c mat -l tabs -d 'tab width' -x
complete -c mat -l terminal-width -d 'columns' -x
complete -c mat -l squeeze-limit -d 'max blank lines under -s' -x
complete -c mat -l binary -x -a 'no-printing as-text'
complete -c mat -l strip-ansi -x -a 'auto never always'
complete -c mat -l theme -d 'color theme' -x
complete -c mat -l list-themes -d 'print available themes'
complete -c mat -l ignored-suffix -d 'strip before detection' -x
complete -c mat -l file-name -d 'display name for stdin' -x
complete -c mat -l fallback-syntax -d 'when detection fails' -x
complete -c mat -l detect-syntax -d 'print detected syntax'
complete -c mat -l diagnostic -d 'print build info'
complete -c mat -l no-config -d 'ignore config files'
complete -c mat -l config-file -d 'print config path'
complete -c mat -l config-dir -d 'print config dir'
complete -c mat -l generate-config-file -d 'print config template'
complete -c mat -l help -d 'show help'
complete -c mat -l version -d 'show version'
complete -c mat -F
