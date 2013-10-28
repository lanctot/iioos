
/* 
 * Now have 2 doubles per infoset action + 2 doubles per infoset
 * 
 * (should make space for 4x infosets for fast hashing)
 *
 * 1,1 with recall = 4
 *
 *   total IA pairs = 19020
 *   total infosets = 6744
 *   memory usage of original (see absInitInfosets below): 35%
 *
 * For the next few, the format is: total-iapairs total-doubles 4*total-infosets (total-mem)
 *
 * 2,2 with recall = 1     12600 27216 4032               (283   KB)
 * 2,2 with recall = 2     97608 218484 46536             (  2.5 MB)
 * 2,2 with recall = 3     543900 1259832 344064          ( 15.0 MB)
 * 2,2 with recall = 4     2329068 5573988 1831704        ( 73.9 MB)
 * 2,2 with recall = 5     7982100 19706568 7484736       (277.4 MB)
 * 2,2 with recall = 6     22518468 57258852 24443832     (849.2 MB)
 * 2,2 with recall = 8     108323418 290648316 148002960  (  4.7 GB)
 * 2,2 with recall = 10    295534218 829815420 477493968  ( 14.3 GB)
 *
 * 3,2 with recall = 7
 *
 *   ia pairs p1 p2 total = 484788416 181795656 666584072
 *   infosets p1 p2 total = 122214176 45830316 168044492
 *   bytes is index total = 13354057024 10754847488 24108904512
 *   25 GB (assuming index = 4X infosets)
 *  
 * 3,2 with recall = 8 (not possible on grex)
 *
 *   ia pairs p1 p2 total = 1285988816 482245806 1768234622
 *   infosets p1 p2 total = 362574296 135965361 498539657
 *   bytes is index total = 36268388464 31906538048 68174926512
 *
 * 3,3 with recall = 5
 * 
 *   ia pairs p1 p2 total = 133923720 133923720 267847440
 *   infosets p1 p2 total = 21513408 21513408 43026816
 *   bytes is index total = 4973988096 2753716224 7727704320
 *   (7.7 GB assuming 4X index)
 *
 * 3,3 with recall = 6
 *
 *   ia pairs p1 p2 total = 601393800 601393800 1202787600
 *   infosets p1 p2 total = 112410368 112410368 224820736
 *   bytes is index total = 22841733376 14388527104 37230260480
 *   (37.2 GB assuming 4X index)
 */

#include <iostream>
#include <cassert>

#include "bluff.h"

int RECALL = 0;              // now passed in as cmdline arg!
bool absAvgFullISS = false;  // use the full infoset store for the average strategy

using namespace std; 

// used to properly compute the average strategy
InfosetStore issfull; 

static StopWatch stopwatch;
static int routcome = 0;

unsigned long long absConvertKey(unsigned long long fullkey)
{
  unsigned long long bidseq = fullkey >> (1+iscWidth);

  unsigned long long pbit = fullkey & 1;
  unsigned long long cbits = (fullkey >> 1) & (pow2(iscWidth) - 1);

  unsigned long long bit = 1; 
  int matches = 0; 
  int b;
  for (b = 0; b < BLUFFBID; b++) {
    unsigned long long mask = bidseq & bit; 
    if (mask > 0) 
      matches++;

    if (matches >= RECALL)
      break;

    bit <<= 1; 
  }
  
  unsigned absbidseq = 0; 
  if (matches < RECALL || b >= BLUFFBID)
    absbidseq = bidseq; 
  else
    absbidseq = bidseq & (pow2(b+1)-1);
  
  unsigned long long abskey = absbidseq << iscWidth; 
  abskey |= cbits;
  abskey <<= 1;  
  abskey |= pbit; 

  return abskey;
}

void getAbsInfosetKey(GameState & gs, unsigned long long & infosetkey, int player,  
                      unsigned long long bidseq, int chanceOutcome)
{
  infosetkey = 0; 

  unsigned long long bit = 1; 
  int matches = 0; 
  int b;
  for (b = 0; b < BLUFFBID; b++) {
    unsigned long long mask = bidseq & bit; 
    if (mask > 0) 
      matches++;

    if (matches >= RECALL)
      break;

    bit <<= 1; 
  }
  
  unsigned absbidseq = 0; 
  if (matches < RECALL || b >= BLUFFBID)
    absbidseq = bidseq; 
  else
    absbidseq = bidseq & (pow2(b+1)-1);

  infosetkey = absbidseq << iscWidth; 

  // replace chance outcome by one specified if it's > 0
  if (chanceOutcome > 0) 
    infosetkey |= chanceOutcome;
  else
    infosetkey |= (player == 1 ? gs.p1roll : gs.p2roll); 

  infosetkey <<= 1; 
  if (player == 2) infosetkey |= 1; 
}

void getAbsSSKey(GameState & gs, unsigned long long & sskey, int player, unsigned long long bidseq)
{
  // looks up the last RECALL bids and masks out only those 
  sskey = 0; 

  unsigned long long bit = 1; 
  int matches = 0; 
  int b;
  for (b = 0; b < BLUFFBID; b++) {
    unsigned long long mask = bidseq & bit; 
    if (mask > 0) 
      matches++;

    if (matches >= RECALL)
      break;

    bit <<= 1; 
  }
  
  unsigned absbidseq = 0; 
  if (matches < RECALL || b >= BLUFFBID)
    absbidseq = bidseq; 
  else
    absbidseq = bidseq & (pow2(b+1)-1);

  sskey = absbidseq; 

  // replace chance outcome by one specified if it's > 0
  /*if (chanceOutcome > 0) 
    infosetkey |= chanceOutcome;
  else
    infosetkey |= (player == 1 ? gs.p1roll : gs.p2roll); */

  sskey <<= 1; 
  if (player == 2) sskey |= 1; 
}

unsigned long long getAbsSSKey(bool player1, unsigned int * sequence)
{
  //newbidseq |= (1 << (BLUFFBID-i));
  unsigned long long key = 0;
  for (int j = 0; j < RECALL; j++) {
    unsigned int bid = sequence[j]; 

    if (bid > 0) 
      key |= (1ULL << (BLUFFBID-bid));
  }

  key <<= 1; 
  if (!player1) key |= 1ULL;

  return key;
}

void absInitInfosets(GameState & gs, int player, int depth, unsigned long long bidseq)
{
  if (terminal(gs))
    return;

  // check for chance nodes
  if (gs.p1roll == 0) 
  {
    for (int i = 1; i <= P1CO; i++) 
    {
      GameState ngs = gs; 
      ngs.p1roll = i; 

      absInitInfosets(ngs, player, depth+1, bidseq); 
    }

    return;
  }
  else if (gs.p2roll == 0)
  {
    for (int i = 1; i <= P2CO; i++)
    {
      GameState ngs = gs; 
      ngs.p2roll = i; 

      absInitInfosets(ngs, player, depth+1, bidseq); 
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
      cout << "InitTrees: " << routcome << ". iss stats = " << iss.getStats() << endl;
      routcome++;
      //cout << "InitTrees, d2 1r" << gs.p1roll << " 2r" << gs.p2roll << " i" << i << endl;
    }

    GameState ngs = gs; 
    ngs.prevbid = gs.curbid;
    ngs.curbid = i; 
    ngs.callingPlayer = player;
    unsigned long long newbidseq = bidseq; 
    newbidseq |= (1ULL << (BLUFFBID-i)); 

    absInitInfosets(ngs, (3-player), depth+1, newbidseq); 
  }

  unsigned long long infosetkey = 0; 
  getAbsInfosetKey(gs, infosetkey, player, bidseq);
  iss.put(infosetkey, is, actionshere, 0); 
}

void absInitInfosets()
{
  unsigned long long bidseq = 0;

  GameState gs;

  cout << "Initialize info set store..." << endl;

  // # doubles in total, size of index (must be at least # infosets)
  // 2 doubles per iapair + 2 per infoset = 
 
  //  *   ia pairs p1 p2 total = 1164534 1164534 2329068
  // *   infosets p1 p2 total = 228963 228963 457926

  /*
 * 2,2 with recall = 1     12600 27216 4032               (283   KB)
 * 2,2 with recall = 2     97608 218484 46536             (  2.5 MB)
 * 2,2 with recall = 3     543900 1259832 344064          ( 15.0 MB)
 * 2,2 with recall = 4     2329068 5573988 1831704        ( 73.9 MB)
 * 2,2 with recall = 5     7982100 19706568 7484736       (277.4 MB)
 * 2,2 with recall = 6     22518468 57258852 24443832     (849.2 MB)
 * 2,2 with recall = 8     108323418 290648316 148002960  (  4.7 GB)
 * 2,2 with recall = 10    295534218 829815420 477493968  ( 14.3 GB)
 */

  // 24600
  // 804
  // 144
  if (P1DICE == 1 && P2DICE == 1 && RECALL == 10)
    iss.init(147384, 100000); 
  else if (P1DICE == 1 && P2DICE == 1 && RECALL == 4)
    iss.init(51528, 100000); 
  else if (P1DICE == 1 && P2DICE == 1 && RECALL == 3)
    iss.init(24600, 100000); 
  else if (P1DICE == 1 && P2DICE == 1 && RECALL == 2)
    iss.init(8760, 100000); 
  else if (P1DICE == 1 && P2DICE == 1 && RECALL == 1)
    iss.init(2160, 100000); 
  else if (P1DICE == 2 && P2DICE == 2 && RECALL == 1)
    iss.init(27216, 4032); 
  else if (P1DICE == 2 && P2DICE == 2 && RECALL == 2)
    iss.init(218484, 46536); 
  else if (P1DICE == 2 && P2DICE == 2 && RECALL == 3)
    iss.init(1259832, 344064); 
  else if (P1DICE == 2 && P2DICE == 2 && RECALL == 4)
    iss.init(5573988, 1831704); 
  else if (P1DICE == 2 && P2DICE == 2 && RECALL == 5)
    iss.init(19706568, 7484736); 
  else if (P1DICE == 2 && P2DICE == 2 && RECALL == 6)
    iss.init(57258852, 24443832); 
  else if (P1DICE == 2 && P2DICE == 2 && RECALL == 8)
    iss.init(290648316ULL, 148002960ULL); 
  else if (P1DICE == 2 && P2DICE == 2 && RECALL == 10)
    iss.init(829815420ULL, 477493968ULL); 
  else if (P1DICE == 3 && P2DICE == 2 && RECALL == 7)
    iss.init(1669257128ULL, 672177968ULL);
  else if (P1DICE == 3 && P2DICE == 3 && RECALL == 6)
    iss.init(2855216672ULL, 899282944ULL);
  else 
  {
    assert(false);
  }

  assert(iss.getSize() > 0);

  cout << "Initializing info sets..." << endl;
  stopwatch.reset(); 
  absInitInfosets(gs, 1, 0, bidseq);

  cout << "time taken = " << stopwatch.stop() << " seconds." << endl;
  iss.stopAdding(); 

  cout << "Final iss stats: " << iss.getStats() << endl;
  stopwatch.reset();
  
  string filename = filepref + "issa-r" + to_string(RECALL) + ".initial.dat";
  cout << "Dumping information sets to " << filename << endl;
  iss.dumpToDisk(filename);
}

