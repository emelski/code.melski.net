#!tclsh
#
# p42gource
#
# Convert a collection of perforce changelist descriptions to a gource-format
# log.  Usage instructions:
#
#       1.  Extract data from perforce.  Find the highest changelist in your
#           depot, and use something like the following (bash 3.0+):
#               for n in {1..MAX} ; do
#                   p4 describe -s $n >> perforce.log
#               done
#       2.  Enumerate the branches you are interested in below, one per line
#           in the "branches" variable.
#       3.  Run this script as
#               tclsh p42gource < perforce.log > gource.log
#
# Then use the gource custom log format to render the log:
#
#       gource --log-format custom gource.log
#
# For more information on gource, see http://code.google.com/p/gource/.
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

    # List of branches of the depot that we're interested in.  List one branch
    # per line, for example:
    #
    # set branches {
    #     //depot/foo/1.0
    #     //depot/foo/2.0
    #     //depot/foo/3.0
    # }

    set branches {

    }

    if { [file exists branches.tcl] } {
        source branches.tcl
    }

    if { [llength $branches] == 0 } {
        puts stderr "No branches specified."
        exit 1
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

    fconfigure stdin -buffering full -buffersize 16777216
    while { ![eof stdin] } {
        set line [gets stdin]

        if { [string length $line] == 0 } {
            # Skip blank lines.
        }

        set prefix [string range $line 0 3]
        if { $prefix == "Chan" } {
            # Found the start of a new changelist.  Extract the user and
            # timestamp.

            set change [lindex $line 1]
            set u      [lindex $line 3]
            set d      [string map [list / -] [lindex $line 5]]
            set t      [lindex $line 6]

            set timestamp [clock scan "$d $t"]
            set user      [lindex [split $u @] 0]
            if { $change % 500 == 0 } {
                puts stderr "Processing $change"
            }
        } elseif { $prefix == "... " } {
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
                    # Don't recognize this branch, so skip to the next file.

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
    puts stderr "Finished through changelist $change"
}

main
