#!/bin/sh

exec "perl" -x "$0" "${@}"

#!perl

use CGI;

my $q = new CGI;
my %params = $q->Vars;
if (defined $params{"n"}) {
    my $ext = $params{"n"};
    if ($ext =~ m/\//) {
        print "Content-type: text/html\n\nMalformed filename\n";
        exit;
    }
    my $filename = "/tmp/flowviz_" . $params{"n"};
    open F, $filename or die $!;
    while (<F>) {
        print $_;
    }
    close(F);
    unlink($filename);
} else {
    print "Content-type: text/html\n\nMalformed request\n";
    exit;
}

