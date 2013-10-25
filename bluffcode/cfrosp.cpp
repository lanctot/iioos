
#include <iostream>
#include <cassert>
#include <cstdlib>

#include "bluff.h"

using namespace std; 

static string runname = "";
static bool probing = false; 

/* this is rich's version of outcome sampling */

static double probe(GameState & gs, int player, int depth, unsigned long long bidseq, int updatePlayer)
{
  // check: at terminal node?
  if (terminal(gs))
  {
    return payoff(gs, updatePlayer); 
  }
  
  Infoset is;
  unsigned long long infosetkey = 0;
  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 
  
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

  double sampleProb;
  int action = sampleAction(player, is, actionshere, sampleProb, 0.0, false);
  assert(action >= 0 && action < actionshere);
  //double lva = isLVA(infosetkey, action);

  //double lvaSampleProb = 0.5;
  //double newsprob1 = ( (lva && player == 1) ? lvaSampleProb*sprob1 : sprob1); 
  //double newsprob2 = ( (lva && player == 2) ? lvaSampleProb*sprob2 : sprob2); 

  GameState ngs = gs; 
  ngs.prevbid = gs.curbid;
  int i = gs.curbid+action+1;
  ngs.curbid = i;
  ngs.callingPlayer = player;
  unsigned long long newbidseq = bidseq;
  newbidseq |= (1 << (BLUFFBID-i)); 

  return probe(ngs, 3-player, depth+1, newbidseq, updatePlayer);
  //return is.curMoveProbs[action]*probe(ngs, 3-player, depth+1, newbidseq, newsprob1, newsprob2, updatePlayer);
}

double cfrosp(GameState & gs, int player, int depth, unsigned long long bidseq, 
              double reach1, double reach2, double sprob1, double sprob2, int updatePlayer)
{
  CHKPROB(reach1); 
  CHKPROB(reach2); 

  // check: at terminal node?
  if (terminal(gs))
  {
    return payoff(gs, updatePlayer) / (sprob1 * sprob2); 
  }
  
  nodesTouched++;

  // chance nodes
  if (gs.p1roll == 0) 
  {
    GameState ngs = gs; 
    double sampleProb;
    sampleChanceEvent(1, ngs.p1roll, sampleProb);

    // don't need to worry about keeping track of sampled chance probs for outcome sampling
    return cfrosp(ngs, player, depth+1, bidseq, reach1, reach2, sprob1, sprob2, updatePlayer); 
  }
  else if (gs.p2roll == 0)
  {
    GameState ngs = gs; 
    double sampleProb;
    sampleChanceEvent(2, ngs.p2roll, sampleProb);
    
    // don't need to worry about keeping track of sampled chance probs for outcome sampling
    return cfrosp(ngs, player, depth+1, bidseq, reach1, reach2, sprob1, sprob2, updatePlayer); 
  }

  // declare the variables
  Infoset is;
  unsigned long long infosetkey = 0;
  int action = -1;

  // count the number of actions. Can't call Bluff as first action when there is no bid
  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 
  assert(actionshere > 0);
  double moveEVs[actionshere]; 
  double stratEV = 0.0;

  // get the info set from the info set store (iss) 
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

  // sample the action to take. Behavioral uniform at my nodes. On-Policy at opponents'.

  double sampleprob = -1; 
  int takeAction = 0; 

  if (player == updatePlayer)
    takeAction = sampleAction(player, is, actionshere, sampleprob, 0.5, false); 
  else
    takeAction = sampleAction(player, is, actionshere, sampleprob, 0.0, false); 

  CHKPROBNZ(sampleprob); 
  double newsprob1 = (player == 1 ? sampleprob*sprob1 : sprob1); 
  double newsprob2 = (player == 2 ? sampleprob*sprob2 : sprob2); 
  
  for (int i = gs.curbid+1; i <= maxBid; i++) 
  {
    action++;
   
    // on opponent nodes, just sample the single action
    if (player != updatePlayer) { 
      if (action != takeAction)
        continue;

      // take this move, make a new state for it  
      unsigned long long newbidseq = bidseq;
      GameState ngs = gs; 
      ngs.prevbid = gs.curbid;
      ngs.curbid = i; 
      ngs.callingPlayer = player;
      newbidseq |= (1 << (BLUFFBID-i)); 
      
      double moveProb = is.curMoveProbs[action]; 
      CHKPROB(moveProb); 
      double newreach1 = (player == 1 ? moveProb*reach1 : reach1); 
      double newreach2 = (player == 2 ? moveProb*reach2 : reach2); 

      moveEVs[action] = cfrosp(ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, newsprob1, newsprob2, 
                               updatePlayer);

      stratEV = moveProb*moveEVs[action];
      break;
    }

    if (action == takeAction) // only get here when player == updatePlayer
    {
      unsigned long long newbidseq = bidseq;
      double moveProb = is.curMoveProbs[action]; 
      CHKPROB(moveProb); 
     
      double newreach1 = (player == 1 ? moveProb*reach1 : reach1); 
      double newreach2 = (player == 2 ? moveProb*reach2 : reach2); 

      // take this move, make a new state for it  
      GameState ngs = gs; 
      ngs.prevbid = gs.curbid;
      ngs.curbid = i; 
      ngs.callingPlayer = player;
      newbidseq |= (1 << (BLUFFBID-i)); 

      // recursive call
      double payoff = 0;

      if (player == updatePlayer && probing)
        payoff = cfrosp(ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, newsprob1, newsprob2, 
                        updatePlayer) * sampleprob; // (p/q) 
      else 
        payoff = cfrosp(ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, newsprob1, newsprob2, 
                        updatePlayer);

      moveEVs[action] = payoff; 
      stratEV += payoff*moveProb; 
    }
    else 
    {
      // probe!
      if (player == updatePlayer && probing) 
      {
        GameState ngs = gs; 
        ngs.prevbid = gs.curbid;
        ngs.curbid = i; 
        ngs.callingPlayer = player;
        unsigned long long newbidseq = bidseq; 
        newbidseq |= (1 << (BLUFFBID-i));

        double moveProb = is.curMoveProbs[action]; 
        CHKPROB(moveProb); 

        double payoff = probe(ngs, 3-player, depth+1, newbidseq, updatePlayer) / (sprob1 * sprob2);  

        moveEVs[action] = payoff;
        stratEV += moveProb*payoff;
      }
      else if (player == updatePlayer)
      {
        moveEVs[action] = 0;
      }
    }
  }

  double myreach = (player == 1 ? reach1 : reach2); 
  //double oppreach = (player == 1 ? reach2 : reach1); 
  //double mysamplereach = (player == 1 ? sprob1 : sprob2); 
 
  // update regrets (my infosets only)
  if (player == updatePlayer) 
  { 
    for (int a = 0; a < actionshere; a++)
    {
      // oppreach and sprob2 cancel
      //is.cfr[a] += oppreach * (moveEVs[a] - stratEV) / sprob1; 
      is.cfr[a] += (moveEVs[a] - stratEV); 
    }
  }
 
  if (player != updatePlayer) { 
    // update av. strat
    for (int a = 0; a < actionshere; a++)
    {
      double inc = (1.0 / (sprob1*sprob2))*myreach*is.curMoveProbs[a];
      is.totalMoveProbs[a] += inc; 
    }
  }

  // update the tracker. needed for optimistic averaging
  //is.lastUpdate = iter; 

  // save the infoset back to the store
  iss.put(infosetkey, is, actionshere, 0); 

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
    
  double totaltime = 0; 
  StopWatch stopwatch;

  for (; true; iter++)
  {
    GameState gs1; bidseq = 0;
    cfrosp(gs1, 1, 0, bidseq, 1.0, 1.0, 1.0, 1.0, 1);
    
    GameState gs2; bidseq = 0;
    cfrosp(gs1, 1, 0, bidseq, 1.0, 1.0, 1.0, 1.0, 2);

    if (   (maxNodesTouched > 0 && nodesTouched >= maxNodesTouched)
        || (maxNodesTouched == 0 && nodesTouched >= ntNextReport))
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
      string str = "cfrosp." + runname + ".report.txt"; 
      report(str, totaltime, 2.0*MAX(b1,b2), 0, 0, conv);
      dumpInfosets("iss-" + runname); 
      dumpMetaData("metainfo-" + runname, totaltime); 
      
      cout << "Report done at: " << getCurDateTime() << endl;

      cout << endl;
      
      stopwatch.reset(); 
    
      if (maxNodesTouched > 0 && nodesTouched >= maxNodesTouched) 
        break;
    
      if (iter == maxIters) break;
    }
  }
  
  return 0;
}

