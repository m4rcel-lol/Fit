#!/bin/bash
# Bash completion script for Fit VCS
# Install: source this file or copy to /etc/bash_completion.d/

_fit_completion() {
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    # Main commands
    local commands="init add commit log status diff branch checkout daemon push pull clone restore gc snapshot help credits"

    # If we're completing the first argument (the command)
    if [ $COMP_CWORD -eq 1 ]; then
        COMPREPLY=( $(compgen -W "${commands}" -- ${cur}) )
        return 0
    fi

    # Command-specific completions
    case "${prev}" in
        add)
            # Complete with files in current directory
            COMPREPLY=( $(compgen -f -- ${cur}) )
            return 0
            ;;
        commit)
            # Suggest -m flag
            COMPREPLY=( $(compgen -W "-m" -- ${cur}) )
            return 0
            ;;
        checkout|branch)
            # Complete with branch names
            if [ -d .fit/refs/heads ]; then
                local branches=$(ls .fit/refs/heads 2>/dev/null)
                COMPREPLY=( $(compgen -W "${branches}" -- ${cur}) )
            fi
            return 0
            ;;
        diff|restore)
            # Complete with recent commit hashes
            if [ -f .fit/HEAD ]; then
                local commits=$(./bin/fit log 2>/dev/null | grep '^commit' | awk '{print substr($2,1,8)}' | head -20)
                COMPREPLY=( $(compgen -W "${commits}" -- ${cur}) )
            fi
            return 0
            ;;
        daemon)
            COMPREPLY=( $(compgen -W "--port" -- ${cur}) )
            return 0
            ;;
        snapshot)
            COMPREPLY=( $(compgen -W "-m" -- ${cur}) )
            return 0
            ;;
    esac

    # Default to file completion
    COMPREPLY=( $(compgen -f -- ${cur}) )
    return 0
}

# Register the completion function
complete -F _fit_completion fit

echo "Fit bash completion loaded! Try typing 'fit <TAB>'"
