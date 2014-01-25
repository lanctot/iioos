
/** 
 * Info about this game from a run of goofcounter.cpp:
 *
 * storing 2 doubles per iapair + 2 doubles per infoset
 * want size of index to be at least double # infosets (then 2 doubles per index size)
 *
 *   Goof(4): size 102942 infosets 23116
 */


#include <iostream>
#include <string>
#include <cstring>
#include <map>
#include <cassert>
#include <cstdlib>
#include <ctime>
#include <set>

#include "goof.h"
#include "infosetstore.h"
#include "sys/time.h"

#define LOC(b,r,c)  b[r*3 + c]

using namespace std; 

// global variables 
InfosetStore iss;
string filepref = "scratch/"; 
unsigned long long iter;
bool useAbstraction = false; 
unsigned long long totalUpdates = 0;
int iscWidth = 0;
// reasonable timings: 5s, 60s, 1200s for 11, 21, 22
double cpWidth = 10.0;             
double nextCheckpoint = cpWidth;
unsigned long long nodesTouched = 0;
unsigned long long ntNextReport = 1000000;  // nodes touched base timing     
unsigned long long ntMultiplier = 2;  // nodes touched base timing     
double sumEV1 = 0.0;
double sumEV2 = 0.0;

// used in simulated games (see sim.cpp)
bool simgame = false;
double timeLimit = 1.0;
int sg_curPlayer = 0;
// infoset stores used during search
InfosetStore sgiss1;   
InfosetStore sgiss2;   
// infoset stores used during full-stitching
InfosetStore fsiss1; 
InfosetStore fsiss2; 

// the amount of randomness to add to RM 
double randMixRM = 0.01;

// key is roll, value is # of time it shows up. Used only when determining chance outcomes
map<int,int> outcomes; 

// probability of this chance move. indexed as 0-5 or 0-20
/*static int numChanceOutcomes1 = 0;
static int numChanceOutcomes2 = 0;
static double * chanceProbs1 = NULL;
static double * chanceProbs2 = NULL;
static int * chanceOutcomes1 = NULL;
static int * chanceOutcomes2 = NULL;
static int * bids = NULL;*/

static StopWatch stopwatch;

void getInfosetKey(GameState & gs, unsigned long long & infosetkey, int player, unsigned long long chanceseq,
                   unsigned long long outcomeseq, unsigned long long bidseq1, unsigned long long bidseq2)
{
  if (player == 1)
  {
    infosetkey = bidseq1;  
    infosetkey <<= (NUMCARDS*iscWidth);
    infosetkey |= chanceseq;
    infosetkey <<= (NUMCARDS*2);
    infosetkey |= outcomeseq;
    infosetkey <<= 1; 
  }
  else if (player == 2)
  {
    infosetkey = bidseq2;  
    infosetkey <<= (NUMCARDS*iscWidth);
    infosetkey |= chanceseq;
    infosetkey <<= (NUMCARDS*2);
    infosetkey |= outcomeseq;
    infosetkey <<= 1; 
    infosetkey |= 1; 
  }
}

void getInfoset(GameState & gs, int player, unsigned long long chanceseq, unsigned long long outcomeseq, 
                unsigned long long bidseq1, unsigned long long bidseq2, 
                Infoset & is, unsigned long long & infosetkey, int actionshere)
{
  // when simultatig a game, get it from a diff place
  if (sg_curPlayer == 1) { 
    getInfosetKey(gs, infosetkey, player, chanceseq, outcomeseq, bidseq1, bidseq2);
    bool ret = sgiss1.get(infosetkey, is, actionshere, 0);
    assert(ret);
  }
  else if (sg_curPlayer == 2) { 
    getInfosetKey(gs, infosetkey, player, chanceseq, outcomeseq, bidseq1, bidseq2);
    bool ret = sgiss2.get(infosetkey, is, actionshere, 0);
    assert(ret);
  }
  else { 
    // normal case when using most algorithms, i.e. solving    
    getInfosetKey(gs, infosetkey, player, chanceseq, outcomeseq, bidseq1, bidseq2);
    bool ret = iss.get(infosetkey, is, actionshere, 0);
    assert(ret);
  }
}


int ceiling_log2(int val)
{
  int exp = 1, num = 2; 
  do { 
    if (num > val) { return exp; }
    num *= 2; 
    exp++;
  }
  while (true); 
}

int intpow(int x, int y)
{
  if (y == 0) return 1; 
  return x * intpow(x, y-1); 
}

void init()
{
  cout << "Initializing Goof globals..." << endl; 

  struct timeval tv;
  gettimeofday(&tv, NULL);
  srand48(tv.tv_usec);

  iscWidth = ceiling_log2(NUMCARDS); 

  cout << "Globals are: " << NUMCARDS << " " << iscWidth << endl;
}


void newInfoset(Infoset & is, int actions)
{
  is.actionshere = actions; 
  is.lastUpdate = 0;

  for (int i = 0; i < actions; i++)
  {
    is.cfr[i] = 0.0;
    is.totalMoveProbs[i] = 0.0;
    is.curMoveProbs[i] = 1.0 / actions;
  }
}

bool terminal(GameState & gs)
{
  return (gs.turn >= NUMCARDS); 
}

int whowon(GameState & gs)
{
  //return whowon(gs.prevbid, bidder, gs.callingPlayer, gs.p1roll, gs.p2roll, delta); 
  return 0;
}

double payoff(int winner, int player)
{
  double p1payoff = 0.0;
  return (winner == player ? 1.0 : -1.0); 
}

void report(std::string filename, double totaltime, double bound, unsigned long long updates, unsigned long long leafevals, double conv)
{
  report(filename, totaltime, bound, updates, leafevals, conv, 0);
}

void report(string filename, double totaltime, double bound, unsigned long long updates, unsigned long long leafevals, double conv, double avgRT12plus)
{
  filename = filepref + filename; 
  cout << "Reporting to " + filename + " ... " << endl; 
  ofstream outf(filename.c_str(), ios::app); 
  outf << iter << " " << totaltime << " " << bound << " " << conv << " "
       << nodesTouched << " " << avgRT12plus << endl;
  outf.close(); 
}

void dumpInfosets(string prefix)
{
  string filename = filepref + prefix + "." + to_string(iter) + ".dat";
  cout << "Dumping infosets to " + filename + " ... " << endl;
  iss.dumpToDisk(filename);
}

void dumpMetaData(string prefix, double totaltime) 
{
  string filename = filepref + prefix + "." + to_string(iter) + ".dat";
  cout << "Dumping metadeta to " + filename + " ... " << endl;

  ofstream outf(filename.c_str(), ios::binary); 
  if (!outf.is_open()) {
    cerr << "Could not open meta data file for writing." << endl; 
    return; 
  }

  // ntNextReport *= ntMultiplier
  outf.write(reinterpret_cast<const char *>(&iter), sizeof(iter)); 
  outf.write(reinterpret_cast<const char *>(&nodesTouched), sizeof(nodesTouched)); 
  outf.write(reinterpret_cast<const char *>(&ntNextReport), sizeof(ntNextReport)); 
  outf.write(reinterpret_cast<const char *>(&ntMultiplier), sizeof(ntMultiplier)); 
  outf.write(reinterpret_cast<const char *>(&totaltime), sizeof(totaltime)); 

  outf.close();
}

void loadMetaData(std::string filename) 
{
  ifstream inf(filename.c_str(), ios::binary);
  if (!inf.is_open()) { 
    cerr << "Could not open meta data file." << endl;
    return;
  }

  double totaltime = 0;

  inf.read(reinterpret_cast<char *>(&iter), sizeof(iter)); 
  inf.read(reinterpret_cast<char *>(&nodesTouched), sizeof(nodesTouched)); 
  inf.read(reinterpret_cast<char *>(&ntNextReport), sizeof(ntNextReport)); 
  inf.read(reinterpret_cast<char *>(&ntMultiplier), sizeof(ntMultiplier)); 
  inf.read(reinterpret_cast<char *>(&totaltime), sizeof(totaltime)); 

  inf.close();
}

void applyBid(GameState & gs, int bid, int player, unsigned long long & outcomeseq, unsigned long long & bidseq1, unsigned long long & bidseq2) { 
  // bid is nonzero
  
  bool * myhand = (player == 1 ? gs.p1hand : gs.p2hand);
  int * mybids = (player == 1 ? gs.p1bids : gs.p2bids);
  unsigned long long & mybidseq = (player == 1 ? bidseq1 : bidseq2); 

  myhand[bid-1] = false; 
  mybids[gs.turn] = bid; 
  mybidseq <<= iscWidth; 
  mybidseq |= (bid-1); 

  if (player == 2) { 
    outcomeseq <<= 2; 

    if (gs.p1bids[gs.turn] > gs.p2bids[gs.turn]) 
      outcomeseq |= 1; 
    else if (gs.p2bids[gs.turn] > gs.p1bids[gs.turn])
      outcomeseq |= 2; 
    else 
      outcomeseq |= 3; 

    gs.turn++;
  }
}

void initInfosets(GameState & gs, int player, int depth, unsigned long long chanceseq, unsigned long long outcomeseq, 
                  unsigned long long bidseq1, unsigned long long bidseq2)
{
  if (terminal(gs))
    return;

  // chance?
  if (gs.chance[gs.turn] == 0) 
  {
    for (int i = 0; i <= NUMCARDS; i++) 
    {
      if (gs.deck[i]) {
        GameState ngs = gs; 
        
        ngs.deck[i] = false; 
        ngs.chance[ngs.turn] = i+1; 
        chanceseq <<= iscWidth;
        chanceseq |= i;

        initInfosets(ngs, player, depth+1, chanceseq, outcomeseq, bidseq1, bidseq2); 
      }  
      
    }

    return;
  }

  int actionshere = NUMCARDS-gs.turn; 

  assert(actionshere > 0);
  Infoset is;
  newInfoset(is, actionshere); 
  bool * myhand = (player == 1 ? gs.p1hand : gs.p2hand); 
  int * mybids = (player == 1 ? gs.p1bids : gs.p2bids); 

  for (int i = 0; i < NUMCARDS; i++) 
  {
    if (depth == 1) {       
      cout << "InitTrees. iss stats = " << iss.getStats() << endl;
      //cout << "InitTrees, d2 1r" << gs.p1roll << " 2r" << gs.p2roll << " i" << i << endl;
    }

    if (!myhand[i]) 
      continue;

    unsigned long long new_bidseq1 = bidseq1; 
    unsigned long long new_bidseq2 = bidseq2; 
    unsigned long long new_outcomeseq = outcomeseq;

    GameState ngs = gs; 
    applyBid(ngs, i+1, player, new_outcomeseq, new_bidseq1, new_bidseq2); 

    initInfosets(ngs, (3-player), depth+1, chanceseq, new_outcomeseq, new_bidseq1, new_bidseq2); 
  }

  unsigned long long infosetkey = 0; 

  //void getInfosetKey(GameState & gs, unsigned long long & infosetkey, int player, unsigned long long chanceseq,
  //                 unsigned long long outcomeseq, unsigned long long bidseq1, unsigned long long bidseq2)

  getInfosetKey(gs, infosetkey, player, chanceseq, outcomeseq, bidseq1, bidseq2);
  iss.put(infosetkey, is, actionshere, 0); 
}

void initInfosets()
{
  unsigned long long chanceseq = 0, outcomeseq = 0, bidseq1 = 0, bidseq2 = 0;
  GameState gs;

  cout << "Initialize info set store..." << endl;

  if (NUMCARDS == 4) 
    iss.init(150000, 125000);  
  else if (NUMCARDS == 5)
    iss.init(10000000, 10000000);  
  else { 
    cerr << "initInfosets not defined for this NUMCARDS" << endl;
  }

  assert(iss.getSize() > 0); 

  cout << "Initializing info sets..." << endl;
  stopwatch.reset(); 

  initInfosets(gs, 1, 0, chanceseq, outcomeseq, bidseq1, bidseq2);

  cout << "time taken = " << stopwatch.stop() << " seconds." << endl;
  iss.stopAdding(); 

  cout << "Final iss stats: " << iss.getStats() << endl;
  stopwatch.reset();
  
  string filename = filepref + "iss.initial.dat";

  cout << "Dumping information sets to " << filename << endl;
  iss.dumpToDisk(filename);
}

