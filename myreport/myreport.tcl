  CreateReport "MyReport" \
      -command    ::Reports::MyReport::Console \
      -cleanup    ::Reports::MyReport::Reset   \
      -desc "My cool report." \
      -longhelp "A longer description of my cool report,\nwith newlines."
  
  namespace eval ::Reports::MyReport { }
  
  proc ::Reports::MyReport::Reset {} { }
  
  # ::Reports::MyReport::FindStuff
  #
  proc ::Reports::MyReport::FindStuff {} {
      global anno
  
      set result "#jobId,name"
      set j [$anno jobs begin]
      set e [$anno jobs end]
      for {} {$j != $e} {set j [$anno job next $j]} {
          if { [$anno job exitcode $j] == 0 } {
              # Good job, skip to the next.
  
              continue
          }
          if { [$anno job type $j] == "rule" || [$anno job type $j] == "continuation" } {
              # Add this job to the output in CSV form.
              append result "\n$j,[$anno job name $j]"
          }
      }
      return $result
  }
  
  # ::Reports::MyReport::Console
  #
  #       Return a list of jobs with errors.
  
  proc ::Reports::MyReport::Console {args} {
      return "[::Reports::MyReport::FindStuff]\n"
  }

