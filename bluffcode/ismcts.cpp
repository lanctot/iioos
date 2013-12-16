
#include <cassert>
#include <iostream>
#include <cstdlib>

#include "bluff.h"

using namespace std; 

static double C = 2.0;

static InfosetStore iss2; 

double getMoveMCTS_rec(int player, GameState & gs, unsigned long long bidseq, int topPlayer, bool treeMode)
{
  //cout << "bidseq = " << bidseq << endl;

  if (terminal(gs))
  {
    //cout << "terminal!" << endl;
    double util = payoff(gs, topPlayer);
    //cout << "util = " << util << ", topPlayer = " << topPlayer << endl;
    return util;
  }
  else if (!treeMode) { 
    // playout
    int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
    int actionshere = maxBid - gs.curbid; 

    // take a random move 
    int takeAction = static_cast<int>(drand48() * actionshere); 

    int bid = gs.curbid + 1 + takeAction;
    gs.prevbid = gs.curbid;
    gs.curbid = bid; 
    gs.callingPlayer = player;
    bidseq |= (1ULL << (BLUFFBID-bid)); 

    return getMoveMCTS_rec(3-player, gs, bidseq, topPlayer, treeMode); 
  }
  
  unsigned long long infosetkey = 0; 
  Infoset is; 
  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 

  getInfoset(gs, player, bidseq, is, infosetkey, actionshere);

  if (treeMode && is.lastUpdate == 0) { 
    // expansion case 
    treeMode = false; 
    //is.lastUpdate = iter;  // stores parent visits
  }

  int takeAction = -1; 

  // check if there are any with 0 visits
  int candidates[actionshere]; 
  int ua = 0; 

  for (int i = 0; i < actionshere; i++)
  {
    if (is.totalMoveProbs[i] == 0.0) 
    {
      candidates[ua] = i;
      ua++; 
    }
  }

  
  if (ua > 0) 
  {
    // there were some actions that have never been chosen
    // choose among those first
  }
  else
  {
    double maxval = -1000.0;
    double tolerance = 0.0000001; 
    assert(is.lastUpdate >= static_cast<unsigned int>(actionshere));
    
    for (int i = 0; i < actionshere; i++) 
    {
      assert(is.totalMoveProbs[i] > 0.0); 
      double val =   (is.cfr[i] / is.totalMoveProbs[i]) 
                   + C * sqrt( log(static_cast<double>(is.lastUpdate)+1) / is.totalMoveProbs[i] ); 

      if (val >= (maxval+tolerance)) {
        // clear better choice
        maxval = val; 
        candidates[0] = i;
        ua = 1; 
      }
      else if (val >= (maxval-tolerance) && val < (maxval+tolerance)) { 
        // tie, add this as a candidate
        candidates[ua] = i; 
        ua++; 
      }
    }

    assert(ua > 0); 
  }

  // choose among the candidates
  int index = static_cast<int>(drand48() * ua); 
  takeAction = candidates[index];
  assert(takeAction >= 0 && takeAction < actionshere); 

  // take the action, modify the state
  
  int bid = gs.curbid + 1 + takeAction;
  gs.prevbid = gs.curbid;
  gs.curbid = bid; 
  gs.callingPlayer = player;
  bidseq |= (1ULL << (BLUFFBID-bid)); 

  double tppayoff = getMoveMCTS_rec(3-player, gs, bidseq, topPlayer, treeMode);
  double mypayoff = (player == topPlayer ? tppayoff : -tppayoff); 

  //cout << "player " << player << ", tpp = " << tppayoff << ", myp = " << mypayoff << endl;

  assert(takeAction >= 0 && takeAction < actionshere); 

  is.cfr[takeAction] += mypayoff; 
  is.totalMoveProbs[takeAction] += 1.0; 
  is.lastUpdate++; 

  //cout << "is.lastUpdate = " << is.lastUpdate << endl;

  if (sg_curPlayer == 1)  
    sgiss1.put(infosetkey, is, actionshere, 0); 
  else 
    sgiss2.put(infosetkey, is, actionshere, 0); 

  return tppayoff;
}

int getMoveMCTS(int player, GameState match_gs, unsigned long long match_bidseq)
{
  int myroll = (player == 1 ? match_gs.p1roll : match_gs.p2roll); 
  cout << "Player: " << player << ", My Roll is: " << myroll << endl << "Running MCTS simulations: ";

  // determine which information set store to use based on the match player 
  //InfosetStore & myiss = (sg_curPlayer == 1 ? sgiss1 : sgiss2); 

  //unsigned long long infosetkey = 0; 
  //Infoset pis; 
  //int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  //int actionshere = maxBid - gs.curbid; 

  // root infoset
  //getInfoset(match_gs, player, match_bidseq, is, infosetkey, actionshere);

  // start the simulations
  int takeAction;

  double totaltime = 0.0;
  StopWatch stopwatch;

  iter = 0; 

  //for (unsigned long long i = 0; i < simulations; i++)
  for (unsigned long long i = 1; true; i++)
  {
    //cout << "Simulation " << i << endl; 

    totaltime = stopwatch.stop();
    if (totaltime >= timeLimit) break; 
    

    // Old code! ~circa 2010
    //
    //InfosetStore iss_save;
    //iss.copy(iss_save);
    //iss.mctsToCFR_mixed();
    //iss.mctsToCFR_pure();

    //double conv = computeBestResponses(false, false); FIXME
    //report("mctspreport.txt", totaltime, conv, 0, 0); FIXME
    //iss_save.copy(iss);
    
    iter = i;

    //dumpInfosets("issmcts");

    //cout << endl;
    //nextCheckpoint += cpWidth;

    //stopwatch.reset(); 
    
    // ONLY when solving!
    //int myoc = 0; double myprob = 0.0;
    //sampleChanceEvent(player, myoc, myprob);

    GameState gscopy = match_gs; 

    // determinize! sample the opponent's chance outcome
    int outcome = 0;
    double prob = 0;
    sampleChanceEvent(3-player, outcome, prob);

    if (player == 1)
      gscopy.p2roll = outcome;
    else if (player == 2)
      gscopy.p1roll = outcome;

    //cout << "determinized outcome for opponent = " << outcome << endl;

    takeAction = -1;
    getMoveMCTS_rec(player, gscopy, match_bidseq, player, true);
  }

  cout << "Total simulations: " << iter << endl;

  // use the last takeAction
  //assert(takeAction >= 0 && takeAction < actionshere); 
  
  // final move selection; highest value
  unsigned long long infosetkey = 0; 
  Infoset is; 
  int maxBid = (match_gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - match_gs.curbid; 

  getInfoset(match_gs, player, match_bidseq, is, infosetkey, actionshere);
  
  int candidates[actionshere]; 
  int ua = 0; 

  for (int i = 0; i < actionshere; i++)
  {
    if (is.totalMoveProbs[i] == 0.0) 
    {
      candidates[ua] = i;
      ua++; 
    }
  }

  assert(ua == 0); 
  
  double maxval = -1000.0;
  double tolerance = 0.0000001; 
    
  for (int i = 0; i < actionshere; i++) 
  {
    double val = (is.cfr[i] / is.totalMoveProbs[i]); 

    cout << "move " << i; 
    if (match_gs.curbid + 1 + i == BLUFFBID) cout << " (bluff)"; 
    else { 
      int quantity = 0;
      int face = 0;
      convertbid(quantity, face, match_gs.curbid + 1 + i);
      cout << " (" << quantity << "-" << face << ")"; 
    }
    cout << " = " << is.cfr[i] << " " << is.totalMoveProbs[i] << " " << val << endl;

    if (val >= (maxval+tolerance)) {
      // clear better choice
      maxval = val; 
      candidates[0] = i;
      ua = 1; 
    }
    else if (val >= (maxval-tolerance) && val < (maxval+tolerance)) { 
      // tie, add this as a candidate
      candidates[ua] = i; 
      ua++; 
    }
  }

  assert(ua > 0);

  // choose among the candidates
  int index = static_cast<int>(drand48() * ua); 
  takeAction = candidates[index];
  assert(takeAction >= 0 && takeAction < actionshere); 
  
  return (match_gs.curbid + 1 + takeAction);
}


void solve_mcts() 
{
  cout << "Solving using MCTS ... " << endl; 

  cout << "Clearing infoset store..." << endl;
  iss.clear();
  
  //for (int i = 1; i <= 6; i++)
  //for (int j = 1; j <= 6; j++)
  {
    //unsigned long long bidseq = 0;   FIXME
    
    //GameState gs;
    //bidseq = 0; 
    //int player = 1; FIXME
  
    //int o1 = -1; double prob;
    //sampleChanceEvent(1, o1, prob);
    //gs.p1roll = i; 
    //gs.p2roll = j;

    // when solving, need bogus entries
    //gs.p1roll = 1; 
    //gs.p2roll = 1; 

    //int o1 = -1, o2 = -1;
    //sampleChanceEvent(1, int & outcome, double & prob);

    //cout << i << " " << j << endl;

    // FIXME: must re-anable!!
    // getMove(player, gs, bidseq);
  }

  //cout << "Converting to CFR strategy ... " << endl;
  //iss.mctsToCFR_mixed();
  //iss.mctsToCFR_pure();

  //iter = simulations;
  //dumpInfosets("issmcts");
}

