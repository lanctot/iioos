
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <ctime>

#include "bluff.h"

using namespace std; 

static bool scaling = false;

static double epsilon = 0.05;
static double tau = 1000;
static double beta = 1000000;

static string runname = "";

static unsigned long long numInfosets[BLUFFBID];

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

double cfrada(GameState & gs, int player, int depth, unsigned long long bidseq, 
              double reach1, double reach2, double q, int updatePlayer, double & subtreeRplus)
{
  CHKPROB(reach1); 
  CHKPROB(reach2); 

  // check: at terminal node?
  if (terminal(gs))
  {
    double ui_z = 0.0;
    double q_z = q;
    CHKPROBNZ(q_z);

    int pwinner = whowon(gs); 
    if (pwinner == updatePlayer) 
      ui_z = 1.0; 
    else if (pwinner == (3-updatePlayer))
      ui_z = -1.0; 
   
    // when probing is enabled, don't correct 
    //
    subtreeRplus = 0.0;

    return (ui_z / q_z); 
  }

  nodesTouched++;

  // chance nodes
  if (gs.p1roll == 0) 
  {
    GameState ngs = gs; 
    double sampleProb;
    sampleChanceEvent(1, ngs.p1roll, sampleProb);
    double dummy; 

    return cfrada(ngs, player, depth+1, bidseq, reach1, reach2, q, updatePlayer, dummy); 
  }
  else if (gs.p2roll == 0)
  {
    GameState ngs = gs; 
    double sampleProb;
    sampleChanceEvent(2, ngs.p2roll, sampleProb);
    double dummy;

    return cfrada(ngs, player, depth+1, bidseq, reach1, reach2, q, updatePlayer, dummy); 
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

  double scaledweights[BLUFFBID];  
  double denom = 0; 
  double maxReg = 0;
  for (int a = 0; a < actionshere; a++) {
    if (is.cfr[a] > maxReg) maxReg = is.cfr[a];
    int bid = gs.curbid+1+a;
    assert(bid >= 1 && bid <= BLUFFBID);

    if (scaling) { 
      if (numInfosets[bid-1] == 0)
          scaledweights[a] = 0.0; 
      else
          scaledweights[a] = is.weight[a] / numInfosets[bid-1];
    }
    else { 
      scaledweights[a] = is.weight[a];
    }

    denom += scaledweights[a];
  }

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
  
      double childWeight = 0;
      moveEVs[action] = cfrada(ngs, 3-player, depth+1, newbidseq, reach1, reach2, q, updatePlayer, childWeight);
      stratEV = moveEVs[action];
      is.weight[action] = childWeight;

      break;
    }
   
    // from here on, we treat only the player == updatePlayer case
    double rho = (beta + tau * scaledweights[action]) / (beta + denom); 
    if (rho > 1.0) rho = 1.0; 
    if (epsilon > rho) rho = epsilon;

    double roll = drand48(); 

    if (roll >= rho)   // not sampled
    {
      moveEVs[action] = 0; 
    }
    else   // otherwise sample it with all the rest
    {
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
      double childWeight = 0;
      payoff = cfrada(ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, q*rho, updatePlayer, childWeight);
      is.weight[action] = childWeight;
    
      // post recursion
      moveEVs[action] = payoff;
      stratEV += moveProb*payoff;
    }
  }

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
      double inc = is.curMoveProbs[a] / q;
      is.totalMoveProbs[a] += inc; 
    }
  }
  
  // save the infoset back to the store
  iss.put(infosetkey, is, actionshere, 0); 

  // adaptive sampling req.
  double sumChildWeights = 0;
  for (int a = 0; a < actionshere; a++) {
    sumChildWeights += is.weight[a];
  }
  subtreeRplus = maxReg + sumChildWeights;

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
      //maxNodesTouched = to_ull(argstr); 
      if (argstr == "1") { scaling = true; }
      else { scaling = false; }
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

  // fill the top
  for (int bid = 1; bid <= BLUFFBID; bid++) { 
    numInfosets[bid-1] = pow2(13-bid) - 1;
  }

  cout << "runname = " << runname << endl;

  unsigned long long bidseq = 0; 
    
  double totaltime = 0; 
  StopWatch stopwatch;

  for (; true; iter++)
  {
    double dummy = 0;


    GameState gs1; bidseq = 0; 
    cfrada(gs1, 1, 0, bidseq, 1.0, 1.0, 1.0, 1, dummy);
    
    GameState gs2; bidseq = 0; 
    cfrada(gs2, 1, 0, bidseq, 1.0, 1.0, 1.0, 2, dummy);

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
      string str = "cfrada-s"; str += (scaling ? "1" : "0"); str += "."; str += runname; str += ".report.txt";
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

