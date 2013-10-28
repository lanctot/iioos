
#include <iostream>
#include <cassert>
#include <cstdlib>

#include "bluff.h"

using namespace std; 

/* outcome sampling as it was described in original MCCFR paper, e.g. samples on both players and reuses the sample */

static double epsilon = 0.6;

double cfros(GameState & gs, int player, int depth, unsigned long long bidseq, 
             double reach1, double reach2, double sprob1, double sprob2, double & suffixreach, 
             double & rtlSampleProb)
{
  CHKPROB(reach1); 
  CHKPROB(reach2); 

  // check: at terminal node?
  if (terminal(gs))
  {
    suffixreach = 1.0; 
    rtlSampleProb = sprob1*sprob2;

    // payoff always return in view of p1
    int pwinner = whowon(gs); 
    if (pwinner == 1) 
      return 1.0; 
    else if (pwinner == 2)
      return -1.0; 
    else
      return 0; 
  }

  // chance nodes
  if (gs.p1roll == 0) 
  {
    GameState ngs = gs; 
    double sampleProb;
    sampleChanceEvent(1, ngs.p1roll, sampleProb);

    // don't need to worry about keeping track of sampled chance probs for outcome sampling
    return cfros(ngs, player, depth+1, bidseq, reach1, reach2, sprob1, sprob2, suffixreach, rtlSampleProb); 
  }
  else if (gs.p2roll == 0)
  {
    GameState ngs = gs; 
    double sampleProb;
    sampleChanceEvent(2, ngs.p2roll, sampleProb);
    
    // don't need to worry about keeping track of sampled chance probs for outcome sampling
    return cfros(ngs, player, depth+1, bidseq, reach1, reach2, sprob1, sprob2, suffixreach, rtlSampleProb); 
  }

  //assert(gs.p1roll >= 1 && gs.p1roll <= 6);
  //assert(gs.p2roll >= 1 && gs.p2roll <= 6);

  // declare the variables
  Infoset is;
  unsigned long long infosetkey = 0;
  int action = -1;

  // count the number of actions. Can't call Bluff as first action when there is no bid
  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 
  // count the number of actions and initialize moveEVs
  // special case. can't just call bluff on the first move!
  assert(actionshere > 0);

  // get the info set from the info set store (iss) 
  // infoset key is the (bid sequence) (my roll) (0|1) 
  infosetkey = bidseq;  
  infosetkey <<= iscWidth; 
  if (player == 1)
  {
    infosetkey |= gs.p1roll; 
    infosetkey <<= 1; 
    bool ret = iss.get(infosetkey, is, actionshere, 0); 
    assert(ret);
  }
  else if (player == 2)
  {
    infosetkey |= gs.p2roll; 
    infosetkey <<= 1; 
    infosetkey |= 1; 
    bool ret = iss.get(infosetkey, is, actionshere, 0); 
    assert(ret);
  }

  // note: regret-matching is done when we pull this infoset out of the infoset store
  // so here, the infoset will have the proper strategy distribution 

  // sample the action to take
  double sampleprob = -1; 
  int takeAction = sampleAction(player, is, actionshere, sampleprob, epsilon, true); 
  CHKPROBNZ(sampleprob); 
  double newsprob1 = (player == 1 ? sampleprob*sprob1 : sprob1); 
  double newsprob2 = (player == 2 ? sampleprob*sprob2 : sprob2); 

  // take the action. find the i for this action
  int i;
  for (i = gs.curbid+1; i <= maxBid; i++) 
  {
    action++; 

    if (action == takeAction)
      break; 
  }

  assert(i >= gs.curbid+1 && i <= maxBid); 

  unsigned long long newbidseq = bidseq;
  double moveProb = is.curMoveProbs[action]; 
 
  CHKPROB(moveProb); 
 
  double newreach1 = (player == 1 ? moveProb*reach1 : reach1); 
  double newreach2 = (player == 2 ? moveProb*reach2 : reach2); 
  double p1payoff = 0.0;
  double newsuffixreach = 1.0; 

  // take this move, make a new state for it  
  GameState ngs = gs; 
  ngs.prevbid = gs.curbid;
  ngs.curbid = i; 
  ngs.callingPlayer = player;
  newbidseq |= (1 << (BLUFFBID-i)); 

  // recursive call
  p1payoff = cfros(ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, newsprob1, newsprob2, 
                   newsuffixreach, rtlSampleProb); 

  // post-traversal: update the infoset
  // ctlReach = child-to-leaf reach
  // itlReach = (current)infoset-to-leaf reach
  double ctlReach = newsuffixreach; 
  double itlReach = newsuffixreach*is.curMoveProbs[action]; 
  suffixreach = itlReach;

  double mypayoff = (player == 1 ? p1payoff : -p1payoff); 
  double myreach = (player == 1 ? reach1 : reach2); 
  double oppreach = (player == 1 ? reach2 : reach1); 
 
  // update regrets
  for (int a = 0; a < actionshere; a++)
  {
    CHKPROBNZ(rtlSampleProb);

    double U = mypayoff * oppreach / rtlSampleProb;
    double r = 0.0; 
    if (a == action) 
      r = U * (ctlReach - itlReach);
    else 
      r = -U * itlReach;

    is.cfr[a] += r; 
  }
  
  // update av. strat
  for (int a = 0; a < actionshere; a++)
  {
    double inc = (iter - is.lastUpdate)*myreach*is.curMoveProbs[a];
    is.totalMoveProbs[a] += inc; 
  }

  // update the tracker. needed for optimistic averaging
  is.lastUpdate = iter; 

  // save the infoset back to the store
  iss.put(infosetkey, is, actionshere, 0); 

  return p1payoff;
}

int main(int argc, char ** argv)
{
  init();

  if (argc < 2)
  {
    initInfosets();
    cout << "Created infosets. Exiting." << endl;
    exit(0);
  }
  else
  {
    cout << "Reading infosets from " << argv[1] << endl;
    if (!iss.readFromDisk(argv[1]))
    {
      cerr << "Problem reading file. " << endl; 
      exit(-1); 
    }
  }

  unsigned long long bidseq = 0; 
    
  double totaltime = 0; 
  StopWatch stopwatch;

  for (iter = 1; true; iter++)
  {
    GameState gs1; bidseq = 0;
    double suffixreach = 1.0; 
    double rtlSampleProb = 1.0; 
    cfros(gs1, 1, 0, bidseq, 1.0, 1.0, 1.0, 1.0, suffixreach, rtlSampleProb);

    if (iter % 100000 == 0) 
    { 
      totaltime += stopwatch.stop();
      stopwatch.reset(); 
      cout << "."; cout.flush(); 
    }

    if (totaltime > nextCheckpoint)
    {
      cout << endl;      
      cout << "total time: " << totaltime << " seconds. ";

      double b1 = 0.0, b2 = 0.0;
      iss.computeBound(b1, b2); 
      cout << "b1 = " << b1 << ", b2 = " << b2 << ", sum = " << (b1+b2) << endl;

      double conv = computeBestResponses(false, true);
      report("cfrosreport.txt", totaltime, (2.0*MAX(b1,b2)), 0, 0, conv);
      //dumpInfosets("issos"); 
      //report(totaltime, "cfrosreport.txt");
      if (iter > 1) nextCheckpoint += cpWidth; 

      cout << endl;

      stopwatch.reset();
    }
  }
  
  return 0;
}

