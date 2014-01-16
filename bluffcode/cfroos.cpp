
#include <iostream>
#include <cassert>
#include <cstdlib>

#include "bluff.h"

using namespace std; 

static string runname = "";

static int expansions = 0; 

static double oos_delta = 0.9; 
static double oos_epsilon = 0.6;

// 1 = IST
// 2 = PST
static int oos_variant = 1; 

bool isMatchPrefix(GameState & match_gs, unsigned long long match_bidseq, GameState & gs, unsigned long long bidseq, 
                   int player, int updatePlayer) { 
  if (oos_variant == 1) { 
    // for IST, we are in the prefix stage if: 
    // - update player's chance outcome is the same, 
    // - all the bids are the same up to here
    // else, we're in the tail stage
 
    int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
    int match_curbid = match_gs.curbid; 

    if (updatePlayer == 1 && match_gs.p1roll != gs.p1roll) { 
      return false; 
    }
    else if (updatePlayer == 2 && match_gs.p2roll != gs.p2roll) { 
      return false; 
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
        return true;
      }
      else {
        return false;
      }
    }
    
  }
  else { 
    cerr << "isMatchPrefix unimplemented for this oos variant." << endl;
    exit(-1); 
  }

  return false;
}

int getMatchAction(GameState & match_gs, unsigned long long match_bidseq, GameState & gs, int player, int updatePlayer) { 
  // get the consistent action (bid)
  int action = -1; 

  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 
      
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

  return action;
}

// mode = 1 -> along the match
// mode = 2 -> post match
// mode = 3 -> off match

double cfroos(GameState & match_gs, unsigned long long match_bidseq, int match_player,  
              GameState & gs, int player, int depth, unsigned long long bidseq, 
              double reach1, double reach2, double sprob_bs, double sprob_us, bool biasedSample, int mode, int updatePlayer, 
              double & suffixreach, double & rtlSampleProb, bool treePhase)
{
  CHKPROB(reach1); 
  CHKPROB(reach2); 

  // check: at terminal node?
  if (terminal(gs))
  {
    suffixreach = 1.0; 
    rtlSampleProb = oos_delta*sprob_bs + (1.0-oos_delta)*sprob_us;
    
    return payoff(gs, updatePlayer); 
  }
  
  nodesTouched++;

  // chance nodes. 

  if (gs.p1roll == 0) 
  {
    GameState ngs = gs; 
    double new_bs = sprob_bs;
    double new_us = sprob_us;

    if (biasedSample && match_player == 1) {
      // biased. force roll
      ngs.p1roll = match_gs.p1roll; 
      new_us *= getChanceProb(1, match_gs.p1roll); 
    }
    else { 
      // regular os
      double sampleProb = 0;
      sampleChanceEvent(1, ngs.p1roll, sampleProb);
      CHKPROBNZ(sampleProb);
      
      // need to keep track when we go off the match path
      if (match_player == 1 && ngs.p1roll != match_gs.p1roll) {
        new_bs = 0; // off path
        mode = 3;
      }
      
      new_us *= sampleProb;
    }

    assert(ngs.p1roll > 0);

    return cfroos(match_gs, match_bidseq, match_player, ngs, player, depth+1, bidseq, reach1, reach2, 
                  new_bs, new_us, biasedSample, mode, updatePlayer, suffixreach, rtlSampleProb, treePhase); 
  }
  else if (gs.p2roll == 0)
  {
    GameState ngs = gs; 
    double new_bs = sprob_bs;
    double new_us = sprob_us;

    if (biasedSample && match_player == 2) {
      // biased. force roll
      ngs.p2roll = match_gs.p2roll; 
      new_us *= getChanceProb(2, match_gs.p2roll); 
    }
    else { 
      // regular os
      double sampleProb = 0;
      sampleChanceEvent(2, ngs.p2roll, sampleProb);
      CHKPROBNZ(sampleProb);
     
      // need to keep track when we go off the match path
      if (match_player == 2 && ngs.p2roll != match_gs.p2roll) {        
        new_bs = 0; // off path
        mode = 3;
      }
      
      new_us *= sampleProb;
    }

    assert(ngs.p2roll > 0);
    
    // don't need to worry about keeping track of sampled chance probs for outcome sampling
    return cfroos(match_gs, match_bidseq, match_player, ngs, player, depth+1, bidseq, reach1, reach2, 
                  new_bs, new_us, biasedSample, mode, updatePlayer, suffixreach, rtlSampleProb, treePhase); 
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

  // check if we're still along the match
  if (mode == 1) { 
    bool prefix = isMatchPrefix(match_gs, match_bidseq, gs, bidseq, player, updatePlayer); 
    if (!prefix) 
      mode = 2; 
  }
  
  CHKPROB(sprob_bs);
  CHKPROBNZ(sprob_us); 

  double new_bs = sprob_bs; 
  double new_us = sprob_us; 

  // sample the action! .. and adjust sample probabilities
  //takeAction = oosSampleAction(match_gs, match_bidseq, gs, bidseq, player, updatePlayer, is, actionshere, sampleprob);

  if (biasedSample && mode == 1) { 
    // still along the match. force the match action
    int matchAction = getMatchAction(match_gs, match_bidseq, gs, player, updatePlayer); 
    takeAction = matchAction;

    // compute what would have been the sample prob for this action
    // FIXME: modify sample probs
  }
  else { 
    // choose like os chooses

    if (player == updatePlayer)
      takeAction = sampleAction(player, is, actionshere, sampleprob, 0.6, false); 
    else
      takeAction = sampleAction(player, is, actionshere, sampleprob, 0.0, false); 

    // FIXME: modify sample probs
  }

  // FIXME: might not have it all correct below here

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

//double cfroos(GameState & match_gs, unsigned long long match_bidseq, int match_player,  
//              GameState & gs, int player, int depth, unsigned long long bidseq, 
//              double reach1, double reach2, double sprob_bs, double sprob_us, bool biasedSample, int mode, int updatePlayer, 
//              double & suffixreach, double & rtlSampleProb, bool treePhase)

  updatePlayerPayoff = cfroos(match_gs, match_bidseq, match_player, ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, 
                              new_bs, new_us, biasedSample, mode, updatePlayer, newsuffixreach, rtlSampleProb, newTreePhase);

  ctlReach = newsuffixreach; 
  itlReach = newsuffixreach*is.curMoveProbs[action]; 
  suffixreach = itlReach;

  // payoff always in view of update player
  double myreach = (player == 1 ? reach1 : reach2); 
  double oppreach = (player == 1 ? reach2 : reach1); 

  double samplereach = oos_delta*sprob_bs + (1.0-oos_delta)*sprob_us;

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
        double inc = (1.0 / samplereach)*myreach*is.curMoveProbs[a];
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
int getMoveOOS(int match_player, GameState match_gs, unsigned long long match_bidseq) { 
  
  unsigned long long bidseq = 0; 
    
  double totaltime = 0; 
  StopWatch sw; 

  iter = 0; 

  cout << "Starting OOS iterations..." << endl;

  for (; true; iter++)
  {
    //cout << "OOS iter = " << iter << endl; 
//double cfroos(GameState & match_gs, unsigned long long match_bidseq, int match_player,  
//              GameState & gs, int player, int depth, unsigned long long bidseq, 
//              double reach1, double reach2, double sprob_bs, double sprob_us, bool biasedSample, int mode, int updatePlayer, 
//              double & suffixreach, double & rtlSampleProb, bool treePhase)
    bool biasedSample = drand48() < oos_delta; 

    GameState gs1; bidseq = 0;
    double suffixreach = 1.0; 
    double rtlSampleProb = 1.0; 
    cfroos(match_gs, match_bidseq, match_player, gs1, 1, 0, bidseq, 1.0, 1.0, 1.0, 1.0, biasedSample, 1, 1, suffixreach, rtlSampleProb, true);
    
    GameState gs2; bidseq = 0;
    suffixreach = 1.0; 
    rtlSampleProb = 1.0; 
    cfroos(match_gs, match_bidseq, match_player, gs1, 1, 0, bidseq, 1.0, 1.0, 1.0, 1.0, biasedSample, 2, 2, suffixreach, rtlSampleProb, true);

    if (sw.stop() > timeLimit) 
      break;
  }

  cout << "OOS, searched " << iter << " iterations, in " << sw.stop() << " seconds " << endl; 

  // now get the information set and sample from the avg. strategy

  unsigned long long infosetkey = 0;
  int action = -1;
  double sampleProb = 0;
  
  // count the number of actions. Can't call Bluff as first action when there is no bid
  int maxBid = (match_gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - match_gs.curbid; 
  assert(actionshere > 0);

  Infoset is; 
  getInfoset(match_gs, match_player, match_bidseq, is, infosetkey, actionshere);

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


