#!/bin/sh
# Copyright (c) 2011 Eric Melski
# Copyright (c) 2011 Electric Cloud, Inc.
# All rights reserved.
#
# flowviz is free software.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
# * Neither the name of Eric Melski nor Electric Cloud nor the names of its
# employees may be used to endorse or promote products derived from this
# software without specific prior written permission.
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

exec "$COMMANDER_HOME/bin/ec-perl" -x "$0" "${@}"

#!perl

use XML::Parser;
use URI::Escape;
use File::Temp qw/ tempfile /;
use CGI;
use ElectricCommander;

###########################################################################

my $VERSION = "2.0.0";

my %states = ();                ;# Map of state names to state id's.
my $stateId;
my $stateName;
my $startable;
my $startingStart = "";
my $value = "";                 ;# Text contents of the current tag.

my $sourceState;
my $targetState;
my $transName;

my $activeState = "";
my $workflowName = "";
my $projectName = "";

my $projectName = "";
my $workflowDef = "";
my $type = "workflow";
my $dotfile;

###########################################################################
# XML processing handlers.

sub ElementStart {
    my($parser,$element,%attrs) = @_;
    $value = "";
}

sub WorkflowElementEnd {
    my($parser,$element,%attrs) = @_;
    if ($element eq "workflowDefinitionName") {
        $workflowDef = $value;
    } elsif ($element eq "activeState") {
        $activeState = $value;
    } elsif ($element eq "startingState") {
        $startingState = $value;
    }
}

sub StatesElementEnd {
    my($parser,$element,%attrs) = @_;
    if ($element eq "stateDefinitionId" || $element eq "stateId") {
        $stateId = $value;
    } elsif ($element eq "stateDefinitionName" || $element eq "stateName") {
        $stateName = $value;
    } elsif ($element eq "startable") {
        $startable = $value;
    } elsif ($element eq "stateDefinition" || $element eq "state") {
        $states{ $stateName } = $stateId;

        my $color = "#ddddddff";
        my $fontcolor = "gray";
        if ($stateName eq $activeState) {
            $color = "red";
            $fontcolor = "black";
        }

        my $url;
        if ($type eq "workflow") {
            $url = " URL=\"/commander/link/stateDetails/projects/"
                . uri_escape($projectName) . "/workflows/"
                . uri_escape($workflowName) . "/states/"
                . uri_escape($stateName) . "\" target=\"_top\"";
        } else {
            $color = "black";
            $fontcolor = "black";
            $url = " URL=\"/commander/link/stateDefinitionDetails/projects/"
                . uri_escape($projectName) . "/workflowDefinitions/"
                . uri_escape($workflowName) . "/stateDefinitions/"
                . uri_escape($stateName) . "?s=Flowviz&redirectTo="
                . uri_escape("/commander/pages/Flowviz-" . $VERSION . "/flowviz"
                             . "?type=definition"
                             . "&name=" . $workflowName 
                             . "&proj=" . $projectName
                             . "&s=Flowviz")
                . "\" target=\"_top\"";
        }

        print $dotfile "$stateId [label=\"$stateName\" color=\"$color\" fontcolor=\"$fontcolor\"$url fontsize=\"10\"]\n";
        if ($startable || $startingState eq $stateName) {
            print $dotfile "start -> $stateId\n";
        }
    }
}

sub TransitionsElementEnd {
    my($parser,$el,%attrs) = @_;
    if ($el eq "transitionDefinitionName" || $el eq "transitionName") {
        $transName = $value;
    } elsif ($el eq "sourceState") {
        $sourceState = $value;
    } elsif ($el eq "targetState") {
        $targetState = $value;
    } elsif ($el eq "trigger") {
        $trigger = $value;
    } elsif ($el eq "transitionDefinition" || $el eq "transition") {
        my $src = $states{ $sourceState };
        my $tgt = $states{ $targetState };
        my $color = "#ddddddff";
        my $fontcolor = "gray";
        my $url = "";
        if ($type eq "workflow") {
            if ($sourceState eq $activeState) {
                $color = "red";
                $fontcolor = "black";
                print $dotfile "$tgt [color=\"black\" fontcolor=\"black\"]\n";

                if ($trigger eq "manual") {
                    # NOTE: if redirectTo worked on the transitionWorkflow URL,
                    # then we could use the following URL here and seamlessly 
                    # handle transitions that require parameters:
                    #
                    # $url = " URL=\""
                    #     . "/commander/link/transitionWorkflow/projects/"
                    #     . uri_escape($projectName) . "/workflows/"
                    #     . uri_escape($workflowName) . "/transitions/"
                    #     . uri_escape($transName) . "?s=Flowviz&redirectTo="
                    #     . uri_escape("/commander/pages"
                    #          . "/Flowviz-" . $VERSION . "/flowviz"
                    #          . "?name=" . $workflowName 
                    #          . "&proj=" . $projectName
                    #          . "&s=Flowviz")
                    #     . "\" target=\"_top\"";

                    $url = " URL=\"/commander/pages"
                        . "/Flowviz-" . $VERSION . "/flowviz"
                        . "?transition=" . uri_escape($transName)
                        . "&name=" . uri_escape($workflowName)
                        . "&proj=" . uri_escape($projectName)
                        . "&s=Flowviz\" target=\"_top\"";
                }
            }
        } else {
            $color = "black";
            $fontcolor = "black";

            $url = " URL=\"/commander/link/editTransitionDefinition"
                . "/projects/" . uri_escape($projectName)
                . "/workflowDefinitions/" . uri_escape($workflowName)
                . "/stateDefinitions/" . uri_escape($sourceState)
                . "/transitionDefinitions/" . uri_escape($transName)
                . "?redirectTo=" 
                . uri_escape("/commander/pages/Flowviz-" . $VERSION . "/flowviz"
                             . "?type=definition"
                             . "&name=" . $workflowName 
                             . "&proj=" . $projectName
                             . "&s=Flowviz")
                . "\" target=\"_top\"";
        }
        print $dotfile "$src -> $tgt [label=\"$transName\"$url color=\"$color\" fontcolor=\"$fontcolor\" fontsize=\"10\"]\n";
    }
}

sub CDATA {
    my($parser,$data) = @_;
    $value .= $data;
}

# EmitForm
#
#     Print a simple form that will allow the user to specify the workflow to
#     view.

sub EmitForm {
    my($msg,$err) = @_;

    print "Content-type: text/html\n\n";
    if ($msg ne "") {
        my $color = "";
        if ($err != 0) {
            $color = " color=\"red\"";
        }
        print "<font$color>$msg</font>\n";
    }
    print <<FORM
<form action="/commander/pages/Flowviz-$VERSION/flowviz" method="GET">
<table>
<tr><td align="right">Project:</td><td><input name="proj" size="40"/></td></tr>
<tr><td align="right">Name:</td><td><input name="name" size="40"/></td></tr>
<tr>
  <td colspan="2">
    <input name="type" type="radio" value="workflow" checked/> Workflow
    <input name="type" type="radio" value="definition" /> Workflow definition
  </td>
</tr>
<tr><td colspan="2"><input type="submit" value="Show workflow"/></td></tr>
</table>
<input type="hidden" name="mode" value="top"/>
<input type="hidden" name="s" value="Flowviz"/>
</form>
FORM
    ;
}

# ExtractXML
#
#       Utility function to help with getting XML out of a Commander response
#       string.

sub ExtractXML {
    my($response,$msg) = @_;
    my @resultSet = $response->findnodes("/responses/response");
    if (!defined @resultSet) {
        EmitForm($msg, 1);
        exit;
    }
    return $resultSet[0]->toString;
}

# Check the CGI parameters.  If "name" is not specified, then put up a simple
# form that allows the user to specify the workflow name.

my $q = new CGI;
my %params = $q->Vars;

if (!defined $params{"name"}) {
    EmitForm("Specify the workflow or workflow definition to display:", 0);
    exit
}

# "name" parameter was present, so we're either generating the toplevel
# response (header and container for the graph), or rendering the graph
# itself.

# Check if "type" was specified, and use that value if so -- this is how
# we decided whether to render a workflow definition or an active workflow.

if (defined $params{"type"}) {
    $type = $params{"type"};
}
$workflowName = $params{"name"};
$projectName  = $params{"proj"};

# Connect to the commander server and get the workflow information.

my $commander = new ElectricCommander({
    server => $q->server_name(),
});
$commander->abortOnError(0);

if (defined $params{"transition"}) {
    # First transition the workflow.

    $commander->transitionWorkflow({
        projectName => $params{"proj"},
        workflowName => $workflowName,
        transitionName => $params{"transition"}
    });
}

# If the CGI was invoked in 'top' mode, just output the boilerplate and
# <object> tag, then return.  The cgi will be invoked a second time to
# actually render the graph.

if ($params{"mode"} ne "render") {
    print "Content-type: text/html\n\n";
    print "Displaying workflow";
    if ($type eq "definition") {
        print " definition";
    }
    print "<br>";
    print "<a href=\"/commander/link/projectDetails/projects/" 
        . uri_escape($projectName) . "\">Project: " 
        . $projectName . "</a><br>";
    if ($type eq "definition") {
        # Link to the definition details page.

        print 
            "<a href=\"/commander/link/workflowDefinitionDetails/projects/"
            . uri_escape($projectName) . "/workflowDefinitions/"
            . uri_escape($workflowName) . "?s=Flowviz&redirectTo="
            . uri_escape("/commander/pages/Flowviz-" . $VERSION . "/flowviz"
                         . "?type=definition"
                         . "&name=" . $workflowName 
                         . "&proj=" . $projectName
                         . "&s=Flowviz")
            . "\">$workflowName</a><br>\n";

        # Include a link for creating new steps in the workflow definition.

        print "<a href=\"/commander/link/editStateDefinition/projects/"
            . uri_escape($projectName) . "/workflowDefinitions/"
            . uri_escape($workflowName) . "?redirectTo=" 
            . uri_escape("/commander/pages/Flowviz-" . $VERSION . "/flowviz"
                         . "?type=definition"
                         . "&name=" . $workflowName 
                         . "&proj=" . $projectName
                         . "&s=Flowviz")
            . "\">Create State Definition</a><br>\n";
    } else {
        # Link to the workflow details page.
            
        print "<a href=\"/commander/link/workflowDetails/projects/"
            . uri_escape($projectName) . "/workflows/"
            . uri_escape($workflowName) . "?s=Flowviz&redirectTo="
            . uri_escape("/commander/pages/Flowviz-" . $VERSION . "/flowviz"
                         . "?type=workflow"
                         . "&name=" . $workflowName 
                         . "&proj=" . $projectName
                         . "&s=Flowviz")
            . "\">$workflowName</a><br>\n";
    }

    # Use an <object> tag to embed the actual rendered result.

    my $dim = " width=\"60%\" height=\"60%\"";
    if ($q->user_agent() =~ m/Firefox/) {
        $dim = "";
    }
    print "<object$dim type=\"image/svg+xml\" ";
    print "data=\"../../plugins/Flowviz-" . $VERSION . "/cgi-bin/flowviz.cgi"
        . "?name=" . uri_escape($workflowName)
        . "&proj=" . uri_escape($projectName)
        . "&mode=render"
        . "&type=" . uri_escape($type)
        . "\">\n";
    print "</object>\n";
    exit
}

my $parser = new XML::Parser(Style=>Stream);
$parser->setHandlers(
    Start       => \&ElementStart,
    End         => \&WorkflowElementEnd,
    Char        => \&CDATA);

my $workflowXML;
my $statesXML;
my $transitionsXML;
my $response;

if ($type eq "workflow") {
    # Get the details on the workflow so we can determine its active state.

    $response = $commander->getWorkflow({
        projectName => $projectName,
        workflowName => $workflowName
    });
    $workflowXML = ExtractXML($response, "Invalid workflow");
    $parser->parse($workflowXML);

    # Get the workflow states.

    $response = $commander->getStates({
        projectName => $projectName,
        workflowName => $workflowName
    });
    $statesXML = ExtractXML($response, "Invalid workflow (states)");

    # Get the workflow transitions.

    $response = $commander->getTransitions({
        projectName => $projectName,
        workflowName => $workflowName
    });
    $transitionsXML = ExtractXML($response, "Invalid workflow (trans)");
} else {
    # Get workflow definition states.

    $response = $commander->getStateDefinitions({
        projectName => $projectName,
        workflowDefinitionName => $workflowName
    });
    $statesXML = ExtractXML($response, "Invalid workflow def (states)");

    # Get workflow definition transitions.

    $response = $commander->getTransitionDefinitions({
        projectName => $projectName,
        workflowDefinitionName => $workflowName
    });
    $transitionsXML = ExtractXML($response,"Invalid workflow def (trans)");
}

# Now we have all the states and transitions in XML form.  As we parse the
# XML, we'll emit the graph description in DOT format to a temporary file.

my(undef, $dotFilename) = File::Temp::tempfile("flowviz_XXXXXX", 
                                               OPEN => 0,
                                               DIR => File::Spec->tmpdir);
open($dotfile, '>', $dotFilename) or die $!;
print $dotfile "digraph workflow {\n";
print $dotfile "rankdir=TB\n";
print $dotfile "outputorder=edgesfirst\n";
print $dotfile "node [shape=box style=filled fillcolor=\"#ffffffe0\"]\n";
print $dotfile "edge [dir=front]\n";
print $dotfile "start [shape=circle style=filled fillcolor=\"#a5a8bdff\" label=\"start\" fontsize=\"10\"]\n";

$parser->setHandlers(
    Start       => \&ElementStart,
    End         => \&StatesElementEnd,
    Char        => \&CDATA);
$parser->parse($statesXML);

$parser->setHandlers(
    Start       => \&ElementStart,
    End         => \&TransitionsElementEnd,
    Char        => \&CDATA);
$parser->parse($transitionsXML);
print $dotfile "}\n";
close $dotfile;

# Now we have a description of the graph, in DOT format.  Use "dot" to
# transform it into SVG, and copy the result to our own stdout for rendering.

my $dot = "dot";
$response = $commander->getProperty({
    propertyName => "/projects/Flowviz-" . $VERSION . "/dotPath"
});
my @results = $response->findnodes("/responses/response/property/value/text()");
if (defined $results[0]) {
    my $tmp = $results[0]->toString;
    if ($tmp != "") {
        $dot = $tmp;
    }
}

open(SVG, "$dot -Tsvg $dotFilename |");
print "Content-type: image/svg+xml\n\n";
my $inNode = 0;
my $lx = 0;
my $ly = 0;
my $rx = 0;
my $ry = 0;
while (<SVG>) {
    # Watch for lines marking the start of a graph node.

    my $line = $_;
    print $line;

    if ($type eq "definition") {
        if ($line =~ m/class="node"/) {
            $inNode = 1;
        } elsif ($inNode) {
            if ($line =~ m/<polygon .* points=\"[^,]+,[^,]+ ([^,]+),([^,]+) [^,]+,[^,]+ ([^,]+),([^,]+)/) {
                $lx = $1;
                $ly = $2;
                $rx = $3;
                $ry = $4;
            } elsif ($line =~ m/<text.*>(.*)<\/text>/) {
                $stateName = $1;
            } elsif ($line =~ m/<\/a>/) {
                # Inject a link into the SVG for the "add transition" button.

                my $url = "/commander/link/editTransitionDefinition/projects/"
                    . uri_escape($projectName) . "/workflowDefinitions/"
                    . uri_escape($workflowName) . "/stateDefinitions/"
                    . uri_escape($stateName) . "?s=Flowviz&amp;redirectTo="
                    . uri_escape("/commander/pages/Flowviz-" . $VERSION 
                                 . "/flowviz"
                                 . "?type=definition"
                                 . "&name=" . $workflowName 
                                 . "&proj=" . $projectName
                                 . "&s=Flowviz");

                print "<a xlink:href=\"$url\" target=\"_top\"><text text-anchor=\"end\" x=\"$rx\" y=\"$ry\" style=\"font-family:Times New Roman;font-size:10.00;\">+</text></a>";

                $inNode = 0;
            }
        }
    }
}
close SVG;

# Cleanup the tempfile containing the graph description.  Too bad perl can't
# handle bidirectional process pipes on Windows, otherwise we wouldn't need
# a temp file at all.

unlink($dotFilename);
