#!/bin/sh

exec "$COMMANDER_HOME/bin/ec-perl" -x "$0" "${@}"
#exec "perl" -x "$0" "${@}"

#!perl

use XML::Parser;
use URI::Escape;
use IPC::Open2;
use File::Temp qw/ tempfile /;
use CGI;
use ElectricCommander;

###########################################################################

my %states = ();                ;# Map of state names to state id's.
my $stateId;
my $stateName;
my $startable;
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

my($dotOut, $dotIn);

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
                . uri_escape("/commander/pages/Flowviz-1.0/flowviz"
                             . "?type=definition"
                             . "&name=" . $workflowName 
                             . "&proj=" . $projectName
                             . "&s=Flowviz")
                . "\" target=\"_top\"";
        }

        print $dotIn "$stateId [label=\"$stateName\" color=\"$color\" fontcolor=\"$fontcolor\"$url fontsize=\"10\"]\n";
        if ($startable) {
            print $dotIn "start -> $stateId\n";
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
                print $dotIn "$tgt [color=\"black\" fontcolor=\"black\"]\n";

                if ($trigger eq "manual") {
                    $url = " URL=\"/commander/pages/Flowviz-1.0/flowviz"
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
                . uri_escape("/commander/pages/Flowviz-1.0/flowviz"
                             . "?type=definition"
                             . "&name=" . $workflowName 
                             . "&proj=" . $projectName
                             . "&s=Flowviz")
                . "\" target=\"_top\"";
        }
        print $dotIn "$src -> $tgt [label=\"$transName\"$url color=\"$color\" fontcolor=\"$fontcolor\" fontsize=\"10\"]\n";
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
<form action="/commander/pages/Flowviz-1.0/flowviz" method="GET">
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
<input type="hidden" name="s" value="Flowviz"/>
</form>
FORM
    ;
}

sub ExtractXML {
    my($response,$msg) = @_;
    my @resultSet = $response->findnodes("/responses/response");
    if (!defined @resultSet) {
        EmitForm($msg, 1);
        exit;
    }
    return $resultSet[0]->toString;
}

# Check the CGI parameters.  If "name" is specified, then we should parse a
# workflow and render the result.  Otherwise, we should just put up a simple
# form that allows the user to specify the workflow name.

my $q = new CGI;
my %params = $q->Vars;

if (defined $params{"type"}) {
    $type = $params{"type"};
}

if (defined $params{"name"}) {
    # Create the XML parser and parse the specified file.

    $workflowName = $params{"name"};
    $projectName  = $params{"proj"};

    my $parser = new XML::Parser(Style=>Stream);
    $parser->setHandlers(
        Start       => \&ElementStart,
        End         => \&WorkflowElementEnd,
        Char        => \&CDATA);

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

    my $workflowXML;
    my $statesXML;
    my $transitionsXML;

    if ($type eq "workflow") {
        # Get the details on the workflow so we can determine its active state.

        my $response = $commander->getWorkflow({
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

    my $dotpid = open2($dotOut, $dotIn, "dot -Tsvg");

    print $dotIn "digraph workflow {\n";
    print $dotIn "rankdir=TB\n";
    print $dotIn "outputorder=edgesfirst\n";
    print $dotIn "node [shape=box style=filled fillcolor=\"#ffffffe0\"]\n";
    print $dotIn "edge [dir=front]\n";
    print $dotIn "start [shape=circle style=filled fillcolor=\"#a5a8bdff\" label=\"start\" fontsize=\"10\"]\n";

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
    print $dotIn "}\n";
    close $dotIn;

    my($svg,$filename) = tempfile("/tmp/flowviz_XXXXXX");
    print $svg "Content-type: image/svg+xml\n\n";
    while (<$dotOut>) {
        print $svg "$_";
    }
    close $svg;
    close $dotOut;
    waitpid($dotpid, 0);

    # Unfortunately due to the way commander handles plugins, we can't just
    # straight-up return the SVG.  We have to return an HTML fragment that has
    # an <object> tag that references the SVG content.  Of course we don't want
    # to leave files littering the filesystem, so we use a custom CGI that
    # handles streaming the file content and then deletes it.

    my $counter = $filename;
    $counter =~ s/\/tmp\/flowviz_//;

    print "Content-type: text/html\n\n";
    print "Displaying workflow";
    if ($type eq "definition") {
        print " definition";
    }
    print "<br>";
    print "<a href=\"/commander/link/projectDetails/projects/" 
        . uri_escape($projectName) . "\">Project: " . $projectName . "</a><br>";
    if ($type eq "definition") {
        # Link to the definition details page.

        print "<a href=\"/commander/link/workflowDefinitionDetails/projects/"
            . uri_escape($projectName) . "/workflowDefinitions/"
            . uri_escape($workflowName) . "?s=Flowviz&redirectTo="
            . uri_escape("/commander/pages/Flowviz-1.0/flowviz"
                         . "?type=definition"
                         . "&name=" . $workflowName 
                         . "&proj=" . $projectName
                         . "&s=Flowviz")
            . "\">$workflowName</a><br>\n";

        # Include a link for creating new steps in the workflow definition.

        print "<a href=\"/commander/link/editStateDefinition/projects/"
            . uri_escape($projectName) . "/workflowDefinitions/"
            . uri_escape($workflowName) . "?redirectTo=" 
            . uri_escape("/commander/pages/Flowviz-1.0/flowviz"
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
            . uri_escape("/commander/pages/Flowviz-1.0/flowviz"
                         . "?type=workflow"
                         . "&name=" . $workflowName 
                         . "&proj=" . $projectName
                         . "&s=Flowviz")
            . "\">$workflowName</a><br>\n";
    }

    # Use an <object> tag to embed the actual rendered result.

    print "<object height=\"60%\" width=\"60%\" type=\"image/svg+xml\" ";
    print "data=\"/cgi-bin/flowviz.cgi?n=" . uri_escape($counter) . "\">\n";
    print "</object>\n";

} else {
    EmitForm("Specify the workflow or workflow definition to display:", 0);
}
