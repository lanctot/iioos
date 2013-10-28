
#include <cassert>
#include <iostream>
#include <cstdlib>

// used to measure variance
#include "statcounter.hpp"

#include "bluff.h"

using namespace std; 

static unsigned long long ttlLeafEvals = 0; 
static unsigned long long ttlUpdates = 0;

static StatCounter ** stats;

void allocStats() {
  cout << "Allocation stats..." << endl;
  stats = new StatCounter* [ISKMAX];
  for (int i = 0; i < ISKMAX; i++) { 
    stats[i] = new StatCounter[ACTMAX];
  }
}

void deallocStats() { 
  for (int i = 0; i < ISKMAX; i++) { 
    delete [] stats[i];
  }
  delete [] stats;
}

void dumpVarianceStats() 
{
  unsigned long long hva = 0;
  unsigned long long lva = 0;

  cout << "Dumping variance statistics" << endl;
  ofstream out("variance-txt.dat");
  ofstream outbin("variance-bin.dat", ios::binary);

  for (int i = 0; i < ISKMAX; i++) {
    for (int j = 0; j < ACTMAX; j++) {
      if (stats[i][j].size() > 0) { 
        unsigned long long infosetkey = static_cast<unsigned long long>(i); 
        string infoset = infosetkey_to_string(infosetkey);
        double var = stats[i][j].variance(); 
        out << infoset << " Move: " << j << ", Variance: " << stats[i][j].variance() << endl;

        if (stats[i][j].variance() < 0.1)
          lva++;
        else
          hva++;

        unsigned long long action = static_cast<unsigned long long>(j); 
        outbin.write(reinterpret_cast<const char *>(&infosetkey), 8); 
        outbin.write(reinterpret_cast<const char *>(&action), 8); 
        outbin.write(reinterpret_cast<const char *>(&var), 8); 
      }
    }
  }

  cout << "LVA: " << lva << endl;
  cout << "HVA: " << hva << endl;

  out.close();
  outbin.close();
}

double cfr(GameState & gs, int player, int depth, unsigned long long bidseq, 
           double reach1, double reach2, double chanceReach, int phase, int updatePlayer)
{
  // at terminal node?
  if (terminal(gs))
  {
    ttlLeafEvals++;
    return payoff(gs, updatePlayer);
  }

  // chance nodes
  if (gs.p1roll == 0) 
  {
    double EV = 0.0; 

    for (int i = 1; i <= P1CO; i++) 
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

    for (int i = 1; i <= P2CO; i++)
    {
      GameState ngs = gs; 
      ngs.p2roll = i; 
      double newChanceReach = getChanceProb(2,i)*chanceReach;

      EV += getChanceProb(2,i)*cfr(ngs, player, depth+1, bidseq, reach1, reach2, newChanceReach, phase, updatePlayer); 
    }

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

  if (infosetkey >= ISKMAX) // 2^17
    std::cerr << "infosetkey = " << infosetkey << endl;
  assert(infosetkey >= 0 && infosetkey <= ISKMAX);

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

    if (player == updatePlayer)
      stats[infosetkey][action].push(payoff);
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

  allocStats(); 

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
    //cout << "CFR; starting iteration " << iter << "... "; cout.flush(); 
    
    GameState gs1; 
    bidseq = 0; 
    cfr(gs1, 1, 0, bidseq, 1.0, 1.0, 1.0, 1, 1);
    
    GameState gs2; 
    cfr(gs2, 1, 0, bidseq, 1.0, 1.0, 1.0, 1, 2);

    //double b1 = 0.0, b2 = 0.0;
    //iss.computeBound(b1, b2); 
    //cout << " b1 = " << b1 << ", b2 = " << b2 << ", sum = " << (b1+b2) << endl;
    
    /*if (iter % 10 == 0)
    { 
      cout << "."; cout.flush(); 
      totaltime += stopwatch.stop();
      stopwatch.reset();
    }*/

    //if (totaltime > nextCheckpoint)
    {
      cout << endl;

      cout << "total time: " << totaltime << " seconds." << endl; 
      cout << "Done iteration " << iter << endl;

      //cout << "Computing bounds... "; cout.flush(); 
      //double b1 = 0.0, b2 = 0.0;
      //iss.computeBound(b1, b2); 
      //cout << " b1 = " << b1 << ", b2 = " << b2 << ", bound = " << (2.0*MAX(b1,b2)) << endl;

      //cout << "Evaluating strategies..."; cout.flush();
      //double ev = evaluate();
      //cout << " ev = " << ev << endl;

      //computeBestResponses(false, false);
      //report("cfrreport.txt", totaltime, (2.0*MAX(b1,b2)), ttlUpdates, ttlLeafEvals);
      //dumpInfosets("iss");

      //dumpVarianceStats();

      cout << endl;

     
      nextCheckpoint += cpWidth;

      stopwatch.reset(); 
    }

    if (iter == maxIters) break;
  }

  dumpVarianceStats();
  deallocStats();
}

