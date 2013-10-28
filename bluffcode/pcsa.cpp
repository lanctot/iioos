
#include <cassert>
#include <iostream>
#include <cstdlib>

#include "bluff.h"
#include "svector.h"
#include "seqstore.h"

using namespace std; 

typedef SVector<P1CO> covector1;
typedef SVector<P2CO> covector2;

extern int RECALL;

extern SequenceStore seqstore;

static unsigned long long ttlLeafEvals = 0; 
static unsigned long long ttlUpdates = 0;

/* 
 * It seems that ehere is currently issue with this implementations of PCS
 * when the number of chance outcomes are not the same for both players.
 */

typedef SVector<P1CO> covector1;
typedef SVector<P2CO> covector2;

void handleLeaf(GameState & gs, int updatePlayer, covector1 & reach1, covector2 & reach2, 
                covector1 & result1, covector2 & result2)
{
  int upco = (updatePlayer == 1 ? P1CO : P2CO); 
  int opco = (updatePlayer == 1 ? P2CO : P1CO);

  assert(upco > 0); 
  assert(opco > 0); 

  int opponent = 3 - updatePlayer;

  // because it's strictly alternating
  int bidder = 3 - gs.callingPlayer;

  // modified after talking to mjo
  int oppNonzero = 0;
  for (int j = 0; j < opco; j++)
  {
    if (updatePlayer == 1 && reach2[j] > 0)
      oppNonzero++;
    else if (updatePlayer == 2 && reach1[j] > 0)
      oppNonzero++;
  }

  int quantity, face;
  convertbid(quantity, face, gs.prevbid);
  int oppDice = (updatePlayer == 1 ? P2DICE : P1DICE);
  double opp_probs[oppDice+1];
  for (int i = 0; i < oppDice+1; i++)
    opp_probs[i] = 0.0;

  for (int o = 0; o < opco; o++) 
  {
    //state->overrideChanceOutcome(opponent, oppChanceOutcomes[i]);
    
    if (opponent == 1) 
      gs.p1roll = o+1;
    else if (opponent == 2)
      gs.p2roll = o+1;

    int d = countMatchingDice(gs, opponent, face); 
     
    //opp_probs[d] += (oppChanceProbs[i]*oppReach[i]);
  
    if (updatePlayer == 1)
      opp_probs[d] += getChanceProb(opponent, o+1)*reach2[o]; 
    else if (updatePlayer == 2)
      opp_probs[d] += getChanceProb(opponent, o+1)*reach1[o]; 
  }


  for (int o = 0; o < upco; o++) 
  {
    // iterate over the update player's outcomes. 
    double val = 0.0; 

    //state->overrideChanceOutcome(updatePlayer, myChanceOutcomes[i]);
    
    if (updatePlayer == 1)
      gs.p1roll = o+1;
    else if (updatePlayer == 2)
      gs.p2roll = o+1;

    for (int j = 0; j < oppDice+1; j++)
    {
      //int myd = state->countDice(updatePlayer, face);  
      ttlLeafEvals++;

      int myd = countMatchingDice(gs, updatePlayer, face);  
      
      double payoffToBidder = (myd+j >= quantity ? 1.0 : -1.0);
      double payoff = (updatePlayer == bidder ? payoffToBidder : -payoffToBidder); 
      val += payoff*opp_probs[j];
    }

    if (updatePlayer == 1)
      result1[o] = val; 
    else if (updatePlayer == 2)
      result2[o] = val;
  }
}

void pcsa(GameState & gs, int player, int depth, unsigned long long bidseq, 
          int updatePlayer, covector1 & reach1, covector2 & reach2, 
          int phase, covector1 & result1, covector2 & result2)
{
  reach1.assertprob();
  reach2.assertprob();
  
  // check if we're at a terminal node
  
  if (terminal(gs))
  {
    handleLeaf(gs, updatePlayer, reach1, reach2, result1, result2);
    return;
  }

  // chance nodes (just bogus entries)
  // note: for expected values to make sense, should iterate over each move.

  if (gs.p1roll == 0) 
  {
    GameState ngs = gs; 
    ngs.p1roll = 1;       
    pcsa(ngs, player, depth+1, bidseq, updatePlayer, reach1, reach2, phase, result1, result2);
    return;
  }
  else if (gs.p2roll == 0) 
  {
    GameState ngs = gs; 
    ngs.p2roll = 1;       
    pcsa(ngs, player, depth+1, bidseq, updatePlayer, reach1, reach2, phase, result1, result2);
    return;
  }

  // cuts?
  if (phase == 1)
  {
    if (iter > 1 && updatePlayer == 1 && player == 1 && reach2.allEqualTo(0.0))
    {
      phase = 2;
    }
    else if (iter > 1 && updatePlayer == 2 && player == 2 && reach1.allEqualTo(0.0))
    {
      phase = 2; 
    }
  }

  if (phase == 2)
  {
    if (iter > 1 && updatePlayer == 1 && player == 1 && reach1.allEqualTo(0.0))
    {
      result1.reset(0.0);
      result2.reset(0.0);
      return;
    }
    if (iter > 1 && updatePlayer == 2 && player == 2 && reach2.allEqualTo(0.0))
    {
      result1.reset(0.0);
      result2.reset(0.0);
      return;
    }
  }
  
  // declare the variables
  //int upco = (updatePlayer == 1 ? P1CO : P2CO); 
  int co = (player == 1 ? P1CO : P2CO);

  int action = -1;
  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 
  // count the number of actions and initialize moveEVs
  // special case. can't just call bluff on the first move!
  assert(actionshere > 0);

  // get the infosets here (one per outcome)

  unsigned long long infosetkeys[co];
  //unsigned long long sskeys[co];
  Infoset is[co];  
 
  // only one of these is used
  covector1 moveEVs1[actionshere];
  covector2 moveEVs2[actionshere];

  for (int i = 0; i < co; i++)
  {
    int outcome = i+1;

    //getAbsSSKey(gs, sskeys[i], player, bidseq);
    //bool ret = seqstore.get(sskeys[i], is[i], actionshere, 0, outcome);
    //assert(ret);

    infosetkeys[i] = bidseq; 
    infosetkeys[i] <<= iscWidth; 
    if (player == 1)
    {
      infosetkeys[i] |= outcome; 
      infosetkeys[i] <<= 1; 
      bool ret = iss.get(infosetkeys[i], is[i], actionshere, 0); 
      assert(ret);
    }
    else if (player == 2)
    {
      infosetkeys[i] |= outcome; 
      infosetkeys[i] <<= 1; 
      infosetkeys[i] |= 1; 
      bool ret = iss.get(infosetkeys[i], is[i], actionshere, 0); 
      assert(ret);
    }
  }

  // iterate over the actions

  for (int i = gs.curbid+1; i <= maxBid; i++) 
  {
    action++;
    assert(action < actionshere);

    // only one of these is used
    covector1 moveProbs1;
    covector2 moveProbs2;

    for (int o = 0; o < co; o++) 
    {
      if (player == 1)
        moveProbs1[o] = is[o].curMoveProbs[action];
      else if (player == 2)
        moveProbs2[o] = is[o].curMoveProbs[action]; 
    }

    covector1 EV1; 
    covector2 EV2; 
    covector1 newReach1 = reach1; 
    covector2 newReach2 = reach2; 
    if (player == 1) newReach1 *= moveProbs1; 
    if (player == 2) newReach2 *= moveProbs2;

    GameState ngs = gs; 
    ngs.prevbid = gs.curbid;
    ngs.curbid = i; 
    ngs.callingPlayer = player;
    unsigned long long newbidseq = bidseq;
    newbidseq |= (1ULL << (BLUFFBID-i)); 

    pcsa(ngs, 3-player, depth+1, newbidseq, updatePlayer, newReach1, newReach2, phase, EV1, EV2); 

    if (player == updatePlayer)
    {
      if (player == 1)
      {
        moveEVs1[action] = EV1;
        EV1 *= moveProbs1;
        result1 += EV1;
      }
      else if (player == 2)
      {
        moveEVs2[action] = EV2;
        EV2 *= moveProbs2;
        result2 += EV2;
      }
    }
    else 
    {
      if (updatePlayer == 1)    
        result1 += EV1;
      else if (updatePlayer == 2)    
        result2 += EV2;
    }
  }

  // now the real stuff, cfr updates

  if (player == updatePlayer && phase == 1)
  {
    // regrets will be changed, so make sure to indicate it to prob updater
    for (int o = 0; o < co; o++)
      is[o].lastUpdate = iter;

    for (int o = 0; o < co; o++)
    {
      ttlUpdates++; 

      for (int a = 0; a < actionshere; a++)
      {
        double moveEV = (player == 1 ? moveEVs1[a][o] : moveEVs2[a][o]);
        double resulto = (player == 1 ? result1[o] : result2[o]); 

        is[o].cfr[a] += (moveEV - resulto); 
      }
    }
  }

  if (player == updatePlayer && phase <= 2)
  {
    for (int o = 0; o < co; o++)
    {
      for (int a = 0; a < actionshere; a++)
      {
        double my_prob = (player == 1 ? reach1[o] : reach2[o]);

        // update total probs
        is[o].totalMoveProbs[a] += my_prob*is[o].curMoveProbs[a];
      }
    }
  }

  if (player == updatePlayer) 
  {
    for (int o = 0; o < co; o++) 
    {
      iss.put(infosetkeys[o], is[o], actionshere, 0); 
      //seqstore.put(sskeys[o], is[o], actionshere, 0, o+1); 
    }
  }
}

int main(int argc, char ** argv)
{
  init();

  if (argc < 2) {
    cerr << "Usage: ./pcsa <recall value>" << endl;
    exit(-1); 
  }

  if (argc <= 2) 
  {
    string recstr = argv[1];
    RECALL = to_int(recstr); 
    assert(RECALL >= 1); 

    absInitInfosets(); // for ISS version    
    ///fsiInitSeqStore();   // for seqstore (ss) version

    exit(-1);
  }
  else
  {
    if (argc != 3) { 
      cerr << "Usage: ./pcsa <recall value> <abstract infoset file>" << endl;
      exit(-1); 
    }
    
    string recstr = argv[1];
    RECALL = to_int(recstr); 
    assert(RECALL >= 1); 

    string filename = argv[2];
    cout << "Reading the infosets from " << filename << "..." << endl;
    
    iss.readFromDisk(filename);
    //seqstore.readFromDisk(filename);  
  }

  unsigned long long bidseq = 0; 
    
  StopWatch stopwatch;
  double totaltime = 0; 

  cout << "PCSa; starting iterations" << endl;

  for (; true; iter++)
  {
    covector1 reach1(1.0); 
    covector2 reach2(1.0); 
    covector1 result1; 
    covector2 result2;

    GameState gs1; 
    bidseq = 0; 
    pcsa(gs1, 1, 0, bidseq, 1, reach1, reach2, 1, result1, result2);
    
    GameState gs2; 
    bidseq = 0; 
    reach1.reset(1.0);
    reach2.reset(1.0);
    pcsa(gs2, 1, 0, bidseq, 2, reach1, reach2, 1, result1, result2);

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

      cout << "Computing bounds... "; cout.flush(); 
      double b1 = 0.0, b2 = 0.0;
      iss.computeBound(b1,b2);
      //seqstore.computeBound(b1, b2); 
      cout << " b1 = " << b1 << ", b2 = " << b2 << ", bound = " << (2.0*MAX(b1,b2)) << endl;

      // note: can't compute this on-the-fly with abstraction
      computeBestResponses(false, false);

      report("pcsareport.txt", totaltime, (2.0*MAX(b1,b2)), 0, 0, 0);
      dumpSeqStore("issa");
      //dumpSeqStore("seqstore-pcsa");
      cout << endl;
     
      nextCheckpoint += cpWidth;

      stopwatch.reset(); 
    }

  }
}
