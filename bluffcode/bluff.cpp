
/** 
 * Info about this game from a run of bluffcounter.cpp:
 *
 * storing 2 doubles per iapair + 2 doubles per infoset
 * want size of index to be at least double # infosets (then 2 doubles per index size)
 *
 * for 1,1:
 *   p1 infosets 12288, p2 infosets 12288, total: 24576
 *   infoset actions pairs, p1 p2 total = 24570 24570 49140
 *
 * for 2,1: 
 *   iapairs  5505003 1572858 7077861
 *   infosets 2752512 786432 3538944 
 *   (roughly 169 mb + 120 mb cache = 400 mb)
 *
 * for 2,2:
 *   iapairs  352321515 352321515 704643030
 *   infosets 176160768 176160768 352321536
 *   (roughly 17 GB + (2**30 * 8 = 8.6 GB) = 26 GB)
 *
 * for 3,1:
 *   iapairs  939524040 100663290 1040187330
 *   infosets 469762048 50331648 520093696
 *   (roughly 25 GB + (4 * #infosets = 16.6 GB) = 42 GB)
 *
 * for 3,2:
 *   iapairs  60129542088 22548578283 82678120371
 *   infosets 30064771072 11274289152 41339060224
 *   (roughly 1.3 TB for iapairs and (4 * infosets = 1.8 TB) = 3.1TB)
 */


#include <iostream>
#include <string>
#include <cstring>
#include <map>
#include <cassert>
#include <cstdlib>
#include <ctime>
#include <set>

#include "bluff.h"
#include "infosetstore.h"
#include "seqstore.h"
#include "sys/time.h"

#define LOC(b,r,c)  b[r*3 + c]

using namespace std; 

#if 0
 This has now been generalized
/*
 * Bid ranks with total number of dice in play 
 */
int bids2[] = { 11, 12, 13, 14, 15, 16, 21, 22, 23, 24, 25, 26 }; 

int bids3[] = { 11, 12, 13, 14, 15, 16, 
                21, 22, 23, 24, 25, 
                31, 32, 33, 34, 35, 26, 
                36 }; 

int bids4[] = { 11, 12, 13, 14, 15, 16, 
                21, 22, 23, 24, 25, 
                31, 32, 33, 34, 35, 26, 
                41, 42, 43, 44, 45, 
                36, 46 }; 

int bids5[] = { 11, 12, 13, 14, 15, 16, 
                21, 22, 23, 24, 25, 
                31, 32, 33, 34, 35, 26, 
                41, 42, 43, 44, 45, 
                51, 52, 53, 54, 55, 36, 
                46, 56 }; 

int bids6[] = { 11, 12, 13, 14, 15, 16, 
                21, 22, 23, 24, 25, 
                31, 32, 33, 34, 35, 26, 
                41, 42, 43, 44, 45, 
                51, 52, 53, 54, 55, 36, 
                61, 62, 63, 64, 65, 
                46, 56, 66 }; 

/*
 * Chance outcomes ranks based on number of number of dice for that player.
 */

int co1[] = { 1, 2, 3, 4, 5, 6 };

int co2[] = { 11, 12, 13, 14, 15, 16, 22, 23, 24, 25, 26, 33, 34, 35, 36, 
              44, 45, 46, 55, 56, 66 };

int co3[] = { 111, 112, 113, 114, 115, 116, 122, 123, 124, 125, 126, 133, 134, 135, 136, 144, 145, 146, 155, 156, 166, 
              222, 223, 224, 225, 226, 233, 234, 235, 236, 244, 245, 246, 255, 256, 266,
              333, 334, 335, 336, 344, 345, 346, 355, 356, 366, 
              444, 445, 446, 455, 456, 466, 
              555, 556, 566, 
              666 }; 
#endif

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
static int numChanceOutcomes1 = 0;
static int numChanceOutcomes2 = 0;
static double * chanceProbs1 = NULL;
static double * chanceProbs2 = NULL;
static int * chanceOutcomes1 = NULL;
static int * chanceOutcomes2 = NULL;
static int * bids = NULL;

static StopWatch stopwatch;

extern SequenceStore seqstore;

double getBoundMultiplier(string algorithm)
{
  if (algorithm == "cfrcs")
    return static_cast<double>(numChanceOutcomes1*numChanceOutcomes2);
  else
    return 1.0;
}

double getChanceProb(int player, int outcome) 
{
  int co = (player == 1 ? numChanceOutcomes1 : numChanceOutcomes2);
  //cout << "outcome = " << outcome << endl;
  assert(outcome-1 >= 0 && outcome-1 < co);
  double * cp = (player == 1 ? chanceProbs1 : chanceProbs2); 
  return cp[outcome-1];
}

int numChanceOutcomes(int player)
{
  return (player == 1 ? numChanceOutcomes1 : numChanceOutcomes2); 
}

#if 0
// Note: not constant time as most ranking functions should be. 
// But that is OK since it is only used upon initialization.
int rankco(int * roll, int outcomes)
{
  int num = 0; 

  assert(outcomes == 21 || outcomes == 56);
  int * co = (outcomes == 21 ? co2 : co3); 

  num = roll[0]*100 + roll[1]*10 + roll[2];

  int index = -1; 

  for (int i = 0; i < outcomes; i++) 
    if (co[i] == num) 
    {
      index = i;
      break;
    }
  
  if (index < 0) 
    cerr << "num = " << num << endl; 

  assert(index >= 0 && index < outcomes); 
  return index; 
}

void initChance(double * array, int size)
{
  if (size == 6) 
  {
    for (int i = 0; i < 6; i++) 
      array[i] = 1.0/6.0; 
  }
  else if (size == 21) 
  {
    int roll[3] = {0,0,0};

    for (int i = 1; i <= 6; i++)
      for (int j = 1; j <= 6; j++)
      {
        if (i < j)
        {
          roll[1] = i; 
          roll[2] = j; 
        }
        else 
        {
          roll[1] = j; 
          roll[2] = i; 
        }

        int index = rankco(roll, size);
        assert(index >= 0 && index < size);
        array[index] += 1.0; 
      }

    for (int i = 0; i < 21; i++)
      array[i] = array[i] / 36.0;
  }
  else if (size == 56)
  {
    int roll[3] = {0,0,0};

    for (int i = 1; i <= 6; i++)
      for (int j = 1; j <= 6; j++)
        for (int k = 1; k <= 6; k++)
        {
          roll[0] = i; 
          roll[1] = j; 
          roll[2] = k; 

          bubsort(roll, 3); 

          int index = rankco(roll, size);
          assert(index >= 0 && index < size);
          array[index] += 1.0; 
        }

    for (int i = 0; i < 56; i++)
      array[i] = array[i] / (6.0*6.0*6.0);
  }
  else {
    assert(false);
  }
}
#endif

void unrankco(int i, int * roll, int player)
{
  int num = 0; 
  int * chanceOutcomes = (player == 1 ? chanceOutcomes1 : chanceOutcomes2); 
  num = chanceOutcomes[i];

  assert(num > 0); 

  int numDice = (player == 1 ? P1DICE : P2DICE);

  for (int j = numDice-1; j >= 0; j--)
  {
    roll[j] = num % 10;
    num /= 10; 
  }
}


void initBids()
{
  bids = new int[BLUFFBID-1]; 
  int nextWildDice = 1; 
  int idx = 0; 
  for (int dice = 1; dice <= P1DICE + P2DICE; dice++)
  {
    for (int face = 1; face <= DIEFACES-1; face++) 
    { 
      bids[idx] = dice*10 + face; 
      idx++;
    }

    if (dice % 2 == 1) { 
      bids[idx] = nextWildDice*10 + DIEFACES; 
      idx++;
      nextWildDice++;
    }
  }

  for(; nextWildDice <= (P1DICE+P2DICE); nextWildDice++) { 
    bids[idx] = nextWildDice*10 + DIEFACES; 
    idx++;
  }

  assert(idx == BLUFFBID-1);

}

void getInfosetKey(GameState & gs, unsigned long long & infosetkey, int player, unsigned long long bidseq)
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

void getInfoset(GameState & gs, int player, unsigned long long bidseq, Infoset & is, unsigned long long & infosetkey, int actionshere)
{
  // when simultatig a game, get it from a diff place
  if (sg_curPlayer == 1) { 
    getInfosetKey(gs, infosetkey, player, bidseq);
    bool ret = sgiss1.get(infosetkey, is, actionshere, 0);
    assert(ret);
  }
  else if (sg_curPlayer == 2) { 
    getInfosetKey(gs, infosetkey, player, bidseq);
    bool ret = sgiss2.get(infosetkey, is, actionshere, 0);
    assert(ret);
  }
  else { 
    // normal case when using most algorithms, i.e. solving    
    getInfosetKey(gs, infosetkey, player, bidseq);
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

void nextRoll(int * roll, int size)
{
  for (int i = size-1; i >= 0; i--)
  {
    // Try to increment if by 1. 
    if (roll[i] < DIEFACES) { 
      // if possible, do it and then set everything to the right back to 1
      roll[i]++; 
      for (int j = i+1; j < size; j++) 
        roll[j] = 1; 

      return;
    }
  }
}

int getRollBase10(int * roll, int size) 
{
  int multiplier = 1; 
  int val = 0;
  for (int i = size-1; i >= 0; i--)
  {    
    val += roll[i]*multiplier;
    multiplier *= 10;
  }

  return val;
}

void determineChanceOutcomes(int player) 
{
  int dice = (player == 1 ? P1DICE : P2DICE); 
  int rolls[dice]; 
  for (int r = 0; r < dice; r++) rolls[r] = 1; 
  outcomes.clear(); 

  int permutations = intpow(DIEFACES, dice); 
  int p;

  for (p = 0; p < permutations; p++) { 

    // first, make a copy
    int rollcopy[dice]; 
    memcpy(rollcopy, rolls, dice*sizeof(int)); 

    // now sort
    bubsort(rollcopy, dice); 

    // now convert to an integer in base 10
    int key = getRollBase10(rollcopy, dice); 

    // now increment the counter for this key in the map
    outcomes[key] += 1; 

    // next roll
    nextRoll(rolls, dice); 
  }

  assert(p == permutations); 

  int & numChanceOutcomes = (player == 1 ? numChanceOutcomes1 : numChanceOutcomes2); 
  double* & chanceProbs = (player == 1 ? chanceProbs1 : chanceProbs2); 
  int* & chanceOutcomes = (player == 1 ? chanceOutcomes1 : chanceOutcomes2); 

  // now, transfer the map keys to the array 
  numChanceOutcomes = outcomes.size();
  chanceProbs = new double[numChanceOutcomes];
  chanceOutcomes = new int[numChanceOutcomes];

  map<int,int>::iterator iter;
  int idx = 0; 
  for (iter = outcomes.begin(); iter != outcomes.end(); iter++) {
    chanceOutcomes[idx] = iter->first;
    idx++;
  }

  bubsort(chanceOutcomes, numChanceOutcomes);

  for (int c = 0; c < numChanceOutcomes; c++) { 
    int key = chanceOutcomes[c];
    chanceProbs[c] = static_cast<double>(outcomes[key]) / static_cast<double>(permutations); 
    //cout << "player " << player << " roll " << key << " prob " << chanceProbs[c] << endl;
  }


}

void init()
{
  assert(bids == NULL); 

  cout << "Initializing Bluff globals..." << endl; 

  struct timeval tv;
  gettimeofday(&tv, NULL);
  srand48(tv.tv_usec);

  determineChanceOutcomes(1); 
  determineChanceOutcomes(2); 
  
  // iscWidth if the number of bits needed to encode the chance outcome in the integer
  int maxChanceOutcomes = (numChanceOutcomes1 > numChanceOutcomes2 ? numChanceOutcomes1 : numChanceOutcomes2); 
  iscWidth = ceiling_log2(maxChanceOutcomes); 

  initBids();

  cout << "Globals are: " << numChanceOutcomes1 << " " << numChanceOutcomes2 << " " << iscWidth << endl;

  #if FSICFR
  fsiInit();
  #endif
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
    is.weight[i] = 0.0;
  }
  
  #if FSICFR
  is.reach1 = 0;
  is.reach2 = 0;
  is.value = 0;

  #if FSIPCS
  for (int opco = 0; opco < MAXCO; opco++) {
    is.reaches1[opco] = 0.0;
    is.reaches2[opco] = 0.0;
    is.values[opco] = 0.0;
  }
  #endif
  #endif
}

bool terminal(GameState & gs)
{
  return (gs.curbid == BLUFFBID); 
}

// a bid is from 1 to 12, for example
void convertbid(int & dice, int & face, int bid)
{
  if (P1DICE == 1 && P2DICE == 1) 
  {
    dice = (bid - 1) / DIEFACES + 1;
    face = bid % DIEFACES;
    if (face == 0) face = DIEFACES; 

    //cout << dice << " " << face << endl;
    assert(dice >= 1 && dice <= 2);
    assert(face >= 1 && face <= DIEFACES);
  }
  else 
  {
    // stored in an array.
    int size = (P1DICE+P2DICE)*DIEFACES; 
    assert((bid-1) >= 0 && (bid-1) < size);
    
    dice = bids[bid-1] / 10;
    face = bids[bid-1] % 10;
  }
}

void getRoll(int * roll, int chanceOutcome, int player) 
{
  unrankco(chanceOutcome-1, roll, player);
}

int countMatchingDice(const GameState & gs, int player, int face)
{
  int roll[3] = {0,0,0};
  int matchingDice = 0;
  int dice = (player == 1 ? P1DICE : P2DICE);

  if (dice == 1)
  {
    /*if (player == 1)
      roll[1] = gs.p1roll;
    else if (player == 2)
      roll[1] = gs.p2roll;*/
    if (player == 1)
      roll[2] = gs.p1roll;
    else if (player == 2)
      roll[2] = gs.p2roll;
  }
  else if (dice > 1)
  {
    if (player == 1)
      unrankco(gs.p1roll-1, roll, 1);
    else if (player == 2)
      unrankco(gs.p2roll-1, roll, 2);
  }

  for (int i = 0; i < 3; i++) 
    if (roll[i] == face || roll[i] == DIEFACES)
      matchingDice++;

  return matchingDice;
}

int whowon(int bid, int bidder, int callingPlayer, int p1roll, int p2roll, int & delta)
{
  int dice = 0, face = 0;
  convertbid(dice, face, bid);

  assert(bidder != callingPlayer);

  // get the dice

  int p1rollArr[P1DICE]; 
  int p2rollArr[P2DICE];

  unrankco(p1roll-1, p1rollArr, 1); 
  unrankco(p2roll-1, p2rollArr, 2); 

  // now check the number of matches

  int matching = 0;

  for (int i = 0; i < P1DICE; i++)
    if (p1rollArr[i] == face || p1rollArr[i] == DIEFACES)
      matching++;
  
  for (int j = 0; j < P2DICE; j++) 
    if (p2rollArr[j] == face || p2rollArr[j] == DIEFACES)
      matching++;

  delta = matching - dice; 
  if (delta < 0) delta *= -1;

  if (matching >= dice)
  {
    return bidder; 
  }
  else
  {
    return callingPlayer;
  }
}

int whowon(GameState & gs, int & delta)
{
  int bidder = 3 - gs.callingPlayer;
  return whowon(gs.prevbid, bidder, gs.callingPlayer, gs.p1roll, gs.p2roll, delta); 
}

int whowon(GameState & gs)
{
  int bidder = 3 - gs.callingPlayer;
  int delta = 0;
  return whowon(gs.prevbid, bidder, gs.callingPlayer, gs.p1roll, gs.p2roll, delta); 
}

double payoff(int winner, int player, int delta)
{
  // according to my rules of liar's dice and the wikipedia entry of Dudo, 
  // the loser only loses one die.. not an amount equal to how much he is off by
  // Bluff on BSW says you lose what you're off by (or 1 if the delta is 0)

  // first thing: if it's an exact match, calling player loses 1 die
  // may as well set delta to 1 in this case 
  if (delta == 0) delta = 1; 

  double p1payoff = 0.0;

  if (P1DICE == 1 && P2DICE == 1) return (winner == player ? 1.0 : -1.0); 
  else if (P1DICE == 2 && P2DICE == 1) // 2,1 depends only on 1,1
  {
    if      (winner == 1) p1payoff = 1.0;                    // p1 wins and it's over
    else if (winner == 2 && delta == 1) p1payoff = -VAL11;   // p1 loses a die and then p2 starts a 1,1 game
    else if (winner == 2 && delta >= 2) p1payoff = -1.0;     // p1 loses and it's over
    else    { cerr << "Error in payoff function " << P1DICE << " " << P2DICE << endl; exit(-1); }
  }
  else if (P1DICE == 1 && P2DICE == 2) // 1,2 depends only on 1,1
  {
    if      (winner == 1 && delta == 1) p1payoff = VAL11;    // p1 wins and starts a 1,1 game
    else if (winner == 1 && delta >= 2) p1payoff = 1.0;      // p1 wins and it's over
    else if (winner == 2) p1payoff = -1.0;                   // p1 loses and it's over
    else    { cerr << "Error in payoff function " << P1DICE << " " << P2DICE << endl; exit(-1); }
  }
  else if (P1DICE == 2 && P2DICE == 2) // 2,2 depends on only on 2,1 
  {
    if      (winner == 1 && delta == 1) p1payoff = VAL21;    // p1 wins and p1 starts a 2,1 game
    else if (winner == 1 && delta >= 2) p1payoff = 1.0;      // p1 wins and it's over
    else if (winner == 2 && delta == 1) p1payoff = -VAL21;   // p1 loses so p2 starts a 2,1 game
    else if (winner == 2 && delta >= 2) p1payoff = -1.0;     // p1 loses and it's over
    else    { cerr << "Error in payoff function " << P1DICE << " " << P2DICE << endl; exit(-1); }
  }
  else if (P1DICE == 3 && P2DICE == 1) // 3,1 depends on 1,2 and 1,1
  {
    if      (winner == 1 && delta >= 1) p1payoff = 1.0;      // p1 wins and it's over
    else if (winner == 2 && delta == 1) p1payoff = -VAL12;   // p1 loses so p2 starts a 1,2 game
    else if (winner == 2 && delta == 2) p1payoff = -VAL11;   // p1 loses so p2 starts a 1,1 game
    else if (winner == 2 && delta >= 3) p1payoff = -1.0;     // p1 loses and it's over
    else    { cerr << "Error in payoff function " << P1DICE << " " << P2DICE << endl; exit(-1); }
  }
  else if (P1DICE == 1 && P2DICE == 3) // 1,3 depends on 1,2 and 1,1
  {
    if      (winner == 1 && delta == 1) p1payoff = VAL12;    // p1 wins and starts a 1,2 game
    else if (winner == 1 && delta == 2) p1payoff = VAL11;    // p1 wins and starts a 1,1 game
    else if (winner == 1 && delta >= 3) p1payoff = 1.0;      // p1 wins and it's over
    else if (winner == 2 && delta >= 1) p1payoff = -1.0;     // p1 loses and it's over
    else    { cerr << "Error in payoff function " << P1DICE << " " << P2DICE << endl; exit(-1); }
  }
  else if (P1DICE == 3 && P2DICE == 2)
  {
    if      (winner == 1 && delta == 1) p1payoff = VAL31;    // p1 wins and starts a 3,1 game
    else if (winner == 1 && delta >= 2) p1payoff = 1.0;      // p1 wins and it's over
    else if (winner == 2 && delta == 1) p1payoff = -VAL22;   // p2 wins and starts a 2,2 game
    else if (winner == 2 && delta == 2) p1payoff = -VAL21;   // p2 wins and starts a 2,1 game
    else if (winner == 2 && delta >= 3) p1payoff = -1.0;     // p2 wins and it's over
    else    { cerr << "Error in payoff function " << P1DICE << " " << P2DICE << endl; exit(-1); }
  }
  else if (P1DICE == 2 && P2DICE == 3)
  {
    if      (winner == 1 && delta == 1) p1payoff = VAL22;    // p1 wins and starts a 2,2 game
    else if (winner == 1 && delta == 2) p1payoff = VAL21;    // p1 wins and starts a 2,1 games
    else if (winner == 1 && delta >= 3) p1payoff = 1.0;      // p1 wins and it's over
    else if (winner == 2 && delta == 1) p1payoff = -VAL31;   // p2 wins and starts a 3,1 game
    else if (winner == 2 && delta >= 2) p1payoff = -1.0;     // p2 wins and it's over
    else    { cerr << "Error in payoff function " << P1DICE << " " << P2DICE << endl; exit(-1); }
  }
  else if (P1DICE == 4 && P2DICE == 1)
  {
    if      (winner == 1 && delta >= 1) p1payoff = 1.0;      // p1 wins and it's over
    else if (winner == 2 && delta == 1) p1payoff = -VAL13;   // p2 wins and starts a 1,3 game
    else if (winner == 2 && delta == 2) p1payoff = -VAL12;   // p2 wins and starts a 1,2 game
    else if (winner == 2 && delta == 3) p1payoff = -VAL11;   // p2 wins and starts a 1,1 game
    else if (winner == 2 && delta >= 4) p1payoff = -1.0;     // p2 wins and it's over
    else    { cerr << "Error in payoff function " << P1DICE << " " << P2DICE << endl; exit(-1); }
  }
  else if (P1DICE == 1 && P2DICE == 4)
  {
    if      (winner == 1 && delta == 1) p1payoff = VAL13;    // p1 wins and starts a 1,3 game
    else if (winner == 1 && delta == 2) p1payoff = VAL12;    // p1 wins and starts a 1,2 game
    else if (winner == 1 && delta == 3) p1payoff = VAL11;    // p1 wins and starts a 1,1 game
    else if (winner == 1 && delta >= 4) p1payoff = 1.0;      // p1 wins and it's over
    else if (winner == 2 && delta >= 1) p1payoff = -1.0;     // p2 wins and it's over
    else    { cerr << "Error in payoff function " << P1DICE << " " << P2DICE << endl; exit(-1); }
  }
  else if (P1DICE == 3 && P2DICE == 3)
  {
  }
  else
  {
    assert(false);
  }
   
  return (player == 1 ? p1payoff : -p1payoff); 
}

// Returns payoff for Liar's Dice (delta always equal to 1)
double payoff(int winner, int player)
{
  return payoff(winner, player, 1); 
}

// this is the function called by all the algorithms.
// Now set to use the delta
double payoff(GameState & gs, int player)
{
  int delta = 0;
  int winner = whowon(gs, delta); 
  return payoff(winner, player, delta); 
}

double payoff(int bid, int bidder, int callingPlayer, int p1roll, int p2roll, int player)
{
  int delta = 0;
  int winner = whowon(bid, bidder, callingPlayer, p1roll, p2roll, delta); 
  //cout << "winner = " << winner << endl;
  return payoff(winner, player); 
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

void dumpSeqStore(string prefix)
{
  string filename = filepref + prefix + "." + to_string(iter) + ".dat";
  cout << "Dumping seqstore to " + filename + " ... " << endl;
  seqstore.dumpToDisk(filename);
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



void initInfosets(GameState & gs, int player, int depth, unsigned long long bidseq)
{
  if (terminal(gs))
    return;

  // check for chance nodes
  if (gs.p1roll == 0) 
  {
    for (int i = 1; i <= numChanceOutcomes1; i++) 
    {
      GameState ngs = gs; 
      ngs.p1roll = i; 

      initInfosets(ngs, player, depth+1, bidseq); 
    }

    return;
  }
  else if (gs.p2roll == 0)
  {
    for (int i = 1; i <= numChanceOutcomes2; i++)
    {
      GameState ngs = gs; 
      ngs.p2roll = i; 

      initInfosets(ngs, player, depth+1, bidseq); 
    }

    return;
  }

  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 

  assert(actionshere > 0);
  Infoset is;
  newInfoset(is, actionshere); 

  for (int i = gs.curbid+1; i <= maxBid; i++) 
  {
    if (depth == 2 && i == (gs.curbid+1)) {       
      cout << "InitTrees. iss stats = " << iss.getStats() << endl;
      //cout << "InitTrees, d2 1r" << gs.p1roll << " 2r" << gs.p2roll << " i" << i << endl;
    }

    GameState ngs = gs; 
    ngs.prevbid = gs.curbid;
    ngs.curbid = i; 
    ngs.callingPlayer = player;
    unsigned long long newbidseq = bidseq; 
    newbidseq |= (1ULL << (BLUFFBID-i)); 

    initInfosets(ngs, (3-player), depth+1, newbidseq); 
  }

  unsigned infosetkey = 0; 
  infosetkey = bidseq;  
  infosetkey <<= iscWidth; 
  if (player == 1)
  {
    infosetkey |= gs.p1roll; 
    infosetkey <<= 1; 
    iss.put(infosetkey, is, actionshere, 0); 
  }
  else if (player == 2)
  {
    infosetkey |= gs.p2roll; 
    infosetkey <<= 1; 
    infosetkey |= 1; 
    iss.put(infosetkey, is, actionshere, 0); 
  }
}

void initSeqStore(GameState & gs, int player, int depth, unsigned long long bidseq)
{
  if (terminal(gs))
    return;

  // check for chance nodes
  if (gs.p1roll == 0) 
  {
    for (int i = 1; i <= numChanceOutcomes1; i++) 
    {
      GameState ngs = gs; 
      ngs.p1roll = i; 

      initSeqStore(ngs, player, depth+1, bidseq); 
    }

    return;
  }
  else if (gs.p2roll == 0)
  {
    for (int i = 1; i <= numChanceOutcomes2; i++)
    {
      GameState ngs = gs; 
      ngs.p2roll = i; 

      initSeqStore(ngs, player, depth+1, bidseq); 
    }

    return;
  }

  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 

  assert(actionshere > 0);
  Infoset is;
  newInfoset(is, actionshere); 

  for (int i = gs.curbid+1; i <= maxBid; i++) 
  {
    if (depth == 2 && i == (gs.curbid+1)) {       
      cout << "InitTrees. iss stats = " << iss.getStats() << endl;
      //cout << "InitTrees, d2 1r" << gs.p1roll << " 2r" << gs.p2roll << " i" << i << endl;
    }

    GameState ngs = gs; 
    ngs.prevbid = gs.curbid;
    ngs.curbid = i; 
    ngs.callingPlayer = player;
    unsigned long long newbidseq = bidseq; 
    newbidseq |= (1ULL << (BLUFFBID-i)); 

    initSeqStore(ngs, (3-player), depth+1, newbidseq); 
  }

  /*unsigned infosetkey = 0; 
  infosetkey = bidseq;  
  infosetkey <<= iscWidth; 
  if (player == 1)
  {
    infosetkey |= gs.p1roll; 
    infosetkey <<= 1; 
    iss.put(infosetkey, is, actionshere, 0); 
  }
  else if (player == 2)
  {
    infosetkey |= gs.p2roll; 
    infosetkey <<= 1; 
    infosetkey |= 1; 
    iss.put(infosetkey, is, actionshere, 0); 
  }
  */

  SSEntry ssentry; 
  ssentry.actionshere = actionshere;

  unsigned long long sskey = getSSKey(player, bidseq);
  
  unsigned int nco = (player == 1 ? numChanceOutcomes(1) : numChanceOutcomes(2)); 
  for (unsigned int co = 1; co <= nco; co++)
  {
    Infoset & is = ssentry.infosets[co-1]; 
    newInfoset(is, actionshere); 
  }

  seqstore.put(sskey, ssentry, actionshere, 0); 
}

void initInfosets()
{
  unsigned long long bidseq = 0;

  GameState gs;

  cout << "Initialize info set store..." << endl;
  // # doubles in total, size of index (must be at least # infosets)
  // 2 doubles per iapair + 2 per infoset = 
  
  if (P1DICE == 1 && P2DICE == 1 && DIEFACES == 6)
    iss.init(196572, 100000);  // ADA
    // iss.init(147432, 100000);  // testing ADA
  else if (P1DICE == 1 && P2DICE == 1 && DIEFACES == 7)
    iss.init(688100, 460000); 
  else if (P1DICE == 1 && P2DICE == 1 && DIEFACES == 8)
    iss.init(3145696, 2100000); 
  else if (P1DICE == 1 && P2DICE == 1 && DIEFACES == 9)
    iss.init(14155740, 9500000); 
  else if (P1DICE == 1 && P2DICE == 1 && DIEFACES == 10)
    iss.init(62914520, 42000000); 
  else if (P1DICE == 1 && P2DICE == 1 && DIEFACES == 11)
    iss.init(276824020, 184550000); 
  else if (P1DICE == 1 && P2DICE == 1 && DIEFACES == 12)
    iss.init(1207959504, 805310000); 
  else if (P1DICE + P2DICE == 3 && DIEFACES == 6)  // 1,2 or 2,1
    iss.init(21233610, 15000000); 
  else if (P1DICE == 2 && P2DICE == 2 && DIEFACES == 6)  // 2,2
    iss.init(2113929132ULL, 1073741824ULL); 
  else if (P1DICE == 3 && P2DICE == 1 && DIEFACES == 6)  // 3,1 = 55GB
    iss.init(3120562052ULL, 2080374784ULL);             
  else if (P1DICE == 1 && P2DICE == 3 && DIEFACES == 6)  // 1,3 = 55GB
    iss.init(3120562052ULL, 2080374784ULL);             
  else 
  {
    cerr << "initInfosets not defined for this PXDICE + DIEFACES" << endl;
  }

  assert(iss.getSize() > 0); 

  cout << "Initializing info sets..." << endl;
  stopwatch.reset(); 
  initInfosets(gs, 1, 0, bidseq);

  cout << "time taken = " << stopwatch.stop() << " seconds." << endl;
  iss.stopAdding(); 

  cout << "Final iss stats: " << iss.getStats() << endl;
  stopwatch.reset();
  
  // used for increasing die faces experiments
  //string filename = filepref + "iss-bluff"+ to_string(P1DICE) + "" + to_string(P2DICE)
  //  + "d" + to_string(DIEFACES) + ".initial.dat";

  string filename = filepref + "iss.initial.dat";

  cout << "Dumping information sets to " << filename << endl;
  iss.dumpToDisk(filename);
}

void initSeqStore()
{
  /*unsigned long long bidseq = 0;
  GameState gs;

  cout << "Initialize info set store..." << endl;*/

  assert(false);
}


