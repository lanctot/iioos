
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <cstring>

#include "bluff.h"

using namespace std; 

static vector<int> savedChanceEvents;
static vector<double> savedChanceProbs;
static unsigned int chanceIndex; 
static bool loadingChance;

static string runname = "";

void sampleOrLoadChanceEvent(GameState & gs, int player, double & sampleProb)
{
  int & roll = (player == 1 ? gs.p1roll : gs.p2roll); 
  
  if (loadingChance) 
  {
    //cout << chanceIndex << " " << savedChanceEvents.size() << endl;
    assert(chanceIndex < savedChanceEvents.size());
    roll = savedChanceEvents[chanceIndex];
    sampleProb = savedChanceProbs[chanceIndex];
    //cout << "loaded chance roll: " << roll << ", prob " << sampleProb << endl;
    chanceIndex++;    
    CHKPROBNZ(sampleProb);
  }
  else 
  {
    sampleChanceEvent(player, roll, sampleProb);
    CHKPROBNZ(sampleProb);
    
    //cout << "sampleing chance roll: " << roll << ", prob " << sampleProb << endl;

    savedChanceEvents.push_back(roll); 
    savedChanceProbs.push_back(sampleProb);
  }
}

double cfres(GameState & gs, int player, int depth, unsigned long long bidseq, 
             double reach1, double reach2, double chanceReach, int updatePlayer) 
{
  CHKPROB(reach1); 
  CHKPROB(reach2); 
  CHKPROBNZ(chanceReach); 

  // check: at terminal node?
  if (terminal(gs))
  {
    return payoff(gs, updatePlayer); 
  }

  nodesTouched++;

  // chance nodes
  if (gs.p1roll == 0) 
  {
    GameState ngs = gs; 
    double sampleProb = 0.0;

    sampleOrLoadChanceEvent(ngs, 1, sampleProb);
    
    return cfres(ngs, player, depth+1, bidseq, reach1, reach2, sampleProb*chanceReach, updatePlayer); 
  }
  else if (gs.p2roll == 0)
  {
    GameState ngs = gs; 
    double sampleProb = 0.0;
    
    sampleOrLoadChanceEvent(ngs, 2, sampleProb);

    return cfres(ngs, player, depth+1, bidseq, reach1, reach2, sampleProb*chanceReach, updatePlayer); 
  }
  
  // declare the variables
  Infoset is;
  unsigned long long infosetkey = 0;
  int action = -1;
  
  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 
  assert(actionshere > 0);
  
  double moveEVs[actionshere]; 
  for (int i = 0; i < actionshere; i++) 
    moveEVs[i] = 0.0;

  // get the info set
  getInfoset(gs, player, bidseq, is, infosetkey, actionshere);

  double stratEV = 0.0;

  // traverse or sample actions.
  if (player != updatePlayer)           
  {
    // sample opponent nodes
    double sampleprob = -1; 
    int takeAction = sampleAction(player, is, actionshere, sampleprob, 0.0, false); 
    CHKPROBNZ(sampleprob); 

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

    CHKPROBNZ(moveProb); 
 
    double newreach1 = (player == 1 ? moveProb*reach1 : reach1); 
    double newreach2 = (player == 2 ? moveProb*reach2 : reach2); 

    GameState ngs = gs; 
    ngs.prevbid = gs.curbid;
    ngs.curbid = i; 
    ngs.callingPlayer = player;
    newbidseq |= (1 << (BLUFFBID-i)); 

    // recursive call
    stratEV = cfres(ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, chanceReach, updatePlayer);
  }
  else 
  {
    // travers over my nodes
    for (int i = gs.curbid+1; i <= maxBid; i++) 
    {
      // there is a valid action here
      action++;
      assert(action < actionshere);

      unsigned long long newbidseq = bidseq;
      double moveProb = is.curMoveProbs[action]; 

      //CHKPROBNZ(moveProb); 

      double newreach1 = (player == 1 ? moveProb*reach1 : reach1); 
      double newreach2 = (player == 2 ? moveProb*reach2 : reach2); 

      GameState ngs = gs; 
      ngs.prevbid = gs.curbid;
      ngs.curbid = i; 
      ngs.callingPlayer = player;
      newbidseq |= (1ULL << (BLUFFBID-i)); 
    
      double payoff = cfres(ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, chanceReach, updatePlayer);
    
      moveEVs[action] = payoff; 
      stratEV += moveProb*payoff; 
    }
  }

  // on my nodes, update the regrets

  if (player == updatePlayer) 
  {
    for (int a = 0; a < actionshere; a++)
      is.cfr[a] += (moveEVs[a] - stratEV); 
  }

  // on opponent node, update the average strategy

  if (player != updatePlayer) 
  {
    // myreach cancels here
    //double myreach = (player == 1 ? reach1 : reach2); 
    //double sampleProb = myreach*chanceReach;
    //for (int a = 0; a < actionshere; a++)
    //  is.totalMoveProbs[a] += (1.0 / sampleProb) * myreach * is.curMoveProbs[a]; 
    
    for (int a = 0; a < actionshere; a++)
      is.totalMoveProbs[a] += (1.0 / chanceReach) * is.curMoveProbs[a]; 
  }
  
  // we're always  updating, so save back to the store
  //ttlUpdates++;
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
    //regretUpdates.clear(); 
    //avstraUpdates.clear(); 

    savedChanceEvents.clear();
    savedChanceProbs.clear();
    chanceIndex = 0;
    loadingChance = false;
    
    GameState gs1; bidseq = 0;
    cfres(gs1, 1, 0, bidseq, 1.0, 1.0, 1.0, 1); 

    chanceIndex = 0;
    loadingChance = true; 
    
    GameState gs2; bidseq = 0;
    cfres(gs2, 1, 0, bidseq, 1.0, 1.0, 1.0, 2); 

    // leaving this in to support fixed time-based checkpoints
    if (iter % 10000 == 0)
    { 
      cout << "."; cout.flush(); 
      totaltime += stopwatch.stop();
      stopwatch.reset();
    }

    if (   (maxNodesTouched > 0 && nodesTouched >= maxNodesTouched)
        || (maxNodesTouched == 0 && nodesTouched >= ntNextReport))
    //if (totaltime > nextCheckpoint)
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
      nextCheckpoint += cpWidth;

      double conv = 0;
      conv = computeBestResponses(false, false);
      string str = "cfres." + runname + ".report.txt"; 
      report(str, totaltime, 2.0*MAX(b1,b2), 0, 0, conv);
      //dumpInfosets("iss-" + runname); 
      //dumpMetaData("metainfo-" + runname, totaltime); 

      cout << "Report done at: " << getCurDateTime() << endl;

      cout << endl;
      

      stopwatch.reset(); 
    }

    if (maxNodesTouched > 0 && nodesTouched >= maxNodesTouched) 
      break;
  }
  
  return 0;
}

