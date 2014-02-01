#!/usr/bin/perl

use strict;
use warnings;

if (not defined($ARGV[1])) { 
  print "Usage: parseres.perl <scratch dir> [sim|agg]\n";
  exit;
}

my $scratchdir = $ARGV[0];
my $simtype = $ARGV[1];

sub winstats
{
  # compute win rates and confidence intervals. correctly account for draws
  my $lwins = shift;
  my $rwins = shift;
  my $total = shift;

  my $statslistref = shift; # must be a reference to a list

  my $ties = ($total - $lwins - $rwins);

  # count wins as 1, draws as 0.5, and losses as 0
  my $mean = 0;
  if ($total > 0) {
    $mean = ($lwins + 0.5*$ties) / $total;
  }
  my $var = 0;

  # wins
  for (my $i = 0; $i < $lwins; $i++)
  { $var += (1.0 - $mean)*(1.0 - $mean); }

  # draws
  for (my $i = 0; $i < $ties; $i++)
  { $var += (0.5 - $mean)*(0.5 - $mean); }

  # losses
  for (my $i = 0; $i < $rwins; $i++)
  { $var += (0.0 - $mean)*(0.0 - $mean); }

  my $stddev = 0; 
  my $ci95 = 0; 
  my $lrate = 0;
  my $rrate = 0;

  if ($total > 0) { 
    $var = $var / $total;
    $stddev = sqrt($var);
    $ci95 = 1.96*$stddev / sqrt($total);

    # does this make sense..? when there are a lot of ties, not really 
    $lrate = ($lwins + 0.5*$ties) / $total;
    $rrate = ($rwins + 0.5*$ties) / $total;
  }

  my $lperc = $lrate*100.0;
  my $rperc = $rrate*100.0;
  my $ci95perc = $ci95*100.0;

  my $line = sprintf("%.2f %.2f +/- %.2f", $lperc, $rperc, $ci95perc);

  push(@$statslistref, $lrate);
  push(@$statslistref, $rrate);
  push(@$statslistref, $ci95);
  push(@$statslistref, $line);
}


# matchup -> number of games
my %matchmap = (); 

opendir(DIR, "$scratchdir");
my @FILES= readdir(DIR);
foreach my $file (@FILES) 
{
  if ($file =~ /\.log$/) { 
    #print "$file\n"; 
    $file =~ s/\.log$//;    
    my @parts = split('-', $file); 

    if (not ($parts[2] eq $simtype)) { 
      next;
    }

    #print "$file\n";

    if ($parts[0] gt $parts[1]) { 
      my $tmp = $parts[1];
      $parts[1] = $parts[0];
      $parts[0] = $tmp;
    }
    my $match = $parts[0] . "," . $parts[1] . "," . $parts[2] . "," . $parts[3] . "," . $parts[4] . "," . $parts[5]; 
    if (not defined $matchmap{$match}) { $matchmap{$match} = 0; }
    if ($matchmap{$match} < $parts[6]) { 
      $matchmap{$match} = $parts[6]; 
    }
    #print "$match\n";
  }
}
closedir(DIR);

my %totalpoints = ();
my %totalgames = ();

foreach my $match (keys %matchmap) { 

  my $gamespermatch = $matchmap{$match};
  my @matchinfo = split(",", $match); 

  my $p1 = $matchinfo[0]; 
  my $p2 = $matchinfo[1];

  my $simtype = $matchinfo[2];
  my $timelimit = $matchinfo[3];
  my $oosvariant = $matchinfo[4];
  my $delta = $matchinfo[5];

  my %wins = (); 
  $wins{$p1} = 0;
  $wins{$p2} = 0;
  my $ties = 0;

  #p1 as first player, p2 as second player
  for (my $m = 1; $m <= $gamespermatch; $m++) { 
  
    my $runname = "$p1-$p2-$simtype-$timelimit-$oosvariant-$delta-$m";
    #print "opening $scratchdir/$runname.log\n";

    my $doesnotexist = 0; 
    open(FILE, '<', "$scratchdir/$runname.log") or $doesnotexist = 1;
    if ($doesnotexist == 0) {
      #print "reading lines...\n";
      while (my $line = <FILE>) {
        chomp($line);
        # example line is "Game over. Winner is 1"
        if ($line =~ /^Game over/) { 
          #print "$runname $line\n"; 
          my @parts = split(' ', $line); 
          my $winner = $parts[2];
          if ($winner eq "P1") { 
            $wins{$p1} += 1; 
            $totalpoints{$p1} += 2;
          }
          elsif ($winner eq "P2") { 
            $wins{$p2} += 1; 
            $totalpoints{$p2} += 2;
          }
          else {
            $totalpoints{$p1} += 1;
            $totalpoints{$p2} += 1;
            $ties += 1; 
          }

          $totalgames{$p1} += 1;
          $totalgames{$p2} += 1;

          last;
        }
      }
      close(FILE); 
    }
  }

  #p2 as first player, p1 as second player
  for (my $m = 1; $m <= $gamespermatch; $m++) { 
  
    my $runname = "$p2-$p1-$simtype-$timelimit-$oosvariant-$delta-$m";

    my $doesnotexist = 0; 
    open(FILE, '<', "$scratchdir/$runname.log") or $doesnotexist = 1;
    if ($doesnotexist == 0) {
      while (my $line = <FILE>) {
        chomp($line);
        if ($line =~ /^Game over/) { 
          # example line is "Game over. Winner is 1"
          my @parts = split(' ', $line); 
          my $winner = $parts[2];
          if ($winner eq "P1") { 
            $wins{$p2} += 1; 
            $totalpoints{$p2} += 2;
          }
          elsif ($winner eq "P2") { 
            $wins{$p1} += 1; 
            $totalpoints{$p1} += 2;
          }
          else {
            $totalpoints{$p1} += 1;
            $totalpoints{$p2} += 1;
            $ties += 1;
          }


          last;
        }
      }
      close(FILE); 
    }
  }

  print "$p1-$p2 tl=$timelimit ov=$oosvariant de=$delta " . $wins{$p1} . " " . $wins{$p2} . " " . $ties;
  my $diff = ($wins{$p1} - $wins{$p2});
  my $games = ($wins{$p1} + $wins{$p2} + $ties);
  print "  (diff $diff, games $games)  ";

  # statsline

  my @statslist = ();
  my $left = $p1;
  my $right = $p2;
  my $total = $wins{$left} + $wins{$right} + $ties;
  winstats($wins{$left}, $wins{$right}, $total, \@statslist);
  my $statsline = $statslist[3];

  my $lperc = $statslist[0]*100.0;
  my $rperc = $statslist[1]*100.0;
  my $ci95perc = $statslist[2]*100.0;

  #print "$left " . $wins{$left} . ", $right " . $wins{$right} . ", ties = $ties, total = $total. $statsline\n"; 
  my $lwinscount = sprintf("(%d)", $wins{$left});
  my $rwinscount = sprintf("(%d)", $wins{$right});
  printf("%3.2f  %3.2f  +/-  %3.2f\n", $lperc, $rperc, $ci95perc);
}

# enable this if we want later
#foreach my $key (sort {$totalpoints{$b} <=> $totalpoints{$a}} keys %totalpoints) {
#  print "total points for $key = " . $totalpoints{$key} . "\n";
#}

