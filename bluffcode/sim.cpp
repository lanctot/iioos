
#include <cassert>
#include <iostream>
#include <cstdlib>

#include "bluff.h"

using namespace std; 

// 0 = human, 1 = mcts ai, 2 = load from strategy file
static int p1type = 1;
static int p2type = 1;
//static unsigned long long simulations = 10000000;
static double C = 2.0;

static InfosetStore iss2; 

double getMoveMCTS_rec(int player, GameState & gs, unsigned long long bidseq, int topPlayer, int & takeAction)
{
  //cout << "bidseq = " << bidseq << endl;

  if (terminal(gs))
  {
    //cout << "terminal!" << endl;
    return payoff(gs, topPlayer);
  }

  unsigned long long infosetkey = 0; 
  Infoset is; 
  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 

  // first get the parent infoset
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

  takeAction = -1; 

  // check if there are any with 0 visits
  int unvisited[actionshere]; 
  int ua = 0; 

  for (int i = 0; i < actionshere; i++)
  {
    if (is.totalMoveProbs[i] == 0.0) 
    {
      unvisited[ua] = i;
      ua++; 
    }
  }

  if (ua > 0) 
  {
    // this still happens fairly often even at high sims
    //cout << "*"; cout.flush();
    int index = static_cast<int>(drand48() * ua); 
    takeAction = unvisited[index];
    assert(takeAction >= 0 && takeAction < actionshere); 
  }
  else
  {
    double maxval = -1000.0;
    int maxact = -1; 
    assert(is.lastUpdate >= static_cast<unsigned int>(actionshere));
    
    for (int i = 0; i < actionshere; i++) 
    {
      assert(is.totalMoveProbs[i] > 0.0); 
      double val =   (is.cfr[i] / is.totalMoveProbs[i]) 
                   + C * sqrt( log(static_cast<double>(is.lastUpdate)) / is.totalMoveProbs[i] ); 

      if (val > maxval) {
        maxval = val; 
        maxact = i;
      }
    }

    assert(maxact >= 0); 
    takeAction = maxact;
  }

  //cout << "takeAction = " << takeAction << endl;

  // take the action, modify the state
  
  int bid = gs.curbid + 1 + takeAction;
  gs.prevbid = gs.curbid;
  gs.curbid = bid; 
  gs.callingPlayer = player;
  bidseq |= (1ULL << (BLUFFBID-bid)); 

  int dummy = 0;
  double tppayoff = getMoveMCTS_rec(3-player, gs, bidseq, topPlayer, dummy);
  double mypayoff = (player == topPlayer ? tppayoff : -tppayoff); 

  assert(takeAction >= 0 && takeAction < actionshere); 

  is.cfr[takeAction] += mypayoff; 
  is.totalMoveProbs[takeAction] += 1.0; 
  is.lastUpdate++; 

  iss.put(infosetkey, is, actionshere, 0); 

  return tppayoff;
}

int getMoveMCTS(int player, GameState gs, unsigned long long bidseq)
{
  int roll = (player == 1 ? gs.p1roll : gs.p2roll); 
  cout << "Player: " << player << ", Roll: " << roll << endl << "Running MCTS simulations: ";

  unsigned long long pisk = 0; 
  Infoset pis; 
  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 

  // first get the parent infoset
  pisk = bidseq;  
  pisk <<= iscWidth; 
  
  if (player == 1)
  {
    pisk |= gs.p1roll; 
    pisk <<= 1; 
    bool ret = iss.get(pisk, pis, actionshere, 0); 
    assert(ret);
  }
  else if (player == 2)
  {
    pisk |= gs.p2roll; 
    pisk <<= 1; 
    pisk |= 1; 
    bool ret = iss.get(pisk, pis, actionshere, 0); 
    assert(ret);
  }

  // start the simulations
  int takeAction;

  double totaltime = 0.0;
  StopWatch stopwatch;

  //for (unsigned long long i = 0; i < simulations; i++)
  for (unsigned long long i = 0; true; i++)
  {
    if (i % 10000 == 0) 
    { 
      totaltime += stopwatch.stop();
      stopwatch.reset(); 
      cout << "."; cout.flush(); 
    }

    if (totaltime > nextCheckpoint)
    {
      cout << endl << "total time: " << totaltime << " seconds. ";
      //iter = i;
      //return 0;

      InfosetStore iss_save;
      iss.copy(iss_save);
  
      //iss.mctsToCFR_mixed();
      iss.mctsToCFR_pure();

      double conv = computeBestResponses(false, false);
      report("mctspreport.txt", totaltime, conv, 0, 0);

      iss_save.copy(iss);
      
      iter = i;
      dumpInfosets("issmcts");

      cout << endl;
      nextCheckpoint += cpWidth;

      stopwatch.reset(); 
    }


    GameState gscopy = gs; 

    // ONLY when solving!
    int myoc = 0; double myprob = 0.0;
    sampleChanceEvent(player, myoc, myprob);

    // sample the opponent's chance outcome
    int outcome = 0;
    double prob = 0;
    sampleChanceEvent(3-player, outcome, prob);

    if (player == 1)
    {
      gscopy.p1roll = myoc;
      gscopy.p2roll = outcome;
    }
    else if (player == 2)
    {
      gscopy.p1roll = outcome;
      gscopy.p2roll = myoc;
    }

    takeAction = -1;
    getMoveMCTS_rec(player, gscopy, bidseq, player, takeAction);

    //if (i % (simulations / 100) == 0) { cout << "."; cout.flush(); }

  }

  cout << endl;

  // use the last takeAction
  assert(takeAction >= 0 && takeAction < actionshere); 
  
  return (gs.curbid + 1 + takeAction);
}


int getMoveKeyboard(int player, GameState gs, unsigned long long bidseq)
{
  int seq[BLUFFBID] = {0};
  int j = 0;
  for (int i = 0; i < BLUFFBID; i++)
  {
    if ((bidseq & (1ULL << (BLUFFBID-i))) > 0ULL) {
      seq[j] = i; 
      j++;
    }
  }

  int roll = (player == 1 ? gs.p1roll : gs.p2roll); 
  cout << "Player: " << player << ", Roll: " << roll << ", Bid: ";
  for (int i = 0; i < j; i++) {
    int dice, face;
    convertbid(dice, face, seq[i]); 
    cout << dice << "-" << face << " ";
  }
  cout << endl; 

  cout << "Move? "; cout.flush();

  int move;
  cin >> move;
  return move;
}

int getMoveStrat(int player, GameState gs, unsigned long long bidseq)
{
  unsigned long long pisk = 0; 
  Infoset pis; 
  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 

  // first get the parent infoset
  pisk = bidseq;  
  pisk <<= iscWidth; 
  
  if (player == 1)
  {
    pisk |= gs.p1roll; 
    pisk <<= 1; 
    bool ret = iss.get(pisk, pis, actionshere, 0); 
    assert(ret);
  }
  else if (player == 2)
  {
    pisk |= gs.p2roll; 
    pisk <<= 1; 
    pisk |= 1; 
    bool ret = iss2.get(pisk, pis, actionshere, 0); 
    assert(ret);
  }

  int index = -1;
  double prob = 0;
  sampleMoveAvg(pis, actionshere, index, prob);

  assert(index >= 0);
  CHKPROBNZ(prob);

  return (gs.curbid + 1 + index);
}

int getMove(int player, GameState gs, unsigned long long bidseq)
{
  int ptype = (player == 1 ? p1type : p2type); 

  switch(ptype)
  {
    case 0: return getMoveKeyboard(player, gs, bidseq); 
    case 1: return getMoveMCTS(player, gs, bidseq); 
    case 2: return getMoveStrat(player, gs, bidseq); 
    default: cerr << "Error, unknown ptype" << endl; exit(-1);
  }
}

double simloop()
{
  unsigned long long bidseq = 0; 
    
  GameState gs;
  bidseq = 0; 
  int player = 1; 
  double p1prob, p2prob;
    
  sampleChanceEvent(1, gs.p1roll, p1prob);
  sampleChanceEvent(2, gs.p2roll, p2prob);

  do 
  {
    int move = getMove(player, gs, bidseq);
    //cout << "move chosen: " << move << endl;
    assert(move >= 1 && move <= 13); // 1,1
    
    GameState ngs = gs; 
    ngs.prevbid = gs.curbid;
    ngs.curbid = move; 
    ngs.callingPlayer = player;
    bidseq |= (1ULL << (BLUFFBID-move)); 
    player = 3-player;

    gs = ngs;
  }  
  while(!terminal(gs));

  //int winner = whowon(gs);
  //cout << "Winner: " << winner << endl;
  
  return payoff(gs, 1);
}

void solve_mcts() 
{
  cout << "Solving using MCTS ... " << endl; 

  cout << "Clearing infoset store..." << endl;
  iss.clear();
  
  //for (int i = 1; i <= 6; i++)
  //for (int j = 1; j <= 6; j++)
  {
    unsigned long long bidseq = 0; 
    
    GameState gs;
    bidseq = 0; 
    int player = 1; 
  
    //int o1 = -1; double prob;
    //sampleChanceEvent(1, o1, prob);
    //gs.p1roll = i; 
    //gs.p2roll = j;

    // when solving, need bogus entries
    gs.p1roll = 1; 
    gs.p2roll = 1; 

    //int o1 = -1, o2 = -1;
    //sampleChanceEvent(1, int & outcome, double & prob);

    //cout << i << " " << j << endl;

    getMove(player, gs, bidseq);
  }

  //cout << "Converting to CFR strategy ... " << endl;
  //iss.mctsToCFR_mixed();
  //iss.mctsToCFR_pure();

  //iter = simulations;
  //dumpInfosets("issmcts");
}

int main(int argc, char ** argv)
{
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
    {
      //simulations = to_ull(argv[2]);
      //nextCheckpoint = to_double(argv[2]);
    
      string filename2 = argv[2];
      cout << "Reading the infosets from " << filename2 << "..." << endl;
      iss2.readFromDisk(filename2);
    }
  }  

  solve_mcts();
  /*
  double sum = 0.0;
  int sims = 1000000;

  for (int i = 0; i < sims; i++)
  {
    double payoff = simloop(); 
    sum += payoff;
    //cout << "payoff = " << payoff << endl;
  }

  cout << "avg payoff to first player = " << sum/sims << endl;
  */
}
  
