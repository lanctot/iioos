
#include <iostream>
#include <cassert>
#include <cstdlib>

#include "bluff.h"

using namespace std; 

static string runname = "";

static int expansions = 0; 

static double oos_gamma = 0.95; 
static double oos_epsilon = 0.6;

// 1 = IST
// 2 = PST
static int oos_variant = 1; 

/**
 * Biased sampling at chance nodes. This is new... no CFR variants have ever done this. 
 * You have to bias the sampling at chance nodes so that sampling Z_{sub} is more likely. 
 * This means having to pass down the sampling prob and the opponent reach, unlike other 
 * CFR variants where these always cancel out. 
 */ 
void biasedChanceSample(GameState & match_gs, int player, int updatePlayer, GameState & gs, 
                        double & sampleProb, double & chanceProb) { 

  if (oos_variant == 1) { 
    // information set targeting applies the bias only at "our" chance event
    
    // if at an "opponent" chance event, choose the usual way

    if (updatePlayer != player) {
      if (player == 1) { 
        sampleChanceEvent(1, gs.p1roll, sampleProb);
        chanceProb = sampleProb;
      }
      else if (player == 2) { 
        sampleChanceEvent(2, gs.p2roll, sampleProb);
        chanceProb = sampleProb;
      }

      return; 
    }

    // ok, we're at "our" chance event here.. time for some good, old-fashioned bias :) 

    double roll = drand48(); 

    if (roll < oos_gamma) { 
      // we're hitting the bias case; set to be consistent with match game
      //int outcomes = (player == 1 ? numChanceOutcomes(1) : numChanceOutcomes(2));  
      
      int outcome = (player == 1 ? match_gs.p1roll : match_gs.p2roll);       
      chanceProb = getChanceProb(player, outcome);
     
      if (player == 1) {
        gs.p1roll = outcome; 
        sampleProb = oos_gamma;
      }
      else if (player == 2) { 
        gs.p2roll = outcome;
        sampleProb = oos_gamma;
      }
    }
    else { 
      int outcome = (player == 1 ? match_gs.p1roll : match_gs.p2roll);       
      double outcomeProb = getChanceProb(player, outcome);

      // sample something else
      bool done = false; 
      while (!done) { 
        // resample until get one that is not the same as the match
        //
        if (player == 1) { 
          sampleChanceEvent(1, gs.p1roll, sampleProb);
          chanceProb = sampleProb;
          if (gs.p1roll != outcome) done = true;
        }
        else if (player == 2) { 
          sampleChanceEvent(2, gs.p2roll, sampleProb);
          chanceProb = sampleProb;
          if (gs.p2roll != outcome) done = true;
        }
      }

      // renormalize over all actions that are not the biased one. then mult by 1/gamma
      sampleProb *= (chanceProb / (1.0 - outcomeProb))*(1.0-oos_gamma); 
    }

  }
  else { 
    // unimplemented
    assert(false); 
  }
}

/**
 * Stage 1: Prefix stage
 *
 * Assume a single action is "consistent with the history" (in the case of Bluff, this is true, 
 * can be generalized w.l.o.g.) there is a pure dist P assigns 1 to only that action and 0 to
 * all other actions.
 *
 * What we constuct here then is a distribution D, 
 *
 *   Let expl = epsilon if P(h) = i, 0 othewise
 *
 *   D =  \gamma P + (1 - \gamma) ( expl Unif(A(I)) + (1 - expl) \sigma(I) )
 *
 * And we sample an action according to D. 
 * 
 */
int oosSampleAction_stage1(GameState & match_gs, unsigned long long match_bidseq, GameState & gs, 
  unsigned long long bidseq, int player, int updatePlayer, Infoset & is, int actionshere, 
  double & sampleProb) {

  // get the consistent action (bid)
  int action = -1; 

  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
      
  unsigned long long singleBitMask = (1ULL << (BLUFFBID-(gs.curbid+1)));

  for (int bid = gs.curbid+1; bid <= maxBid; bid++) {
    action++; 

    if ((match_bidseq & singleBitMask) > 0) { 
      // this is the next bid
      break;
    }

    singleBitMask >>= 1; 
  }

  assert(action >= 0 && action < actionshere); 

  int takeAction = -1;

  if (player == updatePlayer)
    takeAction = sampleActionBiased(player, is, actionshere, sampleProb, oos_epsilon, oos_gamma, action); 
  else
    takeAction = sampleActionBiased(player, is, actionshere, sampleProb, 0.0, oos_gamma, action); 

  return takeAction;
}

/**
 * Stage 2: Sampling is done exactly as in OS
 */
int oosSampleAction_stage2(GameState & match_gs, unsigned long long match_bidseq, GameState & gs, 
  unsigned long long bidseq, int player, int updatePlayer, Infoset & is, int actionshere, 
  double & sampleProb) {

  int takeAction = -1;

  if (player == updatePlayer)
    takeAction = sampleAction(player, is, actionshere, sampleProb, oos_epsilon, false); 
  else
    takeAction = sampleAction(player, is, actionshere, sampleProb, 0.0, false); 

  return takeAction;
}

//int takeAction = oosSampleAction(match_gs, match_bidseq, bidseq, player, updatePlayer, is, actionshere, sampleprob, 0.6);
int oosSampleAction(GameState & match_gs, unsigned long long match_bidseq, GameState & gs, unsigned long long bidseq,
                    int player, int updatePlayer, Infoset & is, int actionshere, double & sampleProb) {

  // decide: are we in the prefix stage or the tail stage?
  // 1 = prefix stage
  // 2 = tail stage
  int stage = 0; 

  if (oos_variant == 1) { 
    // for IST, we are in the prefix stage if: 
    // - update player's chance outcome is the same, 
    // - all the bids are the same up to here
    // else, we're in the tail stage
 
    int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
    int match_curbid = match_gs.curbid; 

    if (updatePlayer == 1 && match_gs.p1roll != gs.p1roll) { 
      stage = 2; 
    }
    else if (updatePlayer == 2 && match_gs.p2roll != gs.p2roll) { 
      stage = 2;
    }
    else { 
      // matching outcome, now much check: is my history a prefix of the match history?
      
      int lastMatchingBid = 0;  // which is the most lastest matching bid position

      //newbidseq |= (1 << (BLUFFBID-i)); 
      unsigned long long singleBitMask = (1ULL << (BLUFFBID-1));
      
      for (int bid = 1; bid <= gs.curbid; bid++) { 
        unsigned long long matchSeqBid = match_bidseq & singleBitMask;
        unsigned long long curSeqBid = bidseq & singleBitMask; 

        if (bid <= gs.curbid && matchSeqBid == curSeqBid && (lastMatchingBid+1) == bid) {
          lastMatchingBid = bid;
        }

        singleBitMask >>= 1; 
      }
      
      // is my history a (strict) prefix of the match history? for this to be true: 
      // - the last matching actual bid has to be equal to the cur bid, and
      // - my history's current bid has to be < the match history's current bid
      if (lastMatchingBid == gs.curbid && gs.curbid < match_gs.curbid)  { 
        // proper prefix
        stage = 1; 
      }
      else {
        stage = 2; 
      }
    }
    
  }

  if (stage == 1)
    return oosSampleAction_stage1(match_gs, match_bidseq, gs, bidseq, player, updatePlayer, 
      is, actionshere, sampleProb);
  else if (stage == 2) 
    return oosSampleAction_stage2(match_gs, match_bidseq, gs, bidseq, player, updatePlayer, 
      is, actionshere, sampleProb);
  else { 
    assert(false);
    return 0;
  }
}


double cfroos(GameState & match_gs, unsigned long long match_bidseq,  
              GameState & gs, int player, int depth, unsigned long long bidseq, 
              double reach1, double reach2, double sprob1, double sprob2, int updatePlayer, 
              double & suffixreach, double & rtlSampleProb, bool treePhase)
{
  CHKPROB(reach1); 
  CHKPROB(reach2); 

  // check: at terminal node?
  if (terminal(gs))
  {
    suffixreach = 1.0; 
    rtlSampleProb = sprob1*sprob2;
    
    return payoff(gs, updatePlayer); 
  }
  
  nodesTouched++;

  // chance nodes. 

  if (gs.p1roll == 0) 
  {
    GameState ngs = gs; 
    double sampleProb = 0, chanceProb = 0;
    biasedChanceSample(match_gs, 1, updatePlayer, ngs, sampleProb, chanceProb);

    CHKPROBNZ(sampleProb);
    CHKPROBNZ(chanceProb);
    assert(ngs.p1roll > 0);

    // need to modify the reaches and sprobs, unlike other sampling variants
    if (updatePlayer == 1) { 
      sprob2 *= sampleProb;
      reach2 *= chanceProb; 
    }
    else if (updatePlayer == 2) { 
      sprob1 *= sampleProb;
      reach1 *= chanceProb; 
    }

    return cfroos(match_gs, match_bidseq, ngs, player, depth+1, bidseq, reach1, reach2, 
                  sprob1, sprob2, updatePlayer, suffixreach, rtlSampleProb, treePhase); 
  }
  else if (gs.p2roll == 0)
  {
    GameState ngs = gs; 
    double sampleProb = 0, chanceProb = 0;
    biasedChanceSample(match_gs, 2, updatePlayer, ngs, sampleProb, chanceProb);
    
    CHKPROBNZ(sampleProb);
    CHKPROBNZ(chanceProb);
    assert(ngs.p2roll > 0);
    
    // need to modify the reaches and sprobs, unlike other sampling variants
    if (updatePlayer == 1) { 
      sprob2 *= sampleProb;
      reach2 *= chanceProb; 
    }
    else if (updatePlayer == 2) { 
      sprob1 *= sampleProb;
      reach1 *= chanceProb; 
    }
    
    // don't need to worry about keeping track of sampled chance probs for outcome sampling
    return cfroos(match_gs, match_bidseq, ngs, player, depth+1, bidseq, reach1, reach2, 
                  sprob1, sprob2, updatePlayer, suffixreach, rtlSampleProb, treePhase); 
  }

  // declare the variables
  Infoset is;
  unsigned long long infosetkey = 0;
  int action = -1;

  // count the number of actions. Can't call Bluff as first action when there is no bid
  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 
  assert(actionshere > 0);

  // get the info set
  getInfoset(gs, player, bidseq, is, infosetkey, actionshere);

  // this is the incremental tree-building part
  
  bool newTreePhase = treePhase;
  
  if (treePhase && is.lastUpdate == 0) { 
    // expand
    is.lastUpdate = 1; 
    newTreePhase = false;
    expansions++;
  }

  // sample the action to take. Epsilon on-policy at my nodes. On-Policy at opponents.

  double sampleprob = -1; 
  int takeAction = 0; 
  double newsuffixreach = 1.0; 

  // sample the action!
  takeAction = oosSampleAction(match_gs, match_bidseq, gs, bidseq, player, updatePlayer, is, actionshere, sampleprob);

  CHKPROBNZ(sampleprob); 
  double newsprob1 = (player == 1 ? sampleprob*sprob1 : sprob1); 
  double newsprob2 = (player == 2 ? sampleprob*sprob2 : sprob2); 

  double ctlReach = 0;
  double itlReach = 0; 
  double updatePlayerPayoff = 0;

  // take the action. find the i for this action
  int i;
  for (i = gs.curbid+1; i <= maxBid; i++) 
  {
    action++; 

    if (action == takeAction)
      break; 
  }
  
  assert(i >= gs.curbid+1 && i <= maxBid); 
  double moveProb = is.curMoveProbs[action]; 

  // do this only once

  unsigned long long newbidseq = bidseq;
  GameState ngs = gs; 
  ngs.prevbid = gs.curbid;
  ngs.curbid = i; 
  ngs.callingPlayer = player;
  newbidseq |= (1 << (BLUFFBID-i)); 
  
  CHKPROB(moveProb); 
  double newreach1 = (player == 1 ? moveProb*reach1 : reach1); 
  double newreach2 = (player == 2 ? moveProb*reach2 : reach2); 

  updatePlayerPayoff = cfroos(match_gs, match_bidseq, ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, 
                              newsprob1, newsprob2, updatePlayer, newsuffixreach, rtlSampleProb, newTreePhase);

  ctlReach = newsuffixreach; 
  itlReach = newsuffixreach*is.curMoveProbs[action]; 
  suffixreach = itlReach;

  // payoff always in view of update player
  double myreach = (player == 1 ? reach1 : reach2); 
  double oppreach = (player == 1 ? reach2 : reach1); 

  // only perform updates if in tree or expanding
  if (treePhase) { 
    // update regrets (my infosets only)
    if (player == updatePlayer) 
    { 
      for (int a = 0; a < actionshere; a++)
      {
        // oppreach and sprob2 cancel in the case of stochastically-weighted averaging
        //is.cfr[a] += (moveEVs[a] - stratEV); 
      
        double U = updatePlayerPayoff * oppreach / rtlSampleProb;
        double r = 0.0; 
        if (a == takeAction) 
          r = U * (ctlReach - itlReach);
        else 
          r = -U * itlReach;

        is.cfr[a] += r; 
      }
    }
   
    if (player != updatePlayer) { 
      // update av. strat
      for (int a = 0; a < actionshere; a++)
      {
        // stochastically-weighted averaging
        double inc = (1.0 / (sprob1*sprob2))*myreach*is.curMoveProbs[a];
        is.totalMoveProbs[a] += inc; 
      }
    }
  
    if (sg_curPlayer == 1)  
      sgiss1.put(infosetkey, is, actionshere, 0); 
    else 
      sgiss2.put(infosetkey, is, actionshere, 0); 
  }

  return updatePlayerPayoff;
}

// Called from sim.cpp
int getMoveOOS(int player, GameState match_gs, unsigned long long match_bidseq) { 
  
  unsigned long long bidseq = 0; 
    
  double totaltime = 0; 
  StopWatch sw; 

  iter = 0; 

  cout << "Starting OOS iterations..." << endl;

  for (; true; iter++)
  {
    //cout << "OOS iter = " << iter << endl; 

    GameState gs1; bidseq = 0;
    double suffixreach = 1.0; 
    double rtlSampleProb = 1.0; 
    cfroos(match_gs, match_bidseq, gs1, 1, 0, bidseq, 1.0, 1.0, 1.0, 1.0, 1, suffixreach, rtlSampleProb, true);
    
    GameState gs2; bidseq = 0;
    suffixreach = 1.0; 
    rtlSampleProb = 1.0; 
    cfroos(match_gs, match_bidseq, gs1, 1, 0, bidseq, 1.0, 1.0, 1.0, 1.0, 2, suffixreach, rtlSampleProb, true);

    if (sw.stop() > timeLimit) 
      break;
  }

  cout << "OOS, searched " << iter << " iterations " << endl; 

  // now get the information set and sample from the avg. strategy

  unsigned long long infosetkey = 0;
  int action = -1;
  double sampleProb = 0;
  
  // count the number of actions. Can't call Bluff as first action when there is no bid
  int maxBid = (match_gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - match_gs.curbid; 
  assert(actionshere > 0);

  Infoset is; 
  getInfoset(match_gs, player, match_bidseq, is, infosetkey, actionshere);

  sampleMoveAvg(is, actionshere, action, sampleProb); 

  assert(action >= 0 && action < actionshere);
  CHKPROBNZ(sampleProb);

  int bid = match_gs.curbid + 1 + action;

  cout << "OOS chose bid " << bid << " with prob " << sampleProb << endl;

  return bid; 
}

#if 0
int main(int argc, char ** argv)
{
  unsigned long long maxIters = 0; 
  unsigned long long maxNodesTouched = 0; 
  init();

  if (argc < 2)
  {
    initInfosets();
    cout << "Created infosets. Exiting." << endl;
    exit(0);
  }
  else
  {
    cout << "Reading infosets from " << argv[1] << endl;
    if (!iss.readFromDisk(argv[1]))
    {
      cerr << "Problem reading file. " << endl; 
      exit(-1); 
    }

    if (argc >= 3) 
      runname = argv[2];
    else
      runname = "bluff11";
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
    
  // try loading metadeta. files have form scratch/iss-runname.iter.dat
  if (iter > 1) { 
    string filename2 = filename;
    replace(filename2, "iss", "metainfo"); 
    cout << "Loading metadata from file " << filename2 << "..." << endl;
    loadMetaData(filename2); 
  }
  

  unsigned long long bidseq = 0; 
    
  double totaltime = 0; 
  StopWatch stopwatch;

  for (; true; iter++)
  {
    GameState gs1; bidseq = 0;
    double suffixreach = 1.0; 
    double rtlSampleProb = 1.0; 
    cfroos(gs1, 1, 0, bidseq, 1.0, 1.0, 1.0, 1.0, 1, suffixreach, rtlSampleProb, true);
    
    GameState gs2; bidseq = 0;
    suffixreach = 1.0; 
    rtlSampleProb = 1.0; 
    cfroos(gs1, 1, 0, bidseq, 1.0, 1.0, 1.0, 1.0, 2, suffixreach, rtlSampleProb, true);

    if (   (maxNodesTouched > 0 && nodesTouched >= maxNodesTouched)
        || (maxNodesTouched == 0 && nodesTouched >= ntNextReport))
    {
      totaltime += stopwatch.stop();

      double nps = nodesTouched / totaltime; 
      cout << endl;      
      cout << "total time: " << totaltime << " seconds. " << endl;
      cout << "nodes = " << nodesTouched << endl;
      cout << "nodes per second = " << nps << endl; 
      cout << "expansions = " << expansions << endl;
      
      // again the bound here is weird for sampling versions
      double b1 = 0.0, b2 = 0.0;
      iss.computeBound(b1, b2); 
      cout << "b1 = " << b1 << ", b2 = " << b2 << ", sum = " << (b1+b2) << endl;

      ntNextReport *= ntMultiplier; // need this here, before dumping metadata

      double conv = 0;
      conv = computeBestResponses(false, false);
      
      cout << "**alglne: iter = " << iter << " nodes = " << nodesTouched << " conv = " << conv << " time = " << totaltime << endl; 

      string str = "cfroos." + runname + ".report.txt"; 
      report(str, totaltime, 2.0*MAX(b1,b2), 0, 0, conv);
      //dumpInfosets("iss-" + runname); 
      //dumpMetaData("metainfo-" + runname, totaltime); 
      
      cout << "Report done at: " << getCurDateTime() << endl;

      cout << endl;
      
      stopwatch.reset(); 
    
      if (maxNodesTouched > 0 && nodesTouched >= maxNodesTouched) 
        break;
    
      if (iter == maxIters) break;
    }
  }
  
  return 0;
}
#endif


