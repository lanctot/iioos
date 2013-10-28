
#include <cassert>
#include <iostream>
#include <cstdlib>

#include "bluff.h"
#include "seqstore.h"

using namespace std; 

extern int RECALL;

static unsigned long long nextReport = 1;
static unsigned long long reportMult = 2;

extern SequenceStore seqstore;

#if FSICFR

static unsigned long long totalHits = 0;
static unsigned long long totalMisses = 0;

unsigned int fsiForwardPass(int p1roll, int p2roll, unsigned int * sequence)
{
  // sampled version. does not use SS
  initSequenceForward(sequence); 
  bool pass = true;
  bool initial = true; 
  unsigned int n = 0;
  
  do { 
  
    int curbid = sequence[RECALL-1];
    int maxBid = (curbid == 0 ? BLUFFBID-1 : BLUFFBID);
    int actionshere = maxBid - curbid; 
    
    unsigned long long sskey = 0; 

    for (int player = 1; player <= 2; player++) 
    {
      bool p1 = (player == 1 ? true : false); 
      unsigned int roll = static_cast<unsigned int>(player == 1 ? p1roll : p2roll); 
      
      Infoset is; 
      //sskey = fsiGetSSKey(p1, sequence); 
      sskey = getAbsSSKey(p1, sequence); 
      bool ret = seqstore.get(sskey, is, actionshere, 0, roll); 
      if (!ret) { totalMisses++; continue; } // failed; this can happen if the information set does not exist for this player

      totalHits++; 

      /*cout << "fsi forward pass, infoset: " << p1 << " " << roll << " actions here = " << actionshere << " seq = "; 
      for (int j = 0; j < RECALL; j++) cout << sequence[j] << " "; 
      cout << endl;*/

      if (initial) 
      { 
        is.reach1 = 1.0; 
        is.reach2 = 1.0; 
      }

      CHKDBL(is.reach1); 
      CHKDBL(is.reach2); 

      if (p1 && is.reach1 <= 0.0) continue;
      if (!p1 && is.reach2 <= 0.0) continue;

      // prepare to traverse all the actions
      int action = -1; 

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
        
        // update the average strategy (line 12 part of the FSICFR paper)
        if (p1) { is.totalMoveProbs[action] += is.reach1*is.curMoveProbs[action]; }
        else { is.totalMoveProbs[action] += is.reach2*is.curMoveProbs[action]; }

        if (i == BLUFFBID) continue; 

        sequence[RECALL-1] = i; 

        //Infoset cis;           
        //childkey = fsiGetSSKey(childp1, sequence); 
        childkey = getAbsSSKey(childp1, sequence); 

        int childMaxBid = BLUFFBID;
        int child_actionshere = childMaxBid - i;

        /*cout << "child infoset: " << childp1 << " " << childroll << " child actions here = " << child_actionshere << " seq = "; 
        for (int j = 0; j < RECALL; j++) cout << sequence[j] << " "; 
        cout << "(isk = " << childkey << ")" << endl;
        cout << endl; */
      
        /*bool cret = seqstore.get(childkey, cis, child_actionshere, 0, childroll, false); 
        assert(cret); 

        cis.reach1 += (p1 ? is.curMoveProbs[action]*is.reach1 : is.reach1); 
        cis.reach2 += (p1 ? is.reach2 : is.curMoveProbs[action]*is.reach2); 
        
        seqstore.put(childkey, cis, child_actionshere, 0, childroll); */
        
        double crInc1 = (p1 ? is.curMoveProbs[action]*is.reach1 : is.reach1); 
        double crInc2 = (p1 ? is.reach2 : is.curMoveProbs[action]*is.reach2); 

        bool cret = seqstore.fsiIncReaches(childkey, child_actionshere, 0, childroll, crInc1, crInc2);
        assert(cret);
      }
      
      assert(action == actionshere-1);

      // put the bid back normal (right-shift)
      for (int j = RECALL-1; j >= 1; j--) { sequence[j] = sequence[j-1]; }
      sequence[0] = oldestbid;
      
      // the average strategy was updated for this node, so must save
      seqstore.put(sskey, is, actionshere, 0, roll); 
    
      initial = false; 
    }
  
    n++;

    pass = addOne(sequence);     
  }
  while(pass); 

  return n;
}


unsigned int fsiBackwardPass(int p1roll, int p2roll, unsigned int * sequence)
{
  // sampled version. does not use SS
  initSequenceBackward(sequence); 
  bool pass = true;
  unsigned int n = 0; 
  
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
      
      if (!ret) { totalMisses++; continue; }  // failed; this can happen if the information set does not exist for this player

      totalHits++; 

      /*cout << "fsi backward pass, infoset: " << p1 << " " << roll << " actions here = " << actionshere << " seq = "; 
      for (int j = 0; j < RECALL; j++) cout << sequence[j] << " "; 
      cout << endl;*/

      // prepare to traverse all the actions
      int action = -1; 

      is.value = 0; 
      double stratEV = 0.0;
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
          stratEV += is.curMoveProbs[action] * moveEVs[action];
        }
        else
        {
          sequence[RECALL-1] = i; 

          //Infoset cis;           
          //childkey = fsiGetSSKey(childp1, sequence);
          childkey = getAbsSSKey(childp1, sequence);

          int childMaxBid = BLUFFBID;
          int child_actionshere = childMaxBid - i;

          /*cout << "child infoset: " << childp1 << " " << childroll << " child actions here = " 
           *     << child_actionshere << " seq = "; 
          for (int j = 0; j < RECALL; j++) cout << sequence[j] << " "; 
          cout << "(isk = " << childkey << ")" << endl;
          cout << endl; */

          /*bool cret = seqstore.get(childkey, cis, child_actionshere, 0, childroll, false);
          assert(cret); 

          assert(cis.value >= -1.0000000001 && cis.value <= 1.000000001); 
          */
          
          double cval = 0;

          bool cret = seqstore.fsiGetValue(childkey, cval, child_actionshere, 0, childroll);

          assert(cret);
          assert(cval >= -1.0000000001 && cval <= 1.000000001); 

          moveEVs[action] = -cval;
          stratEV += is.curMoveProbs[action] * moveEVs[action];
        }
      }

      assert(action == actionshere-1);

      // put the bid back normal (right-shift)
      for (int j = RECALL-1; j >= 1; j--) { sequence[j] = sequence[j-1]; }
      sequence[0] = oldestbid;

      is.value = stratEV; 

      CHKDBL(is.value); 

      assert(is.value >= -1.00000001 && is.value <= 1.000000001); 

      double oppreach = (player == 1 ? is.reach2 : is.reach1); 
      for (int a = 0; a < actionshere; a++)
      {
        is.cfr[a] += oppreach*(moveEVs[a] - stratEV); 
      }
      
      is.reach1 = is.reach2 = 0.0;

      seqstore.put(sskey, is, actionshere, 0, roll); 
    }
  
    n++;
    pass = subOne(sequence);     
  }
  while(pass); 

  return n;
}


void fsiIter()
{
  // sampled version. does not use SS 
  //
  int p1roll = 0, p2roll = 0;
  double prob1 = 0.0, prob2 = 0.0;

  sampleChanceEvent(1, p1roll, prob1); 
  sampleChanceEvent(2, p2roll, prob2);

  CHKPROBNZ(prob1);
  assert(p1roll > 0); 
  
  CHKPROBNZ(prob2);
  assert(p2roll > 0); 

  unsigned int sequence[RECALL];

  unsigned int infosets1 = fsiForwardPass(p1roll, p2roll, sequence); 
  unsigned int infosets2 = fsiBackwardPass(p1roll, p2roll, sequence); 

  assert(infosets1 == infosets2);
}

#else

void fsiIter()
{
  cerr << "fsiIter unimplemented when FSICFR is #defined to 0" << endl;
  exit(-1);
}

#endif


int main(int argc, char ** argv)
{
  //unsigned long long maxIters = 0; 
  init();
  
  if (argc < 2) {
    cerr << "Usage: ./fsicfr <recall value>" << endl;
    exit(-1); 
  }

  if (argc <= 2) 
  {
    string recstr = argv[1];
    RECALL = to_int(recstr); 
    assert(RECALL >= 1); 

    //fsiInitInfosets(); // for ISS version    
    fsiInitSeqStore();   // for seqstore (ss) version

    exit(-1);
  }
  else
  {
    if (argc != 3) { 
      cerr << "Usage: ./fsicfr <recall value> <abstract infoset file>" << endl;
      exit(-1); 
    }
    
    string recstr = argv[1];
    RECALL = to_int(recstr); 
    assert(RECALL >= 1); 

    string filename = argv[2];
    cout << "Reading the infosets from " << filename << "..." << endl;
    seqstore.readFromDisk(filename);  
  }

  cout << "Starting FSICFR..." << endl;

  StopWatch stopwatch;
  double totaltime = 0; 
  
  double nextProgressWidth = cpWidth/100.0;
  double nextProgressTime = nextProgressWidth;

  for (iter = 1; true; iter++)
  {
    //if (iter % 10 == 0) { cout << "Starting iter " << iter << endl; }
    
    fsiIter(); 

    if (stopwatch.stop() > nextProgressTime)
    {
      cout << "."; cout.flush(); 
      nextProgressTime += nextProgressWidth;
    }
    
    //if (iter >= nextReport)
    if (totaltime + stopwatch.stop() > nextCheckpoint)
    {
      cout << endl;
      //cout << "iter = " << iter << ", " << seqstore.getStats() << endl;
      //exit(-1);

      //cout << totalHits << " " << totalMisses << endl;
      //cout << seqstore.getStats() << endl;


      //if (iter % 10 == 0) { cout << "Starting iter " << iter << endl; }
      cout << "Finished iter " << iter << endl; 

      totaltime += stopwatch.stop();
      cout << "total time = " << totaltime << endl;

      unsigned int sequence[RECALL];
      initSequenceForward(sequence);
      double avStratEV = fsiAvgStratEV(true, sequence, 0, 0);
      cout << totaltime << " expected value of av strat = " << avStratEV << endl;

      double b1 = 0.0, b2 = 0.0;
      //iss,computeBounds(b1, b2);   // Sampling version
      //fsiss.computeBound(b1, b2);  // SS version
      seqstore.computeBound(b1, b2); 
      cout << "b1 = " << b1 << ", b2 = " << b2 << ", bound = " << (2.0*MAX(b1,b2)) << endl;
      
      double conv = 0.0;
      conv = computeBestResponses(false, false);

      string tag = "fsi-" + to_string(P1DICE) + "-" + to_string(P2DICE) + "-r" + to_string(RECALL);
      report(tag + ".report.txt", 
            totaltime, 2.0*MAX(b1,b2), 0, 0, conv);
      //dumpInfosets(tag);
      
      cout << endl;

      stopwatch.reset();
      nextProgressWidth = cpWidth/100.0;
      nextProgressTime = nextProgressWidth;
     
      nextCheckpoint += cpWidth;
      nextReport *= reportMult;
      
      //if (iter > 32) 
      //  exit(-1);

    }

  }
}

