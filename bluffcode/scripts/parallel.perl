#!/usr/bin/perl
use Carp;
use strict;
use IPC::Open3;
use Time::HiRes qw/ time sleep /;
use vars qw( $debug );
$debug = 1;

if (not defined($ARGV[4])) { 
  print "Usage: parallel.perl <scratch dir> <timelimit (ms)> <game> <CPUs> <games per match>\n";
  exit;
}

my $scratchdir = $ARGV[0];
my $tl = $ARGV[1];
my $game = $ARGV[2];
my $CPUS = $ARGV[3];
my $gamespermatch = $ARGV[4];

if (not -d "$scratchdir") { 
  print "Need a subdirectory called scratch. Running from base directory recommended.\n";
  exit;
}

# for estimating time remaining
my $ttljobs = 0;
my $curjob = 0;
my $starttime = 0;

# returns a nicely displayed time
sub prettytime
{
  my $seconds = shift;

  my $hours = int($seconds / 3600.0);
  my $remainder = ($seconds - $hours*3600);

  my $minutes = int($remainder / 60.0);
  $remainder = ($remainder - $minutes*60);

  if ($hours == 0 and $minutes == 0) {
    return sprintf("00:%02d", int($seconds));
  }
  elsif ($hours == 0) {
    return sprintf("%02d:%02d", $minutes, int($remainder));
  }
  else {
    return sprintf("%02d:%02d:%02d", $hours, $minutes, int($remainder));
  }
}

# The first parameter is how many jobs to run at once, the remaining are
# the jobs.  Jobs may be a string, or an anonymous array of the cmd and
# args.
#
# All output from children go to your STDERR and STDOUT.  They get no
# input.  It prints fairly uninformative errors for commands with
# problems, and returns a hash of problems.
#
# The jobs SHOULD NOT depend on each other!
sub run_parallel {
  my $job_count = shift;
  unless (0 < $job_count) {
    confess("run_parallel called without a positive parallel job count!");
  }
  my @to_start = @_;
  my %running;
  my %errors;
  my $is_running = 0;
  while (@to_start or %running) {
    if (@to_start and ($is_running < $job_count)) {
      # Launch a job
      my $job = shift @to_start;
      unless (ref($job)) {
        $job = [$job];
      }
      $curjob++;
      my $nowtime = time;
      my $jobspersec = $curjob / ($nowtime - $starttime);
      my $esttimeremaining = ($ttljobs - $curjob) / $jobspersec;
      print "Launching '$job->[0]'\n" if $debug;
      my $ptr = prettytime($esttimeremaining);
      print "  job: #$curjob / $ttljobs (~$ptr seconds remaining)\n" if $debug;
      local *NULL;
      my $null_file = ($^O =~ /Win/) ? 'nul': '/dev/null';   
      open (NULL, $null_file) or confess("Cannot read from $null_file:$!");
      my $proc_id = open3("<&NULL", ">&STDOUT", ">&STDERR", @$job);
      $running{$proc_id} = $job;
      ++$is_running;
    }
    else {
      # collect a job
      my $proc_id = wait();
      if (! exists $running{$proc_id}) {
        confess("Reaped unknown process $proc_id!");
      }
      elsif ($?) {
        # Oops
        my $job = $running{$proc_id};
        my ($cmd, @args) = @$job;
        my $err = "Running '$cmd' gave return code '$?'";
        if (@args) {
          $err .= join "\n\t", "\nAdditional args:", @args;
        }
        print STDERR $err, "\n";
        $errors{$proc_id} = $err;
      }
      print "Reaped '$running{$proc_id}->[0]'\n" if $debug;
      delete $running{$proc_id};
      --$is_running;
    }
  }
  return %errors;
}

sub get_cmd
{
  #push(@matchups, "sim,1,2,1.0,1,0.0"); 

  my $alg1 = shift;
  my $alg2 = shift;
  my $seed = shift;

  my $simtype = shift;
  my $timelimit = shift;
  my $oosvariant = shift;
  my $delta = shift;

  my $cmd = "";
  my $arglist = "";

  if ($simtype eq "sim") {
    $arglist = "single 1 $timelimit $oosvariant $delta"; 
  }
  else {
    $arglist = "agg 500 $timelimit $oosvariant $delta"; 
  }

  if ($alg1 eq "3") {
    $cmd = "./sim scratch/iss.0001Nash.dat scratch/iss.initial.dat $alg1 $alg2 $arglist";
  }
  elsif ($alg2 eq "3") { 
    $cmd = "./sim scratch/iss.initial.dat scratch/iss.0001Nash.dat $alg1 $alg2 $arglist";
  }
  else { 
    $cmd = "./sim scratch/iss.initial.dat scratch/iss.initial.dat $alg1 $alg2 $arglist";
  }
  
  return $cmd;
}

my @jobs = (); 

my @matchups = (); 

# sim or exp
# player type A
# player type B
# time limit
# oos variant, 1 = IST, 2 = PST
# oos delta

# player types: 1 = mcts, 2 = oos, 3 = 0.001 Nash, 4 = random

#push(@matchups, "sim,1,4,25.0,2,0.0"); 
#push(@matchups, "sim,1,3,25.0,2,0.0"); 

#push(@matchups, "sim,4,2,25.0,2,0.0"); 
#push(@matchups, "sim,4,2,25.0,2,0.1"); 
#push(@matchups, "sim,4,2,25.0,2,0.5"); 
#push(@matchups, "sim,4,2,25.0,2,0.65"); 
#push(@matchups, "sim,4,2,25.0,2,0.8"); 
#push(@matchups, "sim,4,2,25.0,2,0.95"); 
#push(@matchups, "sim,4,2,25.0,2,1.0"); 

#push(@matchups, "sim,3,2,25.0,2,0.0"); 
#push(@matchups, "sim,3,2,25.0,2,0.1"); 
#push(@matchups, "sim,3,2,25.0,2,0.5"); 
#push(@matchups, "sim,3,2,25.0,2,0.65"); 
#push(@matchups, "sim,3,2,25.0,2,0.8"); 
#push(@matchups, "sim,3,2,25.0,2,0.95"); 
#push(@matchups, "sim,3,2,25.0,2,1.0"); 


#MCRNR
#my @times = ( "1.0", "5.0" );
#my @oosv = ( "1", "2" ); 
#my @deltas = ("0.00", "0.10", "0.50", "0.65", "0.80", "0.90", "0.95", "0.99", "1.00"); 
#push(@matchups, "sim,1,2,5.0,1,0.00"); 
#for (my $i = 0; $i < scalar(@times); $i++) { 
#  for (my $j = 0; $j < scalar(@oosv); $j++) { 
#    for (my $k = 0; $k < scalar(@deltas); $k++) { 
#      my $time = $times[$i];
#      my $ov = $oosv[$j]; 
#      my $delta = $deltas[$k];
#      my $matchup = "sim,1,2,$time,$ov,$delta";
#      push(@matchups, "$matchup"); 
#    }
#  }
#}

#Expl
my @times = ( "1.0", "5.0" );
my @oosv = ( "1", "2" ); 
my @deltas = ("0.00", "0.10", "0.50", "0.65", "0.80", "0.90", "0.95", "0.99", "1.00"); 
push(@matchups, "sim,1,2,5.0,1,0.00"); 
for (my $i = 0; $i < scalar(@times); $i++) { 
  for (my $j = 0; $j < scalar(@oosv); $j++) { 
    for (my $k = 0; $k < scalar(@deltas); $k++) { 
      my $time = $times[$i];
      my $ov = $oosv[$j]; 
      my $delta = $deltas[$k];
      my $matchup = "agg,2,2,$time,$ov,$delta";
      push(@matchups, "$matchup"); 
    }
  }
}




print "queuing jobs... \n";

for (my $i = 0; $i < scalar(@matchups); $i++)
{
  my @parts = split(',', $matchups[$i]);

  for (my $run = 1; $run <= $gamespermatch; $run++)
  {
    # a1 as player1 a2 as player 2
    my $seed = int(rand(100000000)) + 1;
    chomp($seed);

    my $simtype = $parts[0];
    my $timelimit = $parts[3];
    my $oosvariant = $parts[4];
    my $delta = $parts[5];
    
    my $alg1 = $parts[1];
    my $alg2 = $parts[2];
    
    my $runname = "$alg1-$alg2-$simtype-$timelimit-$oosvariant-$delta-$run";

    my $cmd = get_cmd($alg1, $alg2, $seed, $simtype, $timelimit, $oosvariant, $delta); 
    my $fullcmd = "$cmd >$scratchdir/$runname.log 2>&1";
    
    push(@jobs, $fullcmd); 
    
    #print "queueing $fullcmd\n";
    if ($simtype eq "sim") { 
      # now swap seats
      $alg1 = $parts[2];
      $alg2 = $parts[1];

      $runname = "$alg1-$alg2-$simtype-$timelimit-$oosvariant-$delta-$run";

      $cmd = get_cmd($alg1, $alg2, $seed, $simtype, $timelimit, $oosvariant, $delta); 
      $fullcmd = "$cmd >$scratchdir/$runname.log 2>&1";
      
      #print "queueing $fullcmd\n";
      push(@jobs, $fullcmd); 
    }
  }
}

sub fisher_yates_shuffle
{
    my $array = shift;
    my $i = scalar(@$array);
    while ( --$i )
    {
        my $j = int rand( $i+1 );
        @$array[$i,$j] = @$array[$j,$i];
    }
}

# shut off for exploitability runs
#fisher_yates_shuffle( \@jobs );

print "queued " . scalar(@jobs) . " jobs\n";
sleep 1;

$ttljobs = scalar(@jobs);
$starttime = time;

&run_parallel($CPUS, @jobs);




