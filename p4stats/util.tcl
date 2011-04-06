proc ParseBranches {branches} {
    upvar 1 __tree tree
    foreach branch $branches {
        set p ""
        set components [file split $branch]
        foreach component $components {
            set curr $p,$component
            set tree($curr) 0
            set p $component
        }
        set tree($curr) 1
    }
}

proc IsInteresting {depotPath outFilename} {
    upvar 1 __tree tree
    upvar 1 $outFilename filename
    
    set components [file split $depotPath]
    set p ""
    set prefix ""
    set interesting false
    foreach component $components {
        append prefix $component/
        if { ![info exists tree($p,$component)] } {
            # Don't recognize this branch, so skip to the next file.

            break
        }

        if { $tree($p,$component) } {
            # Found a match marking this as an "interesting" branch.

            set interesting true
            break
        }

        set p $component
    }

    if { $interesting } {
        # Filename is everything after the branch name.

        set filename [string range $depotPath [string length $prefix] end]
    }
    return $interesting
}

    
    
