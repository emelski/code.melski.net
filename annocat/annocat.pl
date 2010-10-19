#!/usr/bin/perl

# annocat.pl
#
# A simple script that concatenates a set of annotation files, adjusting
# timing information and job references to present the appearance of a single
# build.  This is useful in situations where a single logical "build" is
# implemented as a series of distinct emake instances.
#
# Copyright (c) 2009, Electric Cloud Inc.
# All rights reserved.

use XML::Parser;

my $gEnabled = 0;                       # If non-zero, we will echo parsed XML
                                        # to stdout; else we will swallow it.
my $gBuildCount = 0;                    # Number of <build> tags we've seen;
                                        # this is used to provide scoping for
                                        # the jobId's in the annotation.
my $gElapsed = 0;                       # Elapsed time as of the begining of
                                        # the latest <build> tag.
my $gMax = 0;                           # Largest time value seen so far.
my $gCurrentBuildId = 0;                # Emake buildId for the current
                                        # annofile.
my $gFakeJobId = 1;                     # Next jobId to use for the stub jobs
                                        # we insert to create the illusion of
                                        # a single build.
my $gBuffering = 0;                     # If nonzero, output will be
                                        # temporarily buffered instead of
                                        # immediately printed, so that we can
                                        # get a chance to put our stub jobs in
                                        # the stream first.
my $gBuffered = "";                     # The current buffered anno fragment.

#------------------------------------------------------------------------------
# ScopeJobId
#
#       Given a job identifier from an annotation file, prefix it with the
#       buildId so that it is unique across all the annotation files that
#       are being concatenated.
#------------------------------------------------------------------------------

sub ScopeJobId {
    my($jobId) = @_;
    my $prefix = $gCurrentBuildId . "_";
    $jobId =~ s/J/J$prefix/;
    return $jobId;
}

#------------------------------------------------------------------------------
# EmitFakeJob
#
#       Write a fake job to the output stream with appropriate waitingJob
#       linkage, such that the original independent builds are represented as
#       a series of serialized steps.
#------------------------------------------------------------------------------

sub EmitFakeJob {
    my($type,$emitWaiters,$extraWaiter) = @_;
    print  "<job type=\"" . $type . "\"";
    printf " id=\"J%08d\"", $gFakeJobId;
    if ($type eq "follow") {
        printf " partof=\"J%08d\"", $gFakeJobId - 1;
    } else {
        printf " name=\"fake_%d\"", $gFakeJobId;
    }
    print  " node=\"LocalNode\">\n";
    print  "<timing invoked=\"" . $gElapsed . "\"";
    print  " completed=\"" . $gElapsed . "\"";
    print  " node=\"LocalNode\"/>\n";
    if ($gFakeJobId != 0) {
        print  "<waitingJobs idList=\"J00000000";
        if ($emitWaiters) {
            printf " J%08d", $gFakeJobId + 1;
        }
        if ($extraWaiter ne "") {
            print  " " . $extraWaiter;
        }
        print  "\"/>\n";
    }
    print  "</job>\n";
    $gFakeJobId++;
}

#------------------------------------------------------------------------------
# Emit
#
#       Output a bit of annotation, either to the internal buffer if we are
#       currently buffering, or directly to stdout if not.
#------------------------------------------------------------------------------

sub Emit {
    my($text) = @_;
    if ($gBuffering) {
        $gBuffered .= $text;
    } else {
        print $text;
    }
}

#------------------------------------------------------------------------------
# AnnoElementStart
#
#       Process an open tag extracted from annotation.  Many of these just get
#       echoed to stdout as is, but some of them we do some special swizzling
#       on in order to allow us to present several distinct anno files as a
#       single cohesive build.  In particular, we fixup timing information for
#       jobs in the second and subsequent anno files, such that the start and
#       end time is relative to the start of the sequence of builds, rather
#       than simply relative to the start of the build that owns the job; and
#       we scope all job Id's by the id of the build that owns the job.
#------------------------------------------------------------------------------

sub AnnoElementStart {
    my($parser,$element,%attrs) = @_;
      if ($element eq "build") {
          $gBuildCount++;
          $gEnabled = 1;
          if ($gBuildCount == 1) {
              Emit "<build";
              while (my($key,$value) = each(%attrs)) {
                  Emit " $key=\"" . EscapeSpecials($value) . "\"";
              }
              Emit ">";
          }
          $gBuffering = 1;

          # Save the buildId -- we'll use it to ensure jobId's are unique
          # across the entire combined anno file.

          $gCurrentBuildId = $attrs{"id"};

      } elsif ($element eq "environment" || $element eq "properties") {
          # Only keep the first <environment> and <properties> blocks.

          if ($gBuildCount > 1) {
              $gEnabled = 0;
          } else {
              Emit "<" . $element . ">";
          }
      } elsif ($element eq "metrics") {
          # We don't bother keeping any of the metrics blocks, although if
          # we really wanted to we probably could accumulate all of them and
          # emit a synthesized set of metrics at the end of the combined
          # annotation.

          $gEnabled = 0;
      } elsif ($element eq "timing") {
          # Swizzle the timing values so that they are relative to the start
          # of the first build in the set of builds, rather than relative to
          # the start of the current build.

          Emit "<timing";

          my $invoked   = $attrs{"invoked"}   + $gElapsed;
          my $completed = $attrs{"completed"} + $gElapsed;
          $gMax = $completed;
          Emit " invoked=\"$invoked\" completed=\"$completed\"";
          delete($attrs{"invoked"});
          delete($attrs{"completed"});
          while (my($key, $value) = each(%attrs)) {
              Emit " $key=\"" . EscapeSpecials($value) . "\"";
          }
          Emit "/>";
      } else {
          # Everything else we just echo back to stdout, after swizzling
          # jobId's that appear in attribute values.

          if ($gEnabled == 0) {
              return;
          }

          if ($element eq "job" && $gBuffering) {
              $gBuffering = 0;
              if ($gBuildCount > 1) {
                  EmitFakeJob("follow", 1, "");
              } else {
                  Emit "<make>\n";
              }

              EmitFakeJob("rule", 1, ScopeJobId($attrs{"id"}));
              Emit $gBuffered;
              $gBuffered = "";
          }

          Emit "<$element";
          if (exists $attrs{"idList"}) {
              my $separator = "";
              Emit " idList=\"";
              foreach $jobId (split(/ /,$attrs{"idList"})) {
                  Emit $separator . ScopeJobId($jobId);
                  $separator = " ";
              }
              delete($attrs{"idList"});
              Emit "\"";
          }
          foreach $idref ("id", "neededby", "rerunby", "writejob", "partof") {
              if (exists $attrs{$idref}) {
                  $attrs{$idref} = ScopeJobId($attrs{$idref});
              }
          }
          while (my($key, $value) = each(%attrs)) {
              Emit " $key=\"" . EscapeSpecials($value) . "\"";
          }
          Emit ">";
      }
}

#------------------------------------------------------------------------------
# AnnoElementEnd
#
#       Handle wrapup for a tag extracted from annotation.
#------------------------------------------------------------------------------

sub AnnoElementEnd {
    my($parser, $element, %attrs) = @_;
    if ($element eq "build") {
        # Do nothing.
    } elsif ($element eq "environment" 
               || $element eq "properties"
               || $element eq "metrics") {
        if ($gEnabled) {
            Emit "</$element>\n";
            my $oldBuffering = $gBuffering;
            $gBuffering = 0;
            Emit $gBuffered;
            $gBuffered = "";
            $gBuffering = $oldBuffering;
        }
        $gEnabled = 1;
    } elsif ($element eq "timing") {
        # Do nothing for now.
    } else {
        if ($gEnabled) {
            Emit "</$element>";
        }
    }
}

#------------------------------------------------------------------------------
# AnnoCDATA
#
#       Copy CDATA from annotation to stdout.
#------------------------------------------------------------------------------

sub AnnoCDATA {
    my($parser,$data) = @_;
    if ($gEnabled != 0) {
        Emit EscapeSpecials($data);
    }
}

#------------------------------------------------------------------------------
# EscapeSpecials
#
#       Dumb function that re-escapes special characters so that we
#       don't accidentally emit invalid XML in the combined anno file.
#------------------------------------------------------------------------------

sub EscapeSpecials {
    my($data) = @_;
    $data =~ s/&/&amp;/sg;
    $data =~ s/</&lt;/sg;
    $data =~ s/>/&gt;/sg;
    $data =~ s/\"/&quot;/sg;
    return($data);
}

# Create and initialize an XML parser.

my $parser = new XML::Parser(Style=>Stream);
$parser->setHandlers (
                      Start   => \&AnnoElementStart,
                      End     => \&AnnoElementEnd,
                      Char    => \&AnnoCDATA);

# Emit the XML boilerplate for the combined anno file.

Emit "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
Emit "<!DOCTYPE build SYSTEM \"build.dtd\">\n";

# Copy each specified anno file to stdout, tweaking those attributes that
# need adjusting to ensure a logically consistent combined anno file.

foreach $file (@ARGV) {
    $parser->parsefile($file);
    $gElapsed = $gMax;
}

# Finish up:  
#     o  Output the fake follow job that comes after the last build in the
#        series.
#     o  Output a fake end job for the logical build we just synthesized.
#     o  Output the closing </make> tag for the logical build.
#     o  Output the closing </build> tag for the logical build.

EmitFakeJob("follow", 0, "");
$gFakeJobId = 0;
EmitFakeJob("end", 0, "");
Emit "</make>\n";
Emit "</build>";

