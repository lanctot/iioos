
#include <cassert>
#include <iostream>
#include <cstdlib>

#include "bluff.h"

using namespace std; 

static unsigned long long ttlLeafEvals = 0; 
static unsigned long long ttlUpdates = 0;

static unsigned long long nextReport = 1;
static unsigned long long reportMult = 2;

double cfr(GameState & gs, int player, int depth, unsigned long long bidseq, 
           double reach1, double reach2, double chanceReach, int phase, int updatePlayer)
{
  // at terminal node?
  if (terminal(gs))
  {
    ttlLeafEvals++;
    return payoff(gs, updatePlayer);
  }

  nodesTouched++;

  // use chance sampling for imperfect recall paper
  if (gs.p1roll == 0) 
  {
    double EV = 0.0; 

    for (int i = 1; i <= numChanceOutcomes(1); i++) 
    {
      GameState ngs = gs; 
      ngs.p1roll = i; 
      double newChanceReach = getChanceProb(1,i)*chanceReach;

      EV += getChanceProb(1,i)*cfr(ngs, player, depth+1, bidseq, reach1, reach2, newChanceReach, phase, updatePlayer); 
    }

    return EV;
  }
  else if (gs.p2roll == 0)
  {
    double EV = 0.0; 

    for (int i = 1; i <= numChanceOutcomes(2); i++)
    {
      GameState ngs = gs; 
      ngs.p2roll = i; 
      double newChanceReach = getChanceProb(2,i)*chanceReach;

      EV += getChanceProb(2,i)*cfr(ngs, player, depth+1, bidseq, reach1, reach2, newChanceReach, phase, updatePlayer); 
    }

    return EV;
  }
 
  /*
  // sample chance
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

    EV += cfr(ngs, player, depth+1, bidseq, reach1, reach2, chanceReach*prob, phase, updatePlayer); 

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
    
    EV += cfr(ngs, player, depth+1, bidseq, reach1, reach2, chanceReach*prob, phase, updatePlayer); 

    return EV;
  }
  */

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
    
    double payoff = cfr(ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, chanceReach, phase, updatePlayer); 
   
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
      is.cfr[a] += (chanceReach*oppreach)*(moveEVs[a] - stratEV); 
      //is.cfr[a] += (chanceReach*myreach*oppreach)*(moveEVs[a] - stratEV); 
      //is.cfr[a] += (moveEVs[a] - stratEV); 
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

    if (argc >= 3)
      maxIters = to_ull(argv[2]);
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

  unsigned long long bidseq = 0; 
    
  StopWatch stopwatch;
  double totaltime = 0; 

  cout << "Starting CFR iterations" << endl;

  for (; true; iter++)
  {
    GameState gs1; 
    bidseq = 0; 
    double ev1 = cfr(gs1, 1, 0, bidseq, 1.0, 1.0, 1.0, 1, 1);
    
    GameState gs2; 
    bidseq = 0; 
    double ev2 = cfr(gs2, 1, 0, bidseq, 1.0, 1.0, 1.0, 1, 2);

    sumEV1 += ev1; 
    sumEV2 += ev2; 

    //double b1 = 0.0, b2 = 0.0;
    //iss.computeBound(b1, b2); 
    //cout << " b1 = " << b1 << ", b2 = " << b2 << ", sum = " << (b1+b2) << endl;
    
    if (iter % 10 == 0)
    { 
      cout << "."; cout.flush(); 
      totaltime += stopwatch.stop();
      stopwatch.reset();
    }

    //if (iter >= nextReport)
    //if (totaltime >= nextCheckpoint)
    if (iter == 1 || nodesTouched >= ntNextReport)
    {
      cout << endl;

      cout << "total time: " << totaltime << " seconds." << endl; 
      cout << "Done iteration " << iter << endl;

      cout << "Computing bounds... "; cout.flush(); 
      double b1 = 0.0, b2 = 0.0;
      iss.computeBound(b1, b2); 
      cout << " b1 = " << b1 << ", b2 = " << b2 << ", bound = " << (2.0*MAX(b1,b2)) << endl;

      //cout << "Evaluating strategies..."; cout.flush();
      //double ev = evaluate();
      //cout << " ev = " << ev << endl;

      double conv = 0.0;
      double p1value = 0.0;
      double p2value = 0.0;
      conv = computeBestResponses(false, false, p1value, p2value);

      double R1Tplus = iter*p1value - sumEV1; if (R1Tplus <= 0.0) { R1Tplus = 0.0; } 
      double R2Tplus = iter*p2value - sumEV2; if (R2Tplus <= 0.0) { R2Tplus = 0.0; } 

      report("cfrreport.txt", totaltime, (2.0*MAX(b1,b2)), ttlUpdates, ttlLeafEvals, conv, (R1Tplus+R2Tplus)/iter);
      //dumpInfosets("iss");

      //dumpVarianceStats();

      cout << endl;

      nextCheckpoint += cpWidth;
      nextReport *= reportMult;
      ntNextReport *= ntMultiplier;

      stopwatch.reset(); 
    }

    if (iter == maxIters) break;
  }
}

