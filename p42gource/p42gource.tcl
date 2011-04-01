#!tclsh

if { [catch {package require Tcl 8.6}] } {
    puts stderr "p42gource requires Tcl 8.6 or later"
    exit 1
}

proc main {} {
    global argv

    array set ops {
        branch      A
        move/add    A
        add         A
        edit        M
        integrate   M
        delete      D
        move/delete D
        purge       D
    }

    # List of branches of the depot that we're interested in.

    set branches {
        //depot/ecloud/2.1
        //depot/ecloud/2.2
        //depot/ecloud/2.3
        //depot/ecloud/2.3.0
        //depot/ecloud/3.0
        //depot/ecloud/3.0.2
        //depot/ecloud/3.0.3
        //depot/ecloud/3.0.4
        //depot/ecloud/3.5
        //depot/ecloud/3.6
        //depot/ecloud/3.7
        //depot/ecloud/4.0
        //depot/ecloud/4.1
        //depot/ecloud/4.2
        //depot/ecloud/4.3
        //depot/ecloud/4.4
        //depot/ecloud/4.5
        //depot/ecloud/4.6
        //depot/ecloud/5.0
        //depot/ecloud/5.1
        //depot/ecloud/5.2
        //depot/ecloud/5.3
        //depot/ecloud/5.4
        //depot/ecloud/main
        //depot/ecloud/autodep
        //depot/ecloud/other
        //depot/ecloud/other/autodep
        //depot/ecloud/other/blind
        //depot/ecloud/other/newcm
        //depot/ecloud/other/sharedcache
        //depot/ecloud/other/subbuilds
        //depot/ecloud/other/versionfs
        //depot/ecloud/other/orderonly
        //depot/ecloud/other/registry
        //depot/ecloud/other/64bit
    }

    # Convert list of branches to a tree of path components, so we can do
    # the matching more efficiently later.

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

    while { ![eof stdin] } {
        set line [gets stdin]

        if { [string length $line] == 0 } {
            # Skip blank lines.
        }

        if { [string range $line 0 5] == "Change" } {
            # Found the start of a new changelist.  Extract the user and
            # timestamp.

            set change [lindex $line 1]
            set u      [lindex $line 3]
            set d      [lindex $line 5]
            set t      [lindex $line 6]

            set timestamp [clock scan "$d $t" -format "%Y/%m/%d %H:%M:%S"]
            set user      [lindex [split $u @] 0]
            if { $change % 500 == 0 } {
                puts stderr "Processing $change"
            }
        } elseif { [string range $line 0 2] == "..." } {
            # Found a filename from the "Affected files ..." section.

            set hash [string first # $line]
            set filename [string range $line 4 [expr {$hash - 1}]]
            set op [lindex [string range $line [expr {$hash + 1}] end] end]

            # Ignore itegrate and branch operations:  we should have gotten the
            # "interesting" operation on the originating branch.

            if { $op == "branch" || $op == "integrate" } {
                continue
            }

            # Filter for branches we care about.

            set components [file split $filename]
            set p ""
            set prefix ""
            set skip true
            foreach component $components {
                append prefix $component/
                if { ![info exists tree($p,$component)] } {
                    # Don't recognize this branch, so bail out.

                    break
                }

                if { $tree($p,$component) } {
                    # Found a match marking this as an "interesting" branch.

                    set skip false
                    break
                }

                set p $component
            }

            if { $skip } {
                continue
            }

            # Filename is everything after the branch name.

            set filename [string range $filename [string length $prefix] end]

            if { ![info exists ops($op)] } {
                puts stderr "Error in changelist $change: $line"
                continue
            }

            # Gource custom log format is:
            #
            #   epoch|username|operation|filename

            puts "$timestamp|$user|$ops($op)|$filename"
        }
    }
    puts "Finished through changelist $change"
}

main
