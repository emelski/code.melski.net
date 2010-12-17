#   Bash completion for ElectricAccelerator
#
#   Copyright © 2010, Eric Melski <ericm@electric-cloud.com>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2, or (at your option)
#   any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software Foundation,
#   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
#   Portions of this software are derived from the bash-completion project,
#   available here:
#
#   http://bash-completion.alioth.debian.org/

# Get the word to complete.
# This is nicer than ${COMP_WORDS[$COMP_CWORD]}, since it handles cases
# where the user is completing in the middle of a word.
# (For example, if the line is "ls foobar",
# and the cursor is here -------->   ^
# it will complete just "foo", not "foobar", which is what the user wants.)
#
#
# Accepts an optional parameter indicating which characters out of
# $COMP_WORDBREAKS should NOT be considered word breaks. This is useful
# for things like scp where we want to return host:path and not only path.
#
# From the bash-completion project.

_get_cword()
{
        if [[ "${#COMP_WORDS[COMP_CWORD]}" -eq 0 ]] || [[ "$COMP_POINT" == "${#COMP_LINE}" ]]; then
                echo "${COMP_WORDS[COMP_CWORD]}"
        else
                local i
                local cur="$COMP_LINE"
                local index="$COMP_POINT"
                for (( i = 0; i <= COMP_CWORD; ++i )); do
                        while [[ "${#cur}" -ge ${#COMP_WORDS[i]} ]] && [[ "${cur:0:${#COMP_WORDS[i]}}" != "${COMP_WORDS[i]}" ]]; do
                                cur="${cur:1}"
                                index="$(( index - 1 ))"
                        done
                        if [[ "$i" -lt "$COMP_CWORD" ]]; then
                                local old_size="${#cur}"
                                cur="${cur#${COMP_WORDS[i]}}"
                                local new_size="${#cur}"
                                index="$(( index - old_size + new_size ))"
                        fi
                done
                
                if [[ "${COMP_WORDS[COMP_CWORD]:0:${#cur}}" != "$cur" ]]; then
                        # We messed up! At least return the whole word so things 
                        # keep working
                        echo "${COMP_WORDS[COMP_CWORD]}"
                else
                        echo "${cur:0:$index}"
                fi
        fi
}

# This function performs file and directory completion. It's better than
# simply using 'compgen -f', because it honours spaces in filenames.
# If passed -d, it completes only on directories. If passed anything else,
# it's assumed to be a file glob to complete on.
#
# From the bash-completion package.

_filedir()
{
        local IFS=$'\t\n' xspec

        local toks=( ) tmp
        while read -r tmp; do
                [[ -n $tmp ]] && toks[${#toks[@]}]=$tmp
        done < <( compgen -d -- "$(quote_readline "$cur")" )
        
        if [[ "$1" != -d ]]; then
                xspec=${1:+"!*.$1"}
                while read -r tmp; do
                        [[ -n $tmp ]] && toks[${#toks[@]}]=$tmp
                done < <( compgen -f -X "$xspec" -- "$(quote_readline "$cur")" )
        fi

        COMPREPLY=( "${COMPREPLY[@]}" "${toks[@]}" )
}

# Filter a list of words ($1) out of another list of words ($2).
# Result is stored in $_filtered after this function returns.

_filterout()
{
        local IFS=$'\t\n ,' n
        _filtered=$2
        for n in $1 ; do
            _filtered=${_filtered/$n/}
        done
        return 0
}

# ElectricMake completion
#
_emake()
{
        local file makef makef_dir="." makef_inc cur prev i cc pp opts

        COMPREPLY=()
        cur=`_get_cword`
        prev=${COMP_WORDS[COMP_CWORD-1]}

        # --name value style option
        case $prev in
                -f|-o|-W|--file|--makefile|--old-file|--new-file|--assume-old|--assume-new|--what-if)
                        _filedir
                        return 0
                        ;;
                -I|-C|-directory|--include-dir)
                        _filedir -d
                        return 0
                        ;;
        esac

        # --name=value style option
        if [[ "$cur" == *=* ]]; then
                prev=${cur/=*/}
                cur=${cur/*=/}
                case "$prev" in
                        --file|--makefile|--emake-annofile|--emake-historyfile|--emake-ledgerfile|--emake-subbuild-db)
                                _filedir
                                return 0
                                ;;
                        --directory|--include-dir|--emake-tmpdir)
                                _filedir -d
                                return 0
                                ;;
                        --emake-debug)
                                pp=""
                                cc=${cur}
                                if [[ -n $cc ]] ; then
                                        pp="-P $cc"
                                        cc=""
                                fi
                                COMPREPLY=( $( compgen -W 'a c f g h j L l m n p r s t e o D M S' $pp -- $cc ) )
                                return 0
                                ;;


                        --emake-clearcase)
                                pp=""
                                cc=${cur}
                                opts='rofs vobs'
                                if [[ "$cur" == *,* ]]; then
                                    cc=${cur/*,/}
                                    pp="-P ${cur/,$cc/,}"
                                    _filterout "$pp" "$opts"
                                    opts=$_filtered

                                fi
                                COMPREPLY=( $( compgen -W "$opts" $pp -- $cc ) )
                                return 0
                                ;;
                                        
                        --emake-history)
                                COMPREPLY=( $( compgen -W 'create merge read' -- $cur ) )
                                return 0
                                ;;

                        --emake-emulation)
                                COMPREPLY=( $( compgen -W 'gmake gmake3.80 gmake3.81 ant' -- $cur ) )
                                return 0
                                ;;
                                        
                        --emake-ledger)
                                pp=""
                                cc=${cur}
                                opts='timestamp size command'
                                if [[ "$cur" == *,* ]]; then
                                    cc=${cur/*,/}
                                    pp="-P ${cur/,$cc/,}"
                                    _filterout "$pp" "$opts"
                                    opts=$_filtered
                                fi
                                COMPREPLY=( $( compgen -W "$opts" $pp -- $cc ) )
                                return 0
                                ;;
                                        
                        --emake-annodetail)
                                pp=""
                                cc=${cur}
                                levels='basic env file history lookup registry waiting'
                                if [[ "$cur" == *,* ]]; then
                                    cc=${cur/*,/}
                                    pp="-P ${cur/,$cc/,}"
                                    _filterout "$pp" "$levels"
                                    levels=$_filtered
                                fi
                                COMPREPLY=( $( compgen -W "$levels" $pp -- $cc ) )
                                return 0
                                ;;

                        --emake-annoupload|--emake-autodepend|--emake-blind-create|--emake-collapse|--emake-disable-chain-cleanup|--emake-disable-variable-pruning|--emake-history-force|--emake-mergestreams|--emake-pedantic|--emake-showinfo|--emake-gen-subbuild-db)
                                COMPREPLY=( $( compgen -W '0 1' -- $cur ) )
                                return 0
                                ;;
                esac
        fi

        if [[ "$cur" == -* ]]; then
                COMPREPLY=( $( compgen -W '-b -B -C -e -f -h -i -I\
                        -j -l -k -m -n -r -R - s -S -v -w -W \
                        --always-make                           \
                        --assume-new=                           \
                        --directory=                            \
                        --dont-keep-going                       \
                        --dry-run                               \
                        --environment-overrides                 \
                        --file=                                 \
                        --help                                  \
                        --ignore-errors                         \
                        --include=                              \
                        --include-dir=                          \
                        --jobs                                  \
                        --just-print                            \
                        --keep-going                            \
                        --load-average                          \
                        --makefile=                             \
                        --max-load                              \
                        --new-file=                             \
                        --no-builtin-rules                      \
                        --no-builtin-variables                  \
                        --no-print-directory                    \
                        --print-directory                       \
                        --quiet                                 \
                        --recon                                 \
                        --silent                                \
                        --stop                                  \
                        --touch                                 \
                        --version                               \
                        --warn-undefined-variables              \
                        --what-if=                              \
                        --emake-annodetail=                     \
                        --emake-annofile=                       \
                        --emake-annoupload=                     \
                        --emake-autodepend=                     \
                        --emake-big-file-size=                  \
                        --emake-blind-create=                   \
                        --emake-build-label=                    \
                        --emake-class=                          \
                        --emake-cluster-timeout=                \
                        --emake-cm=                             \
                        --emake-collapse=                       \
                        --emake-debug=                          \
                        --emake-disable-chain-cleanup=          \
                        --emake-disable-pragma=                 \
                        --emake-disable-variable-pruning=       \
                        --emake-emulation=                      \
                        --emake-emulation-table=                \
                        --emake-exclude-env=                    \
                        --emake-history=                        \
                        --emake-historyfile=                    \
                        --emake-history-force=                  \
                        --emake-idle-time=                      \
                        --emake-impersonate-user=               \
                        --emake-job-limit=                      \
                        --emake-ledger=                         \
                        --emake-ledgerfile=                     \
                        --emake-clearcase=                      \
                        --emake-logfile=                        \
                        --emake-logfile-mode=                   \
                        --emake-maxagents=                      \
                        --emake-mem-limit=                      \
                        --emake-mergestreams=                   \
                        --emake-pedantic=                       \
                        --emake-priority=                       \
                        --emake-rdebug=                         \
                        --emake-remake-limit=                   \
                        --emake-resource=                       \
                        --emake-rlogdir=                        \
                        --emake-root=                           \
                        --emake-read-only=                      \
                        --emake-showinfo=                       \
                        --emake-subbuild-db=                    \
                        --emake-gen-subbuild-db=                \
                        --emake-tmpdir=                         \
                        --emake-proxy-dir=                      \
                        --emake-crossmake=                      \
                        ' -- $cur ) )
        else
                # before we check for makefiles, see if a path was specified
                # with -C
                for (( i=0; i < ${#COMP_WORDS[@]}; i++ )); do
                        if [[ ${COMP_WORDS[i]} == -C ]]; then
                                # eval for tilde expansion
                                eval makef_dir=${COMP_WORDS[i+1]}
                                break
                        fi
                done

                # make reads `GNUmakefile', then `makefile', then `Makefile'
                if [ -f ${makef_dir}/GNUmakefile ]; then
                        makef=${makef_dir}/GNUmakefile
                elif [ -f ${makef_dir}/makefile ]; then
                        makef=${makef_dir}/makefile
                elif [ -f ${makef_dir}/Makefile ]; then
                        makef=${makef_dir}/Makefile
                else
                        makef=${makef_dir}/*.mk        # local convention
                fi

                # before we scan for targets, see if a Makefile name was
                # specified with -f
                for (( i=0; i < ${#COMP_WORDS[@]}; i++ )); do
                        if [[ ${COMP_WORDS[i]} == -f ]]; then
                                # eval for tilde expansion
                                eval makef=${COMP_WORDS[i+1]}
                                break
                        fi
                done

                [ ! -f $makef ] && return 0

                # deal with included Makefiles
                makef_inc=$( grep -E '^-?include' $makef | sed -e "s,^.* ,"$makef_dir"/," )

                for file in $makef_inc; do
                        [ -f $file ] && makef="$makef $file"
                done

                COMPREPLY=( $( awk -F':' '/^[a-zA-Z0-9][^$#\/\t=]*:([^=]|$)/ \
                                {split($1,A,/ /);for(i in A)print A[i]}' \
                                $makef 2>/dev/null | command grep "^$cur" ) \
                                $( compgen -f ) )
        fi
} &&
complete -o nospace -F _emake $filenames emake

# ElectricnInsight completion: match *.anno and *.xml files, and directories.

_einsight()
{
        local cur prev xspec IFS=$'\t\n'
        cur=`_get_cword`

        COMPREPLY=()
        COMPREPLY=( $( compgen -f -X "!*.anno" -- $cur ) \
                    $( compgen -f -X "!*.xml"  -- $cur ) \
                    $( compgen -d -- $cur ) )
        return 0
} &&
complete -F _einsight $filenames einsight
