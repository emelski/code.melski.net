source util.tcl

proc main {branchFile matchUser} {
    set branches {
    }

    if { [file exists $branchFile] } {
        set f [open $branchFile r]
        while { ![eof $f] } {
            set branch [gets $f]
            if { [string length $branch] > 0 } {
                lappend branches $branch
            }
        }
        close $f
    }

    if { [llength $branches] == 0 } {
        puts stderr "No branches specified."
        exit 1
    }

    # Convert list of branches to a tree of path components, so we can do
    # the matching more efficiently later.

    ParseBranches $branches

    set interestingFiles false
    set first ""

    fconfigure stdin -buffering full -buffersize 16777216
    while { ![eof stdin] } {
        set line [gets stdin]

        if { [string length $line] == 0 } {
            # Skip blank lines.
        }

        set prefix [string range $line 0 3]
        if { $prefix == "Chan" } {
            # Found the start of a new changelist.  If the previous changelist
            # was "interesting", integrate it into our stats.

            if { $interestingFiles
                 && [string match $matchUser $user] } {
                set day [clock format $timestamp -format "%Y%m%d"]
                if { ![info exists changes($day)] } {
                    set changes($day) 0
                } 
                incr changes($day)
                lappend users($day) $user
                if { $first == "" } {
                    set first $day
                }
                set last $day
            }

            # Now extract the user and timestamp for the new changelist.

            set change [lindex $line 1]
            set u      [lindex $line 3]
            set d      [string map [list / -] [lindex $line 5]]
            set t      [lindex $line 6]

            set timestamp [clock scan "$d $t"]
            set user      [lindex [split $u @] 0]
            if { $change % 500 == 0 } {
                puts stderr "Processing $change"
            }

            # Assume that the new changelist does _not_ have interesting files.

            set interestingFiles false

        } elseif { $prefix == "... " } {
            # Found a filename from the "Affected files ..." section.

            set hash [string first # $line]
            set depotPath [string range $line 4 [expr {$hash - 1}]]
            set op [lindex [string range $line [expr {$hash + 1}] end] end]

            # Ignore itegrate and branch operations:  we should have gotten the
            # "interesting" operation on the originating branch.

            if { $op == "branch" || $op == "integrate" } {
                continue
            }

            # Check for "interesting" files.

            if { [IsInteresting $depotPath filename] } {
                set interestingFiles true
            }
        }
    }

    set start [clock scan $first]
    set end   [clock scan $last]
    set days  [list]
    set userl [list]
    set sum   0

    for {set i $start} {$i <= $end} {incr i 86405} {
        set day [clock format $i -format "%Y%m%d"]
        if { [info exists changes($day)] } {
            lappend days $changes($day)
            lappend userl $users($day)
            incr sum $changes($day)
        } else {
            lappend days 0
            lappend userl {}
        }
        if { [llength $days] > 30 } {
            incr sum [expr {-1 * [lindex $days 0]}]
            set days [lrange $days 1 end]
            set userl [lrange $userl 1 end]
            catch {unset recentUsers}
            foreach sub $userl {
                foreach user $sub {
                    set recentUsers($user) 1
                }
            }

            puts "$day $sum [llength [array names recentUsers]]"
        }
    }
}

set matchUser *
if { [llength $argv] > 1 } {
    set matchUser [lindex $argv 1]
}

main [lindex $argv 0] $matchUser

