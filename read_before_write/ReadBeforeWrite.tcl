# ReadBeforeWrite.tcl
#
# This file contains Tcl code that implements the ReadBeforeWrite report, which
# finds instances of read-before-write in the build, which usually indicate a
# makefile defect.
#
# To install, copy this file to:
#
#     * $HOME/.ecloud/ElectricInsight/reports (for Linux), or
#     * %USERPROFILE%/Electric Cloud/ElectricInsight/reports (for Windows)
#
# Note that command-line invocation is only supported with ElectricInsight 4.0
# or later.
#
# Copyright (c) 2013 Eric Melski
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
# * The name Eric Melski may not be used to endorse or promote products
# derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY ERIC MELSKI "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL ERIC MELSKI BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

if {$::VERSION == "no version" || [package vcompare "4.0.0" $::VERSION] <= 0} {
    # 4.0 version.

    CreateReport "Read Before Write" \
        -command    ::Reports::ReadBeforeWrite::Console \
        -cleanup    ::Reports::ReadBeforeWrite::Reset   \
        -desc "Find read-before-write patterns in the build." \
        -longhelp "Find examples of reads of a file preceeding writes of the
file, which usually indicate a makefile defect."
} else {
    # Use the old-school pre-4.0 declaration style.

    DeclareReport "Read Before Write" \
        ::Reports::ReadBeforeWrite::SimpleGUI \
        "Find read-before-write patterns in the build."
}

namespace eval ::Reports::ReadBeforeWrite {
}

proc ::Reports::ReadBeforeWrite::Reset {} {
}

# ::Reports::ReadBeforeWrite::FindReadBeforeWrite
#

proc ::Reports::ReadBeforeWrite::FindReadBeforeWrite {} {
    global anno
    set instances [list]

    # Iterate over the files referenced in the build...

    foreach filename [$anno files] {
        set readers [list]

        # Iterate over the operations performed on the file...

        foreach tuple [$anno file operations $filename] {
            foreach {job op dummy} $tuple { break }
            if { $op == "read" || $op == "failedlookup" } {
                # If this is a read operation, note the job that did the read.

                lappend readers $job
            } elseif {$op == "create" || $op == "modify" || $op == "truncate"} {
                # If this is a write operation but earlier jobs already read
                # the file, we've found a read-before-write instance.

                if { [llength $readers] } {
                    lappend instances [list $readers $job $filename]
                }

                # After we see a write on this file we can move on to the next.

                break
            }
        }
    }

    # For each instance, print the filename, the writer, and each reader.

    set result ""
    foreach instance $instances {
        foreach {readers writer filename} $instance { break }
        set writerName [$anno job name $writer]
        set writerFile [$anno job makefile $writer]
        set writerLine [$anno job line $writer]
        append result "FILENAME:\n  $filename\n"
        append result "WRITER  :\n  $writerName ($writerFile:$writerLine)\n"
        append result "READERS :\n"
        foreach reader $readers {
            set readerName [$anno job name $reader]
            set readerFile [$anno job makefile $reader]
            set readerLine [$anno job line $reader]
            append result "  $readerName ($readerFile:$readerLine)\n"
        }
    }

    if { [string length $result] == 0 } {
        set result "No instances of read-before-write found.\n"
    }
    return $result
}

# ::Reports::ReadBeforeWrite::Console
#
#       Get a tab-separated list of the longest jobs in the build.

proc ::Reports::ReadBeforeWrite::Console {args} {
    return [::Reports::ReadBeforeWrite::FindReadBeforeWrite]
}

# ::Reports::ReadBeforeWrite::SimpleGUI
#
#       Run the console report but put it in a text widget.

proc ::Reports::ReadBeforeWrite::SimpleGUI {win progressVarName} {
    frame $win
    text $win.t -font {Courier -12} -background white -exportselection 1 \
        -yscrollcommand [list $win.sbv set] \
        -xscrollcommand [list $win.sbh set]
    ttk::scrollbar $win.sbv -orient vertical -command [list $win.t yview]
    ttk::scrollbar $win.sbh -orient horizontal -command [list $win.t xview]

    $win.t insert end [::Reports::ReadBeforeWrite::FindReadBeforeWrite]
    $win.t configure -state disabled
    grid $win.t     $win.sbv    -sticky nsew
    grid $win.sbh               -sticky ew
    grid rowconfigure $win 0    -weight 1
    grid columnconfigure $win 0 -weight 1
    return $win
}
