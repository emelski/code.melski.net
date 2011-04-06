#!tclsh
#
# p4commits
#
# Collect commit stats from a collection of perforce changelist descriptions,
# outputting the number of commits in the previous 30 days for each 30-day
# period in the timeframe covered by the changelists.
#
#       1.  Extract data from perforce.  Find the highest changelist in your
#           depot, and use something like the following (bash 3.0+):
#               for n in {1..MAX} ; do
#                   p4 describe -s $n >> perforce.log
#               done
#       2.  Create a configuration file enumerating the branches you want to
#           track, one per line.
#       3.  Run this script as
#               tclsh p4commits.tcl branches.conf < perforce.log > commits.dat
#           or, to get stats for a specific user
#               tclsh p4commits.tcl branches.conf username < perforce.log > commits.dat
#
# For more information on perforce, see http://www.perforce.com/
#
# Copyright (c) 2011, Eric Melski
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Eric Melski's name may not be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

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

