
/* 
 * In FSI we 2 doubles per infoset action + 4 doubles per infoset
 */

#include <cassert>
#include <iostream>
#include <cstdlib>

#include "bluff.h"
#include "seqstore.h"
#include "statcounter.hpp"

using namespace std;

extern int RECALL;

static StopWatch stopwatch;

unsigned int bidWidth = 0;
unsigned long long initCount = 0;

SequenceStore seqstore; 

// turns a sequence of ints into a sequence of n-bit numbers
// not using this as a key. using getAbsSSKey in abstraction.cpp
unsigned long long encodeSequence(unsigned int * sequence)
{
  unsigned long long seq = 0;

  for (int i = 0; i < RECALL; i++) {
    assert(sequence[i] <= BLUFFBID-1);
    seq <<= bidWidth;
    seq |= static_cast<unsigned long long>(sequence[i]);
  }

  return seq;
}

// not using this as a key. using getAbsSSKey in abstraction.cpp
unsigned long long fsiGetSSKey(bool p1, unsigned int * sequence)
{
  unsigned long long key = encodeSequence(sequence); 
  key <<= 1; 
  if (!p1) { 
    key |= 1ULL;  
  }
  return key; 
}

// not using this as a key. using getAbsSSKey in abstraction.cpp
unsigned long long fsiGetInfosetKey(bool p1, unsigned int co, unsigned int * sequence)
{
  unsigned long long key = encodeSequence(sequence); 
  key <<= iscWidth;
  key |= static_cast<unsigned long long>(co); 
  key <<= 1; 
  if (!p1) { 
    key |= 1ULL;  
  }
  return key; 
}

#if FSICFR

// this is the old version that used the ISS
void fsiInitInfosets(bool player1, unsigned int * sequence)
{
  // create all the information sets with this sequence
  int curbid = sequence[RECALL-1];

  if (curbid == BLUFFBID)
    return;
  
  unsigned int maxBid = (curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - curbid; 

  initCount++; 
  if (initCount % 1000000 == 0) { 
    cout << "fsiInitInfosets, count = " << initCount << endl;
    cout << "fsiInitInfosets, current sequence is: ";
    for (int j = 0; j < RECALL; j++) cout << sequence[j] << " ";
    cout << "; stats: " << iss.getStats() << endl; 
  }
  
  unsigned int nco = (player1 ? numChanceOutcomes(1) : numChanceOutcomes(2)); 
  for (unsigned int co = 1; co <= nco; co++)
  {
    unsigned long long infosetkey = fsiGetInfosetKey(player1, co, sequence); 

    Infoset is;
    newInfoset(is, actionshere); 

    //cout << "adding infoset with player1 = " << player1 << ", co = " << co << " (isk = " << infosetkey << ")" << endl;
    
    iss.put(infosetkey, is, actionshere, 0);
  }

  // now recurse over all children
  
  unsigned int oldestbid = sequence[0];
    
  // left-shift to make space for new bid
  for (int j = 0; j < RECALL-1; j++) { sequence[j] = sequence[j+1]; }

  for (unsigned int i = curbid+1; i <= maxBid; i++) 
  {
    sequence[RECALL-1] = i; 
    fsiInitInfosets(!player1, sequence); 
  }

  // put the bid back normal (right-shift)
  
  for (int j = RECALL-1; j >= 1; j--) { sequence[j] = sequence[j-1]; }
  sequence[0] = oldestbid;

}

// This one recursively traverses the game tree. It's slow for the big games.
void fsiInitSeqStore(bool player1, unsigned int * sequence)
{
  // create all the information sets with this sequence
  int curbid = sequence[RECALL-1];

  if (curbid == BLUFFBID)
    return;
  
  unsigned int maxBid = (curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - curbid; 

  initCount++; 
  if (initCount % 1000000 == 0) { 
    cout << "fsiInitInfosets, count = " << initCount << ". ";
    //cout << "fsiInitSeqStore, current sequence is: ";
    //for (int j = 0; j < RECALL; j++) cout << sequence[j] << " ";
    cout << "Stats: " << seqstore.getStats() << endl; 
  }

  SSEntry ssentry; 
  ssentry.actionshere = actionshere;

  //unsigned long long sskey = fsiGetSSKey(player1, sequence);
  unsigned long long sskey = getAbsSSKey(player1, sequence);
  
  unsigned int nco = (player1 ? numChanceOutcomes(1) : numChanceOutcomes(2)); 
  for (unsigned int co = 1; co <= nco; co++)
  {
    //unsigned long long infosetkey = fsiGetInfosetKey(player1, co, sequence); 

    Infoset & is = ssentry.infosets[co-1]; 
    newInfoset(is, actionshere); 

    //cout << "adding infoset with player1 = " << player1 << ", co = " << co << " (isk = " << infosetkey << ")" << endl;
    
    //iss.put(infosetkey, is, actionshere, 0);
  }

  seqstore.put(sskey, ssentry, actionshere, 0); 

  // now recurse over all children
  
  unsigned int oldestbid = sequence[0];
    
  // left-shift to make space for new bid
  for (int j = 0; j < RECALL-1; j++) { sequence[j] = sequence[j+1]; }

  for (unsigned int i = curbid+1; i <= maxBid; i++) 
  {
    sequence[RECALL-1] = i; 
    fsiInitSeqStore(!player1, sequence); 
  }

  // put the bid back normal (right-shift)
  
  for (int j = RECALL-1; j >= 1; j--) { sequence[j] = sequence[j-1]; }
  sequence[0] = oldestbid;

}

// This one is fast, traverses all possible sequences. 
// Seems to not work for 6 total dice.. not sure why
void fsiInitSeqStore(unsigned int * sequence)
{
  initSequenceForward(sequence); 
  bool pass;

  do { 
  
    int curbid = sequence[RECALL-1];
    int maxBid = (curbid == 0 ? BLUFFBID-1 : BLUFFBID);
    int actionshere = maxBid - curbid; 
    
    unsigned long long sskey = 0; 
  
    initCount++; // this one is for sequences, so keep at 10^5
    if (initCount % 100000 == 0) { 
      cout << "fsiInitInfosets, count = " << initCount << ". ";
      //cout << "fsiInitSeqStore, current sequence is: ";
      //for (int j = 0; j < RECALL; j++) cout << sequence[j] << " ";
      cout << "Stats: " << seqstore.getStats() << endl; 
    }

    SSEntry ssentry; 
    ssentry.actionshere = actionshere;

    bool gtw = true; 
    int parity = 0;
    if (sequence[0] <= 1) { 
      gtw = false;
      for (int j = RECALL-1; j >= 0 && sequence[j] > 0; j--) 
        parity++; 
    }

    int players[2] = { 0, 0 }; 
    if (gtw) { 
      players[0] = 1;
      players[1] = 2;
    }
    else
    {
      if (parity % 2 == 0) players[0] = 1; 
      else players[0] = 2; 
    }

    for (int p = 0; p < 2; p++)
    {
      if (players[p] == 0) continue;

      int player = players[p];

      if (p == 0) 
        sskey = getAbsSSKey(player == 1, sequence);
      else 
        sskey = sskey | 1ULL; 
    
      unsigned int nco = (player == 1 ? numChanceOutcomes(1) : numChanceOutcomes(2)); 
      for (unsigned int co = 1; co <= nco; co++)
      {
        //unsigned long long infosetkey = fsiGetInfosetKey(player1, co, sequence); 

        Infoset & is = ssentry.infosets[co-1]; 
        newInfoset(is, actionshere); 

        //cout << "adding infoset with player1 = " << player1 << ", co = " << co << " (isk = " << infosetkey << ")" << endl;
        
        //iss.put(infosetkey, is, actionshere, 0);
      }

      seqstore.put(sskey, ssentry, actionshere, 0); 
    }

    pass = addOne(sequence);     
  }
  while(pass); 

}



double fsiAvgStratEV(bool player1, unsigned int * sequence, int p1roll, int p2roll)
{
  // create all the information sets with this sequence
  int curbid = sequence[RECALL-1];

  if (curbid == BLUFFBID)
  {
    // terminal
    int bidder = (player1 ? 1 : 2);
    int callingPlayer = 3-bidder; 

    double myutil = payoff(sequence[RECALL-2], bidder, callingPlayer, p1roll, p2roll, 1);
    
    return myutil;
  }
  
  if (p1roll == 0) 
  {
    double EV = 0.0;

    for (int c = 1; c <= 6; c++) 
      EV += fsiAvgStratEV(player1, sequence, c, 0)/6.0;

    return EV;
  }
  else if (p2roll == 0)
  {
    double EV = 0.0;

    for (int c = 1; c <= 6; c++) 
      EV += fsiAvgStratEV(player1, sequence, p1roll, c)/6.0;

    return EV;
  }

  unsigned int maxBid = (curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - curbid; 
  int action = -1;

  int roll = player1 ? p1roll : p2roll;
  
  // Sampled version 
  /*unsigned long long infosetkey = fsiGetInfosetKey(player1, roll, sequence); 
  Infoset is;
  bool ret = iss.get(infosetkey, is, actionshere, 0);
  assert(ret);*/

  // SS version
  SSEntry ssentry;
  //unsigned long long sskey = fsiGetSSKey(player1, sequence);
  unsigned long long sskey = getAbsSSKey(player1, sequence);
  bool ret = seqstore.get(sskey, ssentry, actionshere, 0);
  assert(ret);
  Infoset & is = ssentry.infosets[roll-1];

  double den = 0.0;
  for (int a = 0; a < actionshere; a++) den += is.totalMoveProbs[a];

  double avgStratEV = 0;

  // now recurse over all children
  
  unsigned int oldestbid = sequence[0];
    
  // left-shift to make space for new bid
  for (int j = 0; j < RECALL-1; j++) { sequence[j] = sequence[j+1]; }

  for (unsigned int i = curbid+1; i <= maxBid; i++) 
  {
    action++;
    sequence[RECALL-1] = i; 

    double avgStratProb = (den <= 0.0 ? (1.0 / actionshere) : (is.totalMoveProbs[action] / den)); 
    
    //fsiInitInfosets(!player1, sequence); 
    avgStratEV += avgStratProb * fsiAvgStratEV(!player1, sequence, p1roll, p2roll); 
  }

  // put the bid back normal (right-shift)
  
  for (int j = RECALL-1; j >= 1; j--) { sequence[j] = sequence[j-1]; }
  sequence[0] = oldestbid;

  return avgStratEV;
}

double fsiBRPass(int p1roll, int p2roll, unsigned int * sequence, int fixed_player)
{
  // sampled version. does not use SS
  initSequenceBackward(sequence); 
  bool pass = true;
  unsigned int n = 0; 
  double value = 0;
  
  do { 
  
    int curbid = sequence[RECALL-1];
    int maxBid = (curbid == 0 ? BLUFFBID-1 : BLUFFBID);
    int actionshere = maxBid - curbid; 
    
    unsigned long long sskey = 0; 

    for (int player = 2; player >= 1; player--) 
    {
      bool p1 = (player == 1 ? true : false); 
      unsigned int roll = static_cast<unsigned int>(player == 1 ? p1roll : p2roll); 
      
      Infoset is; 
      //sskey = fsiGetSSKey(p1, sequence); 
      sskey = getAbsSSKey(p1, sequence); 
      bool ret = seqstore.get(sskey, is, actionshere, 0, roll); 
      
      if (!ret) { continue; }  // failed; this can happen if the information set does not exist for this player

      /*cout << "fsi backward pass, infoset: " << p1 << " " << roll << " actions here = " << actionshere << " seq = "; 
      for (int j = 0; j < RECALL; j++) cout << sequence[j] << " "; 
      cout << endl;*/

      // prepare to traverse all the actions
      int action = -1; 

      is.value = 0; 
      //double stratEV = 0.0;
      double moveEVs[actionshere]; 
      for (int a = 0; a < actionshere; a++) moveEVs[a] = 0; 

      bool childp1 = !p1; 
      unsigned int childroll = (childp1 ? p1roll : p2roll); 
      unsigned long long childkey = 0;      

      unsigned int oldestbid = sequence[0];
        
      // left-shift to make space for new bid
      for (int j = 0; j < RECALL-1; j++) { sequence[j] = sequence[j+1]; }

      // traverse all valid actions (valid bids). One-step lookahead
      //
      for (int i = curbid+1; i <= maxBid; i++) 
      {
        action++;
        assert(action < actionshere); 

        if (i == BLUFFBID) 
        {
          // terminal node, get the payoff
          int callingPlayer = player; 
          int bidder = 3-player;
          double myutil = payoff(curbid, bidder, callingPlayer, p1roll, p2roll, player);
   
          moveEVs[action] = myutil; 
          //stratEV += is.curMoveProbs[action] * moveEVs[action];
        }
        else
        {
          sequence[RECALL-1] = i; 

          Infoset cis;           
          //childkey = fsiGetSSKey(childp1, sequence);
          childkey = getAbsSSKey(childp1, sequence);

          int childMaxBid = BLUFFBID;
          int child_actionshere = childMaxBid - i;

          /*cout << "child infoset: " << childp1 << " " << childroll << " child actions here = " 
           *     << child_actionshere << " seq = "; 
          for (int j = 0; j < RECALL; j++) cout << sequence[j] << " "; 
          cout << "(isk = " << childkey << ")" << endl;
          cout << endl; */

          bool cret = seqstore.get(childkey, cis, child_actionshere, 0, childroll);
          assert(cret); 

          assert(cis.value >= -1.0000000001 && cis.value <= 1.000000001); 

          moveEVs[action] = -cis.value;
          //stratEV += is.curMoveProbs[action] * moveEVs[action];

        }
      }

      assert(action == actionshere-1);

      // put the bid back normal (right-shift)
      for (int j = RECALL-1; j >= 1; j--) { sequence[j] = sequence[j-1]; }
      sequence[0] = oldestbid;

      if (player == fixed_player) 
      {
        double EV = 0;
        double den = 0; 
        for (int a = 0; a < actionshere; a++) den += is.totalMoveProbs[a];
        for (int a = 0; a < actionshere; a++) {
          double avgStratProb = (den <= 0.0 ? (1.0 / actionshere) : (is.totalMoveProbs[a] / den)); 
          EV += avgStratProb*moveEVs[a]; 
        }
        is.value = EV;         
      }
      else 
      {
        // choose the best one
        /*double bestVal = -1000000.0; 
        for (int a = 0; a < actionshere; a++) 
          if (moveEVs[a] > bestVal)
            bestVal = moveEVs[a];
        is.value = bestVal;*/
        double EV = 0;
        double den = 0; 
        for (int a = 0; a < actionshere; a++) den += is.totalMoveProbs[a];
        for (int a = 0; a < actionshere; a++) {
          double avgStratProb = (den <= 0.0 ? (1.0 / actionshere) : (is.totalMoveProbs[a] / den)); 
          EV += avgStratProb*moveEVs[a]; 
        }
        is.value = EV;         
      }

      value = is.value;
      CHKDBL(is.value); 

      assert(is.value >= -1.00000001 && is.value <= 1.000000001); 

      seqstore.put(sskey, is, actionshere, 0, roll); 
    }
  
    n++;
    pass = subOne(sequence);     
  }
  while(pass); 

  return value;
}

void fsiBR(int fixed_player)
{
  StatCounter sc; 

  double EV = 0.0;

  for (int co1 = 1; co1 <= numChanceOutcomes(1); co1++) {

    double childEV = 0.0;

    for (int co2 = 1; co2 <= numChanceOutcomes(2); co2++)
    {
      unsigned int sequence[RECALL];            
      double val = fsiBRPass(co1, co2, sequence, fixed_player); 

      childEV += getChanceProb(2, co2)*val; 
    }

    EV += getChanceProb(1, co1)*childEV;
  }

  //cout << "Value = " << 

}



// Adds one to the sequence. Returns true if it worked.
bool addOne(unsigned int * bidseq) 
{
  bool pass = false;
  int BIDS = BLUFFBID-1;

  for (int i = RECALL-1; i >= 0; i--) {
    // get the max digit bid for this position
    assert(BIDS + i - RECALL + 1 > 0);
    unsigned int maxDigit = static_cast<unsigned int>(BIDS + i - RECALL + 1); 

    if (bidseq[i] < maxDigit) { 
      bidseq[i]++; 
      for (int j = i+1; j < RECALL; j++) {
        bidseq[j] = bidseq[j-1]+1; 
      }
      pass = true; 
      break;
    }
  }

  return pass; 
}

// Adds one to the sequence. Returns true if it worked.
bool subOne(unsigned int * bidseq)
{
  bool pass = false;
  int BIDS = BLUFFBID-1;

  for (int i = RECALL-1; i >= 0; i--) {

    if (   (i > 0 && ((bidseq[i] > bidseq[i-1]+1) || (bidseq[i] == 1 && bidseq[i-1] == 0)))  
           // e.g. if maxbid = 6, covers the cases 125 -> 124 and 1456 -> 1356 
           // also 00156 -> 00056
        || (i == 0 && bidseq[i] > 0)   )            
           // e.g. if maxbid = 6, covers the case 234 -> 156
    {  
      bidseq[i]--; 

      for (int j = i+1; j < RECALL; j++) { 
        // get the max digit bid for this position
        assert(BIDS + j - RECALL + 1 > 0);
        unsigned int maxDigit = static_cast<unsigned int>(BIDS + j - RECALL + 1); 
        bidseq[j] = maxDigit;
      }

      pass = true; 
      break; 
    }
    else if (i == 0 && bidseq[i] == 0) 
    {
      pass = false; 
      break; 
    }
  }

  return pass; 
}

void initSequenceForward(unsigned int * sequence)
{
  // empty bid = all zeros
  for (int i = 0; i < RECALL; i++) 
    sequence[i] = 0; 
}

void initSequenceBackward(unsigned int * sequence)
{
  // start at the highest in the order
  // (bluffbid - recall) (bluffbid - recall + 1) ... (bluffbid-2) (bluffbid - 1)
  for (int i = 0; i < RECALL; i++) 
    sequence[i] = BLUFFBID - RECALL + i; 
}


void fsi_test_subtraction()
{
  // test the subtraction
  unsigned int bidseq[RECALL];
  initSequenceBackward(bidseq);

  bool pass = true; 
  int n = 0;
  do {
    //n++;
    //for (int i = 0; i < RECALL; i++) cout << bidseq[i] << " "; 
    //cout << endl; 

    bool gtw = true; 
    int parity = 0;
    if (bidseq[0] <= 1) { 
      gtw = false;
      for (int j = RECALL-1; j >= 0 && bidseq[j] > 0; j--) 
        parity++; 
    }

    if (gtw) { 
      n += 2; // valid for both players
    }
    else
    {
      n++;
    }

    pass = subOne(bidseq); 
  }
  while (pass); 

  cout << "n = " << n << endl; 
}

void fsi_test_addition()
{
  unsigned int bidseq[RECALL];
  initSequenceForward(bidseq); 

  bool pass = true; 
  int n = 0;
  do {
    //n++;
    //for (int i = 0; i < RECALL; i++) cout << bidseq[i] << " "; 
    //cout << endl; 
    bool gtw = true; 
    int parity = 0;
    if (bidseq[0] <= 1) { 
      gtw = false;
      for (int j = RECALL-1; j >= 0 && bidseq[j] > 0; j--) 
        parity++; 
    }

    if (gtw) { 
      n += 2; // valid for both players
    }
    else
    {
      n++;
    }
    
    pass = addOne(bidseq); 
  }
  while (pass); 

  cout << "n = " << n << endl; 
}

void fsiTestAddSub()
{
  fsi_test_addition();
  fsi_test_subtraction();
  //exit(-1);
}



void fsiInit()
{
  assert(iscWidth > 0); 
  bidWidth = static_cast<unsigned int>(ceil(log2(BLUFFBID-1)));
  assert(bidWidth*RECALL + iscWidth + 1 <= 64); 
  cout << "iscWidth = " << iscWidth << ", bidWidth = " << bidWidth << endl;
}

void fsiInitInfosets()
{
  assert(bidWidth > 0);

  cout << "Initialize info set store..." << endl;

  // use bluffcounter to get these numbers

  if (P1DICE == 1 && P2DICE == 1 && RECALL == 5)             // ttl infosets: 12288
    iss.init(121656, 50000); 
  else if (P1DICE == 1 && P2DICE == 1 && RECALL == 8)        // ttl infosets: 23772
    iss.init(215244, 100000); 
  else if (P1DICE == 1 && P2DICE == 1 && RECALL == 10)       // ttl infosets: 24564
    iss.init(221076, 100000); 
  else if (P1DICE == 1 && P2DICE == 1 && RECALL == 12)       // ttl infosets: 24576
    iss.init(221160, 100000); 
  else 
  {
    cerr << "ISS parameters not defined for these dice + recall values." << endl;
    assert(false);
  }
  
  assert(iss.getSize() > 0);
  cout << "Initializing info sets..." << endl;
  stopwatch.reset(); 

  unsigned int * seq = new unsigned int[RECALL];
  for (int i = 0; i < RECALL; i++) { seq[i] = 0; }
  fsiInitInfosets(true, seq); 
  
  cout << "time taken = " << stopwatch.stop() << " seconds." << endl;
  iss.stopAdding(); 

  cout << "Final iss stats: " << iss.getStats() << endl;
  stopwatch.reset();
  
  string filename = filepref + "issf-r" + to_string(RECALL) + ".initial.dat";
  cout << "Dumping information sets to " << filename << endl;
  iss.dumpToDisk(filename);
  
}

void allocSeqStore()
{
  cout << "Initialize info set store..." << endl;
  
  if (P1DICE == 1 && P2DICE == 1 && RECALL == 3)             // ttl sequences: 464
    seqstore.init(27848, 1856); 
  else if (P1DICE == 1 && P2DICE == 1 && RECALL == 5)        // ttl sequences: 2048
    seqstore.init(99128, 10000); 
  else if (P1DICE == 1 && P2DICE == 1 && RECALL == 8)        // ttl sequence: 3962 
    seqstore.init(171662, 16000); 
  else if (P1DICE == 1 && P2DICE == 1 && RECALL == 10)       // ttl sequences: 4094
    seqstore.init(176042, 16400); 
  else if (P1DICE == 1 && P2DICE == 1 && RECALL == 12)       // ttl sequences: 4096
    seqstore.init(176104, 16400); 
  else if (P1DICE == 1 && P2DICE == 2 && RECALL == 5)
    seqstore.init(2464032, 75216);
  else if (P1DICE == 2 && P2DICE == 1 && RECALL == 18)
    seqstore.init(25034698, 1048576);
  else if (P1DICE == 2 && P2DICE == 1 && RECALL == 10)
    seqstore.init(21543404, 874352);
  else if (P1DICE == 2 && P2DICE == 1 && RECALL == 5)
    seqstore.init(2464032, 75216);
  else if (P1DICE == 2 && P2DICE == 1 && RECALL == 3)
    seqstore.init(287760, 6772);
  else if (P1DICE == 3 && P2DICE == 2 && RECALL == 10)       // 5 dice 
    seqstore.init(25085062302ULL, 292156448); 
  else if (P1DICE == 2 && P2DICE == 3 && RECALL == 10)
    seqstore.init(25085062302ULL, 292156448); 
  else if (P1DICE == 4 && P2DICE == 1 && RECALL == 9)
    seqstore.init(20557241972ULL, 131916368); 
  else if (P1DICE == 1 && P2DICE == 4 && RECALL == 9)
    seqstore.init(20557241972ULL, 131916368); 
  else if (P1DICE == 1 && P2DICE == 5 && RECALL == 7)        // 6 dice
    seqstore.init(27931663788ULL, 69854784); 
  else if (P1DICE == 5 && P2DICE == 1 && RECALL == 7)        
    seqstore.init(27931663788ULL, 69854784); 
  else if (P1DICE == 2 && P2DICE == 4 && RECALL == 7)        
    seqstore.init(15922066074ULL, 69854784); 
  else if (P1DICE == 4 && P2DICE == 2 && RECALL == 7)        
    seqstore.init(15922066074ULL, 69854784); 
  else if (P1DICE == 3 && P2DICE == 3 && RECALL == 7)        
    seqstore.init(12135255984ULL, 69854784); 
  else 
  {
    cerr << "ISS parameters not defined for these dice + recall values." << endl;
    assert(false);
  }
}


void fsiInitSeqStore()
{
  assert(bidWidth > 0);

  allocSeqStore(); 
  
  assert(seqstore.getSize() > 0);
  cout << "Initializing seq store..." << endl;
  stopwatch.reset(); 

  unsigned int * seq = new unsigned int[RECALL];
  //for (int i = 0; i < RECALL; i++) { seq[i] = 0; }
  //fsiInitSeqStore(true, seq); 
  fsiInitSeqStore(seq); 
  
  cout << "time taken = " << stopwatch.stop() << " seconds." << endl;
  seqstore.stopAdding(); 

  cout << "Final iss stats: " << seqstore.getStats() << endl;
  stopwatch.reset();
  
  string filename = filepref + "seqstore-" + to_string(P1DICE) + "-" + to_string(P2DICE)
                   + "-r" + to_string(RECALL) + ".initial.dat";

  cout << "Dumping information sets to " << filename << endl;
  seqstore.dumpToDisk(filename);

}

#else

#endif
