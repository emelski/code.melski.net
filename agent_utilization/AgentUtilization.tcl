# AgentUtilization.tcl
#
# This file contains Tcl code that implements the Agent Utilization report for
# ElectricInsight, which computes how many agents the build actually used at
# once, and for how long.
#
# Copyright (c) 2010 Eric Melski
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * The name Eric Melski may not be used to endorse or promote products
#       derived from this software without specific prior written permission.
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

DeclareReport "Agent utilization" \
    CreateAgentUtilizationGUI \
    {This report shows how many agents the build used simultaneously, and for how long.}

proc CreateAgentUtilizationGUI {w unused} {
    set results [GetAgentUtilization]
    set xmax    [lindex $results end-1]
    set ymax    [expr {[lindex $results 1] + 1}]
    set xmin    0
    set ymin    0
    set xstep   [GetStepSize $xmax]
    set ystep   [GetStepSize $ymax]
    set xmax    [expr {int($xstep * ceil(double($xmax / $xstep)))}]
    set ymax    [expr {int($ystep * ceil(double($ymax / $ystep))) + 1}]

    canvas $w -background white
    set p [::Plotchart::createFStepPlot $w \
               [list $xmin $xmax $xstep] \
               [list $ymin $ymax $ystep]]
    $p dataconfig series1 -color #3399cc
    $p title "Agent utilization"
    $p xtext "Cumulative time (s)"
    $p ytext "A\nG\n\E\nN\nT\nS"
    $p plot series1 0 [lindex $results 1]
    foreach {x y} $results {
        $p plot series1 $x $y
    }
#    bind $w <Configure> [list ::Plotchart::AutoScale $w]
    return $w
}


proc GetAgentUtilization {} {
    global anno

    # These values will tell us what type of event we're dealing with later.

    set JOB_START_EVENT  1
    set JOB_END_EVENT   -1

    # Aggregate jobs by agent.

    foreach agent [$anno agents] {
        set pseudo(start)  -1
        set pseudo(finish) -1
        foreach job [$anno agent jobs $agent] {
            set start  [$anno job start  $job]
            set finish [$anno job finish $job]
            if { $pseudo(start) == -1 } {
                set pseudo(start)  $start
                set pseudo(finish) $finish
            } else {
                if { int($start * 100) <= int($pseudo(finish) * 100) } {
                    set pseudo(finish) $finish
                } else {
                    lappend events \
                        [list $pseudo(start)  $JOB_START_EVENT] \
                        [list $pseudo(finish) $JOB_END_EVENT]
                    set pseudo(start)  $start
                    set pseudo(finish) $finish
                }
            }
        }
    }

    # Order the events chronologically.

    set events [lsort -real -increasing -index 0 $events]

    # Scan the list of events.  Every time we see a START event, increment the
    # count of agents in use; every time we see an END event, decrement the
    # count.  This way, "count" always reflects the number of agents in use.

    set count 0
    set last  0

    foreach event $events {
        foreach {t e} $event { break }
        if { ![info exists total($count)] } {
            set total($count) 0
        }

        # Add the time interval between the current and the previous event to
        # the total time for "count".

        set total($count) [expr {$total($count) + ($t - $last)}]

        # Update the in-use counter.  I chose the event type values
        # so that we can simply add the event type to the counter.

        incr count $e

        # Track the current time, so we can compute the size of the next
        # interval.

        set last $t
    }

    set t 0
    foreach c [lsort -integer -decreasing [array names total]] {
        set t [expr {$t + $total($c)}]
        lappend result $t $c
    }

    return $result
}

proc GetStepSize { range {guide 20} } {
    set power [expr {pow(10, floor(log10($range)))}]
    set xnorm [expr {double($range) / $power}]
    set posns [expr {double($guide) / $xnorm}]

    if { $posns > 40 } {
        set tics 0.05
    } elseif { $posns > 20 } {
        set tics 0.1
    } elseif { $posns > 10 } {
        set tics 0.2
    } elseif { $posns > 4 } {
        set tics 0.5
    } elseif { $posns > 2 } {
        set tics 1
    } elseif { $posns > 0.5 } {
        set tics 2
    } else {
        set tics [expr {ceil($xnorm)}]
    }
    return [expr {int($tics * $power)}]
}

# The remaining code adds a gnuplot-like fstep chart type to the Plotchart
# package.  It is not related to the annotation analysis per se.

namespace eval ::Plotchart {
    set methodProc(fstep,title)             DrawTitle
    set methodProc(fstep,xtext)             DrawXtext2
    set methodProc(fstep,ytext)             DrawYtext2
    set methodProc(fstep,plot)              DrawFStepData
    set methodProc(fstep,grid)              DrawGrid
    set methodProc(fstep,contourlines)      DrawIsolines
    set methodProc(fstep,contourfill)       DrawShades
    set methodProc(fstep,contourbox)        DrawBox
    set methodProc(fstep,saveplot)          SavePlot
    set methodProc(fstep,dataconfig)        DataConfig
    set methodProc(fstep,xconfig)           XConfig
    set methodProc(fstep,yconfig)           YConfig
    catch {
    variable config
    lappend config(charttypes) fstep
    set config(fstep,components) {title margin text legend axis background}
    foreach comp $config(fstep,components) {
        foreach prop $config($comp,properties) {
            set config(fstep,$comp,$prop)               [set _$prop]
            set config(fstep,$comp,$prop,default)       [set _$prop]
        }
    }
    }
}

proc ::Plotchart::DrawFStepData { w series xcrd ycrd } {
   variable data_series
   variable scaling

   #
   # Draw the line piece
   #
   set colour "black"
   if { [info exists data_series($w,$series,-colour)] } {
      set colour $data_series($w,$series,-colour)
   }

   set type "line"
   if { [info exists data_series($w,$series,-type)] } {
      set type $data_series($w,$series,-type)
   }

   foreach {pxcrd pycrd} [coordsToPixel $w $xcrd $ycrd] {break}

   if { [info exists data_series($w,$series,x)] } {
      set xold $data_series($w,$series,x)
      set yold $data_series($w,$series,y)
      foreach {pxold pyold} [coordsToPixel $w $xold $yold] {break}
      if { $type == "line" || $type == "both" } {
         $w create line \
             $pxold $pyold \
             $pxold $pycrd \
             $pxcrd $pycrd \
             -fill $colour -tag data -width 2
      }
   }

   if { $type == "symbol" || $type == "both" } {
      set symbol "dot"
      if { [info exists data_series($w,$series,-symbol)] } {
         set symbol $data_series($w,$series,-symbol)
      }
      DrawSymbolPixel $w $series $pxcrd $pycrd $symbol $colour
   }

   $w lower data

   set data_series($w,$series,x) $xcrd
   set data_series($w,$series,y) $ycrd
}

proc ::Plotchart::createFStepPlot { w xscale yscale } {
    variable data_series
    variable config
    
   foreach s [array names data_series "$w,*"] {
      unset data_series($s)
   }

   set newchart "fstep_$w"
   interp alias {} $newchart {} ::Plotchart::PlotHandler fstep $w

   CopyConfig fstep $w
   foreach {pxmin pymin pxmax pymax} [MarginsRectangle $w] {break}

   foreach {xmin xmax xdelt} $xscale {break}
   foreach {ymin ymax ydelt} $yscale {break}

   if { $xdelt == 0.0 || $ydelt == 0.0 } {
      return -code error "Step size can not be zero"
   }

   if { ($xmax-$xmin)*$xdelt < 0.0 } {
      set xdelt [expr {-$xdelt}]
   }
   if { ($ymax-$ymin)*$ydelt < 0.0 } {
      set ydelt [expr {-$ydelt}]
   }

   viewPort         $w $pxmin $pymin $pxmax $pymax
   worldCoordinates $w $xmin  $ymin  $xmax  $ymax

   DrawYaxis2       $w $ymin  $ymax  $ydelt
   DrawXaxis2       $w $xmin  $xmax  $xdelt
   DrawMask         $w

   return $newchart
}

proc ::Plotchart::DrawXaxis2 { w xmin xmax xdelt } {
   variable scaling

   set scaling($w,xdelt) $xdelt

   $w delete xaxis

   $w create line $scaling($w,pxmin) $scaling($w,pymax) \
                  $scaling($w,pxmax) $scaling($w,pymax) \
                  -fill black -tag xaxis

   set format ""
   if { [info exists scaling($w,-format,x)] } {
      set format $scaling($w,-format,x)
   }

   set x $xmin
   while { $x < $xmax+0.5*$xdelt } {
      foreach {xcrd ycrd} [coordsToPixel $w $x $scaling($w,ymin)] {break}

      set xlabel $x
      if { $format != "" } {
         set xlabel [format $format $x]
      }
       $w create text $xcrd [expr {$ycrd+5}] -text $xlabel -tag xaxis -anchor n
       $w create line $xcrd [expr {$ycrd-5}] $xcrd [expr {$ycrd+1}]
      set x [expr {$x+$xdelt}]
      if { abs($x) < 0.5*$xdelt } {
         set x 0.0
      }
   }

   set scaling($w,xdelt) $xdelt
}

proc ::Plotchart::DrawYaxis2 { w ymin ymax ydelt } {
   variable scaling

   set scaling($w,ydelt) $ydelt

   $w delete yaxis

   $w create line $scaling($w,pxmin) $scaling($w,pymin) \
                  $scaling($w,pxmin) $scaling($w,pymax) \
                  -fill black -tag yaxis

   set format ""
   if { [info exists scaling($w,-format,y)] } {
      set format $scaling($w,-format,y)
   }

   set y $ymin
   while { $y < $ymax+0.5*$ydelt } {
      foreach {xcrd ycrd} [coordsToPixel $w $scaling($w,xmin) $y] {break}
      set ylabel $y
      if { $format != "" } {
         set ylabel [format $format $y]
      }
       $w create text [expr {$xcrd-5}] $ycrd -text $ylabel -tag yaxis -anchor e
       $w create line $xcrd $ycrd [expr {$xcrd+5}] $ycrd
      set y [expr {$y+$ydelt}]
      if { abs($y) < 0.5*$ydelt } {
         set y 0.0
      }
   }
}

proc ::Plotchart::DrawXtext2 { w text } {
   variable scaling

   set xt [expr {($scaling($w,pxmin)+$scaling($w,pxmax))/2}]
   set yt [expr {$scaling($w,pymax)+20}]

   $w create text $xt $yt -text $text -fill black -anchor n -tag xtext
}

proc ::Plotchart::DrawYtext2 { w text } {
   variable scaling

    set xt [expr {$scaling($w,pxmin)-40}]
    set yt [expr {$scaling($w,pymin)+($scaling($w,pymax)/2)}]

    $w create text $xt $yt -text $text -fill black -anchor e -justify center
}
