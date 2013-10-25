
#include <cassert>
#include <iostream>
#include <cstdlib>

#include "bluff.h"

using namespace std;

static InfosetStore issa;
      
static int brplayer = -1; 

//static double epsilon = 0.05;

#if 0
static double cfrcache[131072][13]; 
static int numactions[131072] = {0};

void clearcache() { 
  for (int i = 0; i < 131072; i++) 
    for (int j = 0; j < 13; j++) 
      cfrcache[i][j] = 0.0;
}

void commitcache() {
  for (int i = 0; i < 131072; i++) 
  {
    if (numactions[i] > 0) {
      
      Infoset is;
      bool ret = iss.get(i, is, numactions[i], 0); 
      assert(ret);
  
      for (int j = 0; j < numactions[i]; j++) {
        is.cfr[j] += cfrcache[i][j];
      }

      iss.put(i, is, numactions[i], 0);
    }
  }
}
#endif 

void getFullInfosetKey(GameState & gs, unsigned long long & infosetkey, int player, unsigned long long bidseq) 
{  
  infosetkey = bidseq;  
  infosetkey <<= iscWidth; 
  if (player == 1)
  {
    infosetkey |= gs.p1roll; 
    infosetkey <<= 1; 
  }
  else if (player == 2)
  {
    infosetkey |= gs.p2roll; 
    infosetkey <<= 1; 
    infosetkey |= 1; 
  }
}

/**
 * This is a CFR-BR iteration without abstraction. 
 *
 * In a CFR-BR iteration, when you're not the updatePlayer, take the best response 
 * action stored in is.lastUpdate
 */
double cfrbr_noabs(GameState & gs, int player, int depth, unsigned long long bidseq, 
                   double reach1, double reach2, double chanceReach, int phase, int updatePlayer)
{
  // at terminal node?
  if (terminal(gs))
  {
    //ttlLeafEvals++;
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

      EV += getChanceProb(1,i)*cfrbr_noabs(ngs, player, depth+1, bidseq, reach1, reach2, newChanceReach, phase, updatePlayer); 
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
      
      EV += getChanceProb(2,i)*cfrbr_noabs(ngs, player, depth+1, bidseq, reach1, reach2, newChanceReach, phase, updatePlayer); 
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
  getFullInfosetKey(gs, infosetkey, player, bidseq);
  bool ret = iss.get(infosetkey, is, actionshere, 0);   
  assert(ret);
  
  // iterate over the actions
  for (int i = gs.curbid+1; i <= maxBid; i++) 
  {
    // there is a valid action here
    action++;
    assert(action < actionshere);

    unsigned long long newbidseq = bidseq;
    double moveProb = 0.0; 
    double newreach1 = 0.0;
    double newreach2 = 0.0;
    
    if (player == updatePlayer) 
    {
      moveProb = is.curMoveProbs[action]; 
      newreach1 = (player == 1 ? moveProb*reach1 : reach1); 
      newreach2 = (player == 2 ? moveProb*reach2 : reach2); 
    }
    else 
    {
      assert(is.lastUpdate >= 0 && is.lastUpdate < static_cast<unsigned int>(actionshere));

      if (action == static_cast<int>(is.lastUpdate)) 
      {
        moveProb = 1.0;
        newreach1 = reach1;
        newreach2 = reach2; 
      }
      else 
      {
        moveProb = 0.0;
        newreach1 = (player == 1 ? 0.0 : reach1);
        newreach2 = (player == 2 ? 0.0 : reach2);
      }

    }

    GameState ngs = gs; 
    ngs.prevbid = gs.curbid;
    ngs.curbid = i; 
    ngs.callingPlayer = player;
    newbidseq |= (1ULL << (BLUFFBID-i)); 
    
    double payoff = cfrbr_noabs(ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, 
                                chanceReach, phase, updatePlayer); 
   
    moveEVs[action] = payoff; 
    stratEV += moveProb*payoff; 
  }

  // post-traversals: update the infoset
  double myreach = (player == 1 ? reach1 : reach2); 
  double oppreach = (player == 1 ? reach2 : reach1); 

  if (phase == 1 && player == updatePlayer) // regrets
  {
    //ttlUpdates++; 

    for (int a = 0; a < actionshere; a++)
    {
      is.cfr[a] += (chanceReach*oppreach)*(moveEVs[a] - stratEV); 
      //cfrcache[infosetkey][a] += (chanceReach*oppreach)*(moveEVs[a] - stratEV);
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
    //ttlUpdates++;
    iss.put(infosetkey, is, actionshere, 0); 
  }

  return stratEV;
}

/**
 * This is a CFR-BR iteration with abstraction. 
 *
 * Note: abstract current strategies are saved in issa. 
 * Best response strategies should be saved in iss. 
 */
double cfrbr_abs(GameState & gs, int player, int depth, unsigned long long bidseq, 
                 double reach1, double reach2, double chanceReach, int phase, int updatePlayer)
{
  // at terminal node?
  if (terminal(gs))
  {
    //ttlLeafEvals++;
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

      EV += getChanceProb(1,i)*cfrbr_abs(ngs, player, depth+1, bidseq, reach1, reach2, newChanceReach, phase, updatePlayer); 
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
      
      EV += getChanceProb(2,i)*cfrbr_abs(ngs, player, depth+1, bidseq, reach1, reach2, newChanceReach, phase, updatePlayer); 
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

  // get the full infoset (iss); needed for the BR strategy
  unsigned long long fisk = 0;
  Infoset fis;
  getFullInfosetKey(gs, fisk, player, bidseq);
  bool ret = iss.get(fisk, fis, actionshere, 0); 
  assert(ret);

  // get the abstract infoset from abstract table (issa)
  infosetkey = 0; 
  getAbsInfosetKey(gs, infosetkey, player, bidseq);
  ret = issa.get(infosetkey, is, actionshere, 0); 
  assert(ret);

  // iterate over the actions
  for (int i = gs.curbid+1; i <= maxBid; i++) 
  {
    // there is a valid action here
    action++;
    assert(action < actionshere);

    unsigned long long newbidseq = bidseq;
    double moveProb = 0.0; 
    double newreach1 = 0.0;
    double newreach2 = 0.0;
    
    if (player == updatePlayer) 
    {
      moveProb = is.curMoveProbs[action]; 
      newreach1 = (player == 1 ? moveProb*reach1 : reach1); 
      newreach2 = (player == 2 ? moveProb*reach2 : reach2); 
    }
    else 
    {
      assert(fis.lastUpdate >= 0 && fis.lastUpdate < static_cast<unsigned int>(actionshere));

      if (action == static_cast<int>(fis.lastUpdate)) 
      {
        moveProb = 1.0;
        newreach1 = reach1;
        newreach2 = reach2; 
      }
      else 
      {
        moveProb = 0.0;
        newreach1 = (player == 1 ? 0.0 : reach1);
        newreach2 = (player == 2 ? 0.0 : reach2);
      }
    }

    GameState ngs = gs; 
    ngs.prevbid = gs.curbid;
    ngs.curbid = i; 
    ngs.callingPlayer = player;
    newbidseq |= (1ULL << (BLUFFBID-i)); 
    
    double payoff = cfrbr_abs(ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, 
                              chanceReach, phase, updatePlayer); 
   
    moveEVs[action] = payoff; 
    stratEV += moveProb*payoff; 
  }

  // post-traversals: update the infoset
  double myreach = (player == 1 ? reach1 : reach2); 
  double oppreach = (player == 1 ? reach2 : reach1); 

  if (phase == 1 && player == updatePlayer) // regrets
  {
    // update the abstract information sets

    for (int a = 0; a < actionshere; a++)
    {
      is.cfr[a] += (chanceReach*oppreach)*(moveEVs[a] - stratEV); 
      //cfrcache[infosetkey][a] += (chanceReach*oppreach)*(moveEVs[a] - stratEV);
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
    //ttlUpdates++;
    issa.put(infosetkey, is, actionshere, 0); 
  }

  return stratEV;
}


void cfrbr_iter_noabs() 
{
  unsigned long long bidseq = 0;
  
  int updatePlayer = 3-brplayer; 
  assert(updatePlayer == 1 || updatePlayer == 2); 

  // start with computing the best responses for previous iteration
  // fixed player for BR = cfrplayer
  computeHalfBR(updatePlayer, false, false, true);
  
  GameState gs; 
  bidseq = 0; 
  cfrbr_noabs(gs, 1, 0, bidseq, 1.0, 1.0, 1.0, 1, updatePlayer);
}

void cfrbr_iter_abs() 
{
  #if 0
  unsigned long long bidseq = 0; 
  
  // import both strategies into the full information sets.
  iss.absImportStrategy(issa);

  computeHalfBR(1, false, false, true);
  //cout << "!"; cout.flush();
  
  computeHalfBR(2, false, false, true);
  //cout << "!"; cout.flush();

  GameState gs1; 
  bidseq = 0; 
  cfrbr_abs(gs1, 1, 0, bidseq, 1.0, 1.0, 1.0, 1, 1);
  //cout << "!"; cout.flush();
  
  GameState gs2; 
  bidseq = 0; 
  cfrbr_abs(gs2, 1, 0, bidseq, 1.0, 1.0, 1.0, 1, 2);
  #endif
}



int main(int argc, char ** argv)
{
  unsigned long long maxIters = 0; 
  bool useAbstraction = false;
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

    if (argc >= 3) { 
      string arg = argv[2];
      brplayer = to_int(arg); 
    }

    if (argc >= 4) { 
      useAbstraction = true;
      string absfilename = argv[3];
      cout << "Reading abstract infosets from " << absfilename << "..." << endl;
      issa.readFromDisk(absfilename);
    }

    if (argc >= 5) {
      maxIters = to_ull(argv[4]);
    }
  }  

  if (brplayer < 1) { 
    cerr << "Usage: cfrbr issfile [brplayer] [abstract issafile] [maxiters]" << endl;
    exit(-1);
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

  StopWatch stopwatch;
  double totaltime = 0; 

  cout << "Starting CFRBR iterations " << endl;

  for (; true; iter++)
  {
    
    if (useAbstraction)
      cfrbr_iter_abs();
    else
      cfrbr_iter_noabs();
    
    if (iter % 10 == 0)
    { 
      cout << "."; cout.flush(); 
      totaltime += stopwatch.stop();
      stopwatch.reset();
    }

    if (totaltime > nextCheckpoint)
    {
      cout << endl;

      cout << "total time: " << totaltime << " seconds." << endl; 
      cout << "Done iteration " << iter << endl;
      cout << "brplayer = " << brplayer << endl;

      cout << "Computing bounds... "; cout.flush(); 
      double b1 = 0.0, b2 = 0.0;
      iss.computeBound(b1, b2); 
      cout << " b1 = " << b1 << ", b2 = " << b2 << ", bound = " << (2.0*MAX(b1,b2)) << endl;

      //cout << "Evaluating strategies..."; cout.flush();
      //double ev = evaluate();
      //cout << " ev = " << ev << endl;
     
      if (useAbstraction) {
        cout << "Importing abstract strategy before BR." << endl;
        iss.absImportStrategy(issa);
      }

      // must do this offline as we need the conv from average strategy not current
      //double conv = computeBestResponses(false, false);      
      string reportfile = "cfrbr" + to_string(brplayer) + "report.txt";
      report(reportfile, totaltime, (2.0*MAX(b1,b2)), 0, 0, 0.0);
      string dumpprefix = "issbr" + to_string(brplayer);
      dumpInfosets(dumpprefix);
      cout << endl;
     
      nextCheckpoint += cpWidth;

      stopwatch.reset(); 
    }
    
    if (iter == maxIters) break;
  }
}
