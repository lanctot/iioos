
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <ctime>

#include "bluff.h"

using namespace std; 

//static bool probing = true;

static double epsilon = 0.05;
static double tau = 1000;
static double beta = 1000000;

static string runname = "";

int sampleOnPolicy(Infoset & is, int actionshere)
{
  double roll = drand48(); 
  double sum = 0.0; 

  for (int a = 0; a < actionshere; a++) { 
    double prob = is.curMoveProbs[a];
    if (roll >= sum && roll < sum+prob) 
      return a; 

    sum += prob; 
  }

  assert(false);
  return -1;
}

double cfrprbabm(GameState & gs, int player, int depth, unsigned long long bidseq, 
                 double reach1, double reach2, double sprob1, double sprob2, int updatePlayer)
{
  CHKPROB(reach1); 
  CHKPROB(reach2); 

  // check: at terminal node?
  if (terminal(gs))
  {
    double ui_z = 0.0;
    double q_z = sprob1 * sprob2; 
    CHKPROBNZ(q_z);

    int pwinner = whowon(gs); 
    if (pwinner == updatePlayer) 
      ui_z = 1.0; 
    else if (pwinner == (3-updatePlayer))
      ui_z = -1.0; 
   
    // when probing is enabled, don't correct 

    return (ui_z / q_z); 
  }

  nodesTouched++;

  // chance nodes
  if (gs.p1roll == 0) 
  {
    GameState ngs = gs; 
    double sampleProb;
    sampleChanceEvent(1, ngs.p1roll, sampleProb);

    return cfrprbabm(ngs, player, depth+1, bidseq, reach1, reach2, sprob1, sprob2, updatePlayer); 
  }
  else if (gs.p2roll == 0)
  {
    GameState ngs = gs; 
    double sampleProb;
    sampleChanceEvent(2, ngs.p2roll, sampleProb);

    return cfrprbabm(ngs, player, depth+1, bidseq, reach1, reach2, sprob1, sprob2, updatePlayer); 
  }

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
  double moveEVs[actionshere]; 
  double stratEV = 0.0;

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

  double denom = 0; 
  for (int a = 0; a < actionshere; a++)
    denom += is.totalMoveProbs[a];

  int sampledAction = -1; 
  if (player != updatePlayer) {
    sampledAction = sampleOnPolicy(is, actionshere); 
    assert(sampledAction >= 0); 
  }

  for (int i = gs.curbid+1; i <= maxBid; i++) 
  {
    action++;

    // will we sample this action?
    //double sampleprob = (m + is.totalMoveProbs[action]) / (m + denom); 

    if (player != updatePlayer) { 
      if (action != sampledAction)
        continue;

      // take this move, make a new state for it  
      unsigned long long newbidseq = bidseq;
      GameState ngs = gs; 
      ngs.prevbid = gs.curbid;
      ngs.curbid = i; 
      ngs.callingPlayer = player;
      newbidseq |= (1 << (BLUFFBID-i)); 

      moveEVs[action] = cfrprbabm(ngs, 3-player, depth+1, newbidseq, reach1, reach2, sprob1, sprob2, updatePlayer);
      stratEV = moveEVs[action];
      break;
    }
   
    // from here on, we treat only the player == updatePlayer case
    double sampleprob = (beta + tau * is.totalMoveProbs[action]) / (beta + denom); 
    if (sampleprob > 1.0) sampleprob = 1.0; 
    if (epsilon > sampleprob) sampleprob = epsilon;

    double roll = drand48(); 

    if (roll >= sampleprob)   // not sampled
    {
      moveEVs[action] = 0; 
    }
    else   // otherwise sample it with all the rest
    {
      double newsprob1 = ( player == 1 ? sampleprob*sprob1 : sprob1); 
      double newsprob2 = ( player == 2 ? sampleprob*sprob2 : sprob2); 

      unsigned long long newbidseq = bidseq;
      double moveProb = is.curMoveProbs[action]; 
   
      CHKPROB(moveProb); 
   
      double newreach1 = (player == 1 ? moveProb*reach1 : reach1); 
      double newreach2 = (player == 2 ? moveProb*reach2 : reach2); 
      double payoff = 0.0;

      // take this move, make a new state for it  
      GameState ngs = gs; 
      ngs.prevbid = gs.curbid;
      ngs.curbid = i; 
      ngs.callingPlayer = player;
      newbidseq |= (1 << (BLUFFBID-i)); 

      // recursive call
      payoff = cfrprbabm(ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, newsprob1, newsprob2, updatePlayer);
    
      // post recursion
      moveEVs[action] = payoff;
      stratEV += moveProb*payoff;
    }
  }

  //double myreach = (player == 1 ? reach1 : reach2); 
  //double oppreach = (player == 1 ? reach2 : reach1);
  //double mysampleprob = (player == 1 ? sprob1 : sprob2);
 
  if (player == updatePlayer)
  {
    // update regrets
    for (int a = 0; a < actionshere; a++)
    {
      is.cfr[a] += moveEVs[a] - stratEV; 
    }
  }

  if (player != updatePlayer)
  {  
    // update av. strat
    for (int a = 0; a < actionshere; a++)
    {
      double inc = is.curMoveProbs[a] / (sprob1*sprob2);
      is.totalMoveProbs[a] += inc; 
    }
  }
  
  // save the infoset back to the store
  iss.put(infosetkey, is, actionshere, 0); 

  return stratEV;
}

int main(int argc, char ** argv)
{
  init();
  unsigned long long maxNodesTouched = 0; 

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
    
    if (argc >= 3) { 
      runname = argv[2];
    }

    if (argc >= 4) {
      string argstr = argv[3];
      maxNodesTouched = to_ull(argstr); 
    }
  }

  // get the iteration
  string filename = argv[1];
  vector<string> parts; 
  split(parts, filename, '.'); 
  if (parts.size() != 3 || parts[1] == "initial")
    iter = 1; 
  else
    iter = to_ull(parts[1]); 
  cout << "Set iteration to " << iter << endl;
  iter = MAX(1,iter);
    
  // try loading metadeta. files have form scratch/iss-runname.iter.dat
  if (iter > 1) { 
    string filename2 = filename;
    replace(filename2, "iss", "metainfo"); 
    cout << "Loading metadata from file " << filename2 << "..." << endl;
    loadMetaData(filename2); 
  }

  cout << "runname = " << runname << endl;

  unsigned long long bidseq = 0; 
    
  double totaltime = 0; 
  StopWatch stopwatch;

  for (; true; iter++)
  {
    GameState gs1; bidseq = 0;
    cfrprbabm(gs1, 1, 0, bidseq, 1.0, 1.0, 1.0, 1.0, 1);
    
    GameState gs2; bidseq = 0;
    cfrprbabm(gs2, 1, 0, bidseq, 1.0, 1.0, 1.0, 1.0, 2);

    if (   (maxNodesTouched > 0 && nodesTouched >= maxNodesTouched)
        || (maxNodesTouched == 0 && nodesTouched > ntNextReport))
    {
      totaltime += stopwatch.stop();

      double nps = nodesTouched / totaltime; 
      cout << endl;      
      cout << "total time: " << totaltime << " seconds. " << endl;
      cout << "nodes = " << nodesTouched << endl;
      cout << "nodes per second = " << nps << endl; 
      
      double b1 = 0.0, b2 = 0.0;
      iss.computeBound(b1, b2); 
      cout << "b1 = " << b1 << ", b2 = " << b2 << ", sum = " << (b1+b2) << endl;

      ntNextReport *= ntMultiplier; // need this here, before dumping metadata

      double conv = 0;
      conv = computeBestResponses(false, false);
      string str = "cfrabm."; str += runname; str += ".report.txt";
      report(str, totaltime, 2.0*MAX(b1,b2), 0, 0, conv);
      dumpInfosets("iss-" + runname); 
      dumpMetaData("metainfo-" + runname, totaltime); 
      
      cout << "Report done at: " << getCurDateTime() << endl;

      cout << endl;
      
      stopwatch.reset(); 
    }
    
    if (maxNodesTouched > 0 && nodesTouched >= maxNodesTouched) 
      break;
  }
  
  return 0;
}

