
#ifndef __GOOF_H__
#define __GOOF_H__

#include <sys/timeb.h>

#include <cmath>
#include <string>
#include <limits>
#include <map>
#include <set>
#include <list>
#include <vector>

#include "infosetstore.h"

// Notes:
//   Some 

#define NUMCARDS 5

#include "defs.h"

// definitions of structures etc.

struct GameState
{
  bool deck[NUMCARDS];      // true means it's still in the deck
  bool p1hand[NUMCARDS];    // true = card left in hand
  bool p2hand[NUMCARDS];    // true = card left in hand
  int chance[NUMCARDS];     // cards that have appeared, in order (nonzero)
  int p1bids[NUMCARDS];     // bid cards played, in order (nonzero)
  int p2bids[NUMCARDS];     // bid cards played, in order (nonzero)
  int turn;

  GameState() 
  {
    turn = 0; 
    for (int i = 0; i < NUMCARDS; i++) {
      deck[i] = true; 
      p1hand[i] = true;
      p2hand[i] = true;
      chance[i] = 0; 
      p1bids[i] = 0; 
      p2bids[i] = 0; 
    }
  }

};

struct Infoset
{
  // indices of these arrays correspond to the
  // same index when getting the move list;
  // Note: when using MCTS cfr[i] is the total/accumulated reward
  //       and totalMoveProbs[i] is the number of times it has been taken
  double cfr[NUMCARDS];                 
  double totalMoveProbs[NUMCARDS];
  double curMoveProbs[NUMCARDS];

  int actionshere; 
  unsigned long long lastUpdate;

};

// global forward declarations
class InfosetStore;

// game-specific function defs
bool terminal(GameState & gs); 
double payoff(GameState & gs, int player);
double payoff(int bid, int bidder, int callingPlayer, int p1roll, int p2roll, int player);
int whowon(GameState & gs);
int whowon(GameState & gs, int & delta);
int whowon(int bid, int bidder, int callingPlayer, int p1roll, int p2roll, int & delta);
void init();
double getChanceProb(int player, int outcome); 
void convertbid(int & dice, int & face, int bid);
int countMatchingDice(const GameState & gs, int player, int face);
void getRoll(int * roll, int chanceOutcome, int player);  // currently array must be size 3 (may contain 0s)
int numChanceOutcomes(int player);

// util function defs
std::string to_string(double i);
std::string to_string(unsigned long long i);
std::string to_string(int i);
int to_int(std::string str);
unsigned long long to_ull(std::string str);
double to_double(std::string str);
void getSortedKeys(std::map<int,bool> & m, std::list<int> & kl);
void split(std::vector<std::string> & tokens, const std::string line, char delimiter);
unsigned long long pow2(int i);
void bubsort(int * array, int size);
std::string infosetkey_to_string(unsigned long long infosetkey);
bool replace(std::string& str, const std::string& from, const std::string& to);
std::string getCurDateTime(); 

// solver-specific function defs
void newInfoset(Infoset & is, int actionshere);
void getInfoset(GameState & gs, int player, unsigned long long bidseq, Infoset & is, unsigned long long & infosetkey, int actionshere); 
void initInfosets();
void initSeqStore();
void allocSeqStore(); 
double computeBestResponses(bool abs, bool avgFix);
double computeBestResponses(bool abs, bool avgFix, double & p1value, double & p2value);
void report(std::string filename, double totaltime, double bound, unsigned long long updates, unsigned long long leafevals, double conv);
void report(std::string filename, double totaltime, double bound, unsigned long long updates, unsigned long long leafevals, double conv, double avgRT12plus);
void dumpInfosets(std::string prefix);
void dumpSeqStore(std::string prefix);
void dumpMetaData(std::string prefix, double totaltime);
void loadMetaData(std::string file);
double getBoundMultiplier(std::string algorithm);
double evaluate();
unsigned long long absConvertKey(unsigned long long fullkey);
double computeHalfBR(int fixed_player, bool abs, bool avgFix, bool _cfrbr);
void setBRTwoFiles(); 
void estimateValue();
void loadUCTValues(Infoset & is, int actions); 
void saveUCTValues(Infoset & is, int actions); 
void UCTBR(int fixed_player);
void fsiBR(int fixed_player);
double absComputeBestResponses(bool abs, bool avgFix, double & p1value, double & p2value);
double absComputeBestResponses(bool abs, bool avgFix);
double searchComputeHalfBR(int fixed_player, InfosetStore * _issPtr, bool _ismcts);

// abstract versions of stuff
void absInitInfosets();
void getInfosetKey(GameState & gs, unsigned long long & infosetkey, int player, unsigned long long chanceseq,
                   unsigned long long outcomeseq, unsigned long long bidseq1, unsigned long long bidseq2);
void getAbsInfosetKey(GameState & gs, unsigned long long & infosetkey, int player, unsigned long long bidseq, int chanceOutcome = -1);
void brImportAbsStrategy(std::string file);
void getAbsSSKey(GameState & gs, unsigned long long & sskey, int player, unsigned long long bidseq);
unsigned long long getAbsSSKey(bool player1, unsigned int * sequence);

// sampling
void sampleMoveAvg(Infoset & is, int actionshere, int & index, double & prob);
void sampleChanceEvent(int player, int & outcome, double & prob);
void sampleMoveAvg(Infoset & is, int actionshere, int & index, double & prob);
int sampleAction(int player, Infoset & is, int actionshere, double & sampleprob, double epsilon, bool firstTimeUniform);
int sampleActionBiased(int player, Infoset & is, int actionshere, double & sampleprob, double epsilon, 
                       double oos_gamma, int action);

// FSI functions
void fsiInit();
void fsiInitInfosets(); 
void fsiInitSeqStore();  
unsigned long long fsiGetInfosetKey(bool p1, unsigned int co, unsigned int * sequence);
unsigned long long fsiGetSSKey(bool p1, unsigned int * sequence);
bool addOne(unsigned int * bidseq);
bool subOne(unsigned int * bidseq);
void initSequenceForward(unsigned int * sequence);
void initSequenceBackward(unsigned int * sequence);
void fsiTestAddSub();
double fsiAvgStratEV(bool player1, unsigned int * sequence, int p1roll, int p2roll);

// global variables
extern InfosetStore iss; 
extern InfosetStore briss2;
extern InfosetStore evaliss2;
extern int iscWidth;       // number of bits for chance outcome
extern unsigned long long iter;
extern std::string filepref;
extern bool useAbstraction;
extern unsigned long long totalUpdates; 
extern double cpWidth;                   // used for timing
extern double nextCheckpoint;            // used for timing
extern unsigned long long ntNextReport;  // used for timing
extern unsigned long long ntMultiplier;  // used for timing
extern unsigned long long nodesTouched; 
extern double sumEV1;
extern double sumEV2;

// for search-based players
extern bool simgame;
extern int sg_curPlayer; 
extern double timeLimit; 
extern InfosetStore sgiss1;
extern InfosetStore sgiss2;
extern InfosetStore fsiss1; 
extern InfosetStore fsiss2; 
extern double randMixRM; 
extern int p1type; 
extern int p2type;
extern int br_stitchingPlayer; 
extern int stitchedInfosets;
double simloop();
int getMove(int player, GameState gs, unsigned long long bidseq);
int getMoveKeyboard(int player, GameState gs, unsigned long long bidseq);
int getMoveStrat(int player, GameState gs, unsigned long long bidseq);
int getMoveMCTS(int player, GameState gs, unsigned long long bidseq); 
int getMoveOOS(int player, GameState gs, unsigned long long bidseq);
int getMoveRandom(int player, GameState gs, unsigned long long bidseq);


class StopWatch
{
  timeb tstart, tend;
public:
  StopWatch() { ftime(&tstart); }
  void reset() { ftime(&tstart); }
  double stop()
  {
    ftime(&tend);
    return ((tend.time*1000 + tend.millitm)
            - (tstart.time*1000 + tstart.millitm) ) / 1000.0;
  }
};


#endif

