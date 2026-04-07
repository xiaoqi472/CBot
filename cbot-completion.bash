# cbot bash completion script
# 安装方式: sudo cp cbot-completion.bash /etc/bash_completion.d/cbot

_cbot_completion() {
    local cur prev words cword
    _init_completion || return

    local commands="init cmake build doc format commit test_llm"

    case $cword in
        1)
            COMPREPLY=($(compgen -W "$commands" -- "$cur"))
            ;;
        2)
            case $prev in
                init)
                    # 项目名，无法补全，提示占位
                    COMPREPLY=()
                    ;;
                cmake)
                    # 目录补全
                    COMPREPLY=($(compgen -d -- "$cur"))
                    ;;
                doc)
                    # C/C++ 文件补全
                    COMPREPLY=($(compgen -f -X '!*.@(cpp|hpp|c|h)' -- "$cur"))
                    ;;
                format)
                    # --init 或文件/目录
                    COMPREPLY=($(compgen -W "--init" -- "$cur"))
                    COMPREPLY+=($(compgen -f -X '!*.@(cpp|hpp|c|h)' -- "$cur"))
                    COMPREPLY+=($(compgen -d -- "$cur"))
                    ;;
                build|commit|test_llm)
                    COMPREPLY=()
                    ;;
            esac
            ;;
        *)
            # 3 个参数及以后
            case ${words[1]} in
                doc)
                    COMPREPLY=($(compgen -f -X '!*.@(cpp|hpp|c|h)' -- "$cur"))
                    ;;
                format)
                    COMPREPLY=($(compgen -W "--init" -- "$cur"))
                    COMPREPLY+=($(compgen -f -X '!*.@(cpp|hpp|c|h)' -- "$cur"))
                    COMPREPLY+=($(compgen -d -- "$cur"))
                    ;;
                *)
                    COMPREPLY=()
                    ;;
            esac
            ;;
    esac
}

complete -F _cbot_completion cbot
