
#include <cassert>
#include <iostream>
#include <cstdlib>

#include "bluff.h"

using namespace std; 

static unsigned long long ttlLeafEvals = 0; 
static unsigned long long ttlUpdates = 0;

static string runname = "";

double cfrcs(GameState & gs, int player, int depth, unsigned long long bidseq, 
             double reach1, double reach2, int phase, int updatePlayer)
{
  // at terminal node?
  if (terminal(gs))
  {
    ttlLeafEvals++;
    return payoff(gs, updatePlayer);
  }

  nodesTouched++;

  // chance nodes
  if (gs.p1roll == 0) 
  {
    double EV = 0.0; 

    int outcome = 0;
    double prob = 0.0;
    sampleChanceEvent(1, outcome, prob); 

    CHKPROBNZ(prob);
    assert(outcome > 0); 

    GameState ngs = gs; 
    ngs.p1roll = outcome; 

    EV += getChanceProb(1,outcome)*cfrcs(ngs, player, depth+1, bidseq, reach1, reach2, phase, updatePlayer); 

    return EV;
  }
  else if (gs.p2roll == 0)
  {
    double EV = 0.0; 
    
    int outcome = 0;
    double prob = 0.0;
    sampleChanceEvent(2, outcome, prob); 

    CHKPROBNZ(prob);
    assert(outcome > 0); 

    GameState ngs = gs; 
    ngs.p2roll = outcome;
    
    EV += getChanceProb(2,outcome)*cfrcs(ngs, player, depth+1, bidseq, reach1, reach2, phase, updatePlayer); 

    return EV;
  }

  // check for cuts   // must include the updatePlayer
  if (phase == 1 && (   (player == 1 && updatePlayer == 1 && reach2 <= 0.0)
                     || (player == 2 && updatePlayer == 2 && reach1 <= 0.0)))
  {
    phase = 2; 
  }

  if (phase == 2 && (   (player == 1 && updatePlayer == 1 && reach1 <= 0.0)
                     || (player == 2 && updatePlayer == 2 && reach2 <= 0.0)))
  {
    return 0.0;
  }

  // declare the variables
  Infoset is;
  unsigned long long infosetkey = 0;
  double stratEV = 0.0;
  int action = -1;

  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 
  // count the number of actions and initialize moveEVs
  // special case. can't just call bluff on the first move!
  assert(actionshere > 0);
  double moveEVs[actionshere]; 
  for (int i = 0; i < actionshere; i++) 
    moveEVs[i] = 0.0;

  // get the info set
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

  // iterate over the actions
  for (int i = gs.curbid+1; i <= maxBid; i++) 
  {
    // there is a valid action here
    action++;
    assert(action < actionshere);

    unsigned long long newbidseq = bidseq;
    double moveProb = is.curMoveProbs[action]; 
    double newreach1 = (player == 1 ? moveProb*reach1 : reach1); 
    double newreach2 = (player == 2 ? moveProb*reach2 : reach2); 

    GameState ngs = gs; 
    ngs.prevbid = gs.curbid;
    ngs.curbid = i; 
    ngs.callingPlayer = player;
    newbidseq |= (1ULL << (BLUFFBID-i)); 
    
    double payoff = cfrcs(ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, phase, updatePlayer); 
   
    moveEVs[action] = payoff; 
    stratEV += moveProb*payoff; 
  }

  // post-traversals: update the infoset
  double myreach = (player == 1 ? reach1 : reach2); 
  double oppreach = (player == 1 ? reach2 : reach1); 

  if (phase == 1 && player == updatePlayer) // regrets
  {
    ttlUpdates++;

    for (int a = 0; a < actionshere; a++)
    {
      is.cfr[a] += oppreach*(moveEVs[a] - stratEV); 
    }
  }

  if (phase >= 1 && player == updatePlayer) // av. strat
  {
    for (int a = 0; a < actionshere; a++)
    {
      is.totalMoveProbs[a] += myreach*is.curMoveProbs[a]; 
    }
  }

  // save the infoset back to the store if needed
  if (player == updatePlayer) {
    ttlUpdates++;
    iss.put(infosetkey, is, actionshere, 0); 
  }

  return stratEV;
}

int main(int argc, char ** argv)
{
  unsigned long long maxIters = 0; 
  unsigned long long maxNodesTouched = 0; 
  init();

  if (argc < 2)
  {
    initInfosets();
    exit(-1);
  }
  else 
  { 
    string filename = argv[1];
    cout << "Reading the infosets from " << filename << "..." << endl;
    iss.readFromDisk(filename);
  
    //if (argc >= 3)
    //  maxIters = to_ull(argv[2]);
  
    if (argc >= 3) 
      runname = argv[2];
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

  unsigned long long bidseq = 0; 
    
  StopWatch stopwatch;
  double totaltime = 0; 
    
  cout << "Starting CFRCS iterations... " << endl;

  for (; true; iter++)
  {
    GameState gs1; 
    bidseq = 0; 
    cfrcs(gs1, 1, 0, bidseq, 1.0, 1.0, 1, 1);
    
    GameState gs2; 
    bidseq = 0; 
    cfrcs(gs2, 1, 0, bidseq, 1.0, 1.0, 1, 2);

    //cout << "time taken: " << (stopwatch.stop() - totaltime) << " seconds." << endl;  
    //totaltime = stopwatch.stop();

    //double b1 = 0.0, b2 = 0.0;
    //iss.computeBound(b1, b2); 
    //cout << " b1 = " << b1 << ", b2 = " << b2 << ", sum = " << (b1+b2) << endl;

    //if (iter % 100 == 1 || iter == maxIters) 
    if (
         (maxNodesTouched > 0 && nodesTouched >= maxNodesTouched)
        || (maxNodesTouched == 0 && nodesTouched > ntNextReport))
    {
      totaltime += stopwatch.stop();

      double nps = nodesTouched / totaltime; 

      cout << endl << "total time: " << totaltime << " seconds." << endl;
      cout << "nodes = " << nodesTouched << endl;
      cout << "nodes per second = " << nps << endl; 
      cout << "Computing bounds... "; cout.flush(); 
      double b1 = 0.0, b2 = 0.0;
      iss.computeBound(b1, b2); 
      double bound = getBoundMultiplier("cfrcs")*(2.0*MAX(b1,b2));
      cout << " b1 = " << b1 << ", b2 = " << b2 << ", bound = " << bound << endl;

      ntNextReport *= ntMultiplier; // need this here, before dumping metadata

      double conv = 0;
      conv = computeBestResponses(false, false);
      string str = "cfrcs." + runname + ".report.txt"; 
      report(str, totaltime, bound, ttlUpdates, ttlLeafEvals, conv);
      dumpInfosets("iss-" + runname); 
      dumpMetaData("metainfo-" + runname, totaltime); 

      cout << "Report done at: " << getCurDateTime() << endl;

      cout << endl;

      stopwatch.reset(); 
    }
    
    if (maxNodesTouched > 0 && nodesTouched >= maxNodesTouched) 
      break;

    if (iter == maxIters) break;
  }
}

