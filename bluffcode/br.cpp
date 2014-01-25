
#include <cassert>
#include <iostream>
#include <cstdlib>
#include <cstring>

#include "normalizer.h"
#include "fvector.h"
#include "bluff.h"
#include "seqstore.h"
#include "statcounter.hpp"

static bool mccfrAvgFix = false;
static bool cfrbr = false;
static bool twofiles = false;
static bool ismcts = false;
static InfosetStore * issPtr = &iss;

bool uctbr = false;
#if FSICFR
static float flzero = static_cast<float>(0);
#endif 

extern int RECALL;
extern bool absAvgFullISS;

// used to store average strategy when running with abstraction
extern InfosetStore issfull;

InfosetStore briss2;
extern SequenceStore seqstore;

int br_stitchingPlayer = 0;
int stitchedInfosets = 0;
static StopWatch stopwatch;

using namespace std; 

static vector<unsigned long long> oppChanceOutcomes; // all roll outcomes

void setBRTwoFiles() { 
  twofiles = true;
}

void brImportAbsStrategy(std::string file)
{
  InfosetStore abs_iss; 
  cout << "Reading abstract infoset file: " << file << endl;
  if (!abs_iss.readFromDisk(file))
  {
    cerr << "Problem reading file. " << endl; 
    exit(-1); 
  }

  cout << "Importing asbtract strategy..." << endl;
  iss.absImportStrategy(abs_iss);
}

#if FSICFR
extern int RECALL;

void fsiGetInfoset(unsigned long long & infosetkey, Infoset & is, GameState gs, unsigned long long bidseq, int player,  
                   int actionshere, int chanceOutcome, int updatePlayer)
{
  int j = RECALL-1; // position in sequence

  assert(chanceOutcome >= 1 && chanceOutcome <= P1CO);
  
  unsigned int sequence[RECALL]; 
  for (int i = 0; i < RECALL; i++) sequence[i] = 0;

  unsigned long long mask = 1ULL; 
  int i = 0; // iterations of loop below

  do
  {
    if (bidseq & mask) {
      // match!
      assert(i != 0);
      unsigned int bid = BLUFFBID - i; 
      sequence[j] = bid; 
      j--;       
    }

    i++;
    mask <<= 1; 
  }
  while (j >= 0 && i < BLUFFBID); 
   
  //Sampled version
  /*unsigned long long fsikey = fsiGetInfosetKey(player == 1, chanceOutcome, sequence);
  infosetkey = fsikey;
  bool ret = iss.get(fsikey, is, actionshere, 0);
  assert(ret);*/
 
  // SS version
  //SSEntry ssentry; 
  unsigned long long sskey = fsiGetSSKey(player == 1, sequence);
  bool ret = seqstore.get(sskey, is, actionshere, 0, chanceOutcome);
  assert(ret);
  //is = ssentry.infosets[chanceOutcome-1];

  //for (int a = 0; a < is.actionshere; a++) { 
  //  cout << "totalMoveProbs[a] = " << is.totalMoveProbs[a] << endl;
  //}
}
#endif


// only called at opponent (fixed player) nodes in computeActionDist
// get the information set for update player where the chance outcome is replaced by the specified one
void getInfoset(unsigned long long & infosetkey, Infoset & is, GameState gs, unsigned long long bidseq, int player,  
                int actionshere, int chanceOutcome, int updatePlayer)
{
  #if FSICFR
  //fsiGetInfoset(infosetkey, is, gs, bidseq, player, actionshere, chanceOutcome, updatePlayer); 
  //return; 
  unsigned long long sskey = 0;
  getAbsSSKey(gs, sskey, player, bidseq);
  bool retb = seqstore.get(sskey, is, actionshere, 0, chanceOutcome);
  assert(retb);
  return;
  #endif
  
  infosetkey = bidseq << iscWidth; 
  infosetkey |= chanceOutcome; 
  infosetkey <<= 1; 
  if (player == 2) infosetkey |= 1; 

  bool ret = false; 
  
  if (br_stitchingPlayer > 0) { 
    
    InfosetStore & myISS = (player == 1 ? fsiss1 : fsiss2); 

    if (player != br_stitchingPlayer) { 
      ret = myISS.get(infosetkey, is, actionshere, 0); 
      assert(ret); 
      assert(false); // should not get here
      return;
    }
    else {
      unsigned long long uniqueId = myISS.getPosFromIndex(infosetkey);
      int type = (player == 1 ? p1type : p2type);

      ostringstream oss; 
      oss << "scratch/stitched-iss-" << player << "-" << type << "-" << uniqueId << ".dat"; 

      string filename = oss.str(); 
      bool succ = myISS.readFromDisk(filename, false);  // no allocate!
      assert(succ); 

      succ = myISS.get(infosetkey, is, actionshere, 0); 
      assert(succ); 
      
      stitchedInfosets++; 
      double isPerSec = stitchedInfosets / stopwatch.stop();
      double remaining = (147420 - stitchedInfosets)/isPerSec;     

      cout << "Stitching player = " << br_stitchingPlayer << ", infosets looked up: " << stitchedInfosets 
           << ", ispersec = " << isPerSec << ", remaining seconds = " << remaining << endl;

      return; 
    }
  }

  if (twofiles && player == 2) 
    ret = briss2.get(infosetkey, is, actionshere, 0); 
  else if (useAbstraction && absAvgFullISS)
    ret = issfull.get(infosetkey, is, actionshere, 0); 
  else if (useAbstraction && !absAvgFullISS) {

    // must now replace chance outcome with the one we're given
    GameState ngs = gs;
    if (player == 1) ngs.p1roll = chanceOutcome; else ngs.p2roll = chanceOutcome;
    unsigned long long isk = 0;
    getAbsInfosetKey(ngs, isk, player, bidseq);
    ret = iss.get(isk, is, actionshere, 0); 
  }
  else { 
    ret = issPtr->get(infosetkey, is, actionshere, 0); 
  }

  /*
  unsigned long long sskey = 0;
  sskey = getSSKey(player, bidseq); 
  bool ret = seqstore.get(sskey, is, actionshere, 0, chanceOutcome); 
  infosetkey = sskey;
  */

  if (!ret) cerr << "infoset get failed, infosetkey = " << infosetkey << endl;
  assert(ret);  
}
  
double getMoveProbISMCTS(Infoset & is, int action, int actionshere) { 
  
  int candidates[actionshere]; 
  int ua = 0; 
  double totalVisits = 0;

  for (int i = 0; i < actionshere; i++)
  {
    if (is.totalMoveProbs[i] == 0.0) 
    {
      candidates[ua] = i;
      ua++; 
    }
    else { 
      totalVisits += is.totalMoveProbs[i];
    }
  }

  if (ua == 0) { 
    // MCTS has not searched this node yet, return default
    return (1.0 / static_cast<double>(actionshere)); 
  }
  
  double maxval = -1000.0;
  double tolerance = 0.0000001; 
    
  for (int i = 0; i < actionshere; i++) 
  {
    double val = (is.cfr[i] / is.totalMoveProbs[i]); 

    if (val >= (maxval+tolerance)) {
      // clear better choice
      maxval = val; 
      candidates[0] = i;
      ua = 1; 
    }
    else if (val >= (maxval-tolerance) && val < (maxval+tolerance)) { 
      // tie, add this as a candidate
      candidates[ua] = i; 
      ua++; 
    }
  }

  assert(ua > 0);

  // now, if the action is among the candidates, return 1/#candidates
  // else return 0

  for (int i = 0; i < ua; i++) { 
    if (candidates[i] == action) 
      return (1.0 / static_cast<double>(ua)); 
  }

  return 0.0;
}

double getMoveProb(Infoset & is, int action, int actionshere)
{ 
  // if we're doing a cfrbr best response, it must be to the current strategies 
  if (cfrbr) {
    CHKPROB(is.curMoveProbs[action]);
    return is.curMoveProbs[action];
  }
  else if (ismcts) 
    return getMoveProbISMCTS(is, action, actionshere); 

  double den = 0.0; 
  
  for (int a = 0; a < actionshere; a++)
    if (is.totalMoveProbs[a] > 0.0)
      den += is.totalMoveProbs[a];

  if (den > 0.0) 
    return (is.totalMoveProbs[action] / den); 
  else
    return (1.0 / actionshere);
}

void fixAvStrat(unsigned long long infosetkey, Infoset & is, int actionshere, double myreach)
{
  if (iter > is.lastUpdate)
  {
    for (int a = 0; a < actionshere; a++) 
    {
      double inc =   (iter - is.lastUpdate)
                   * myreach
                   * is.curMoveProbs[a];

      is.totalMoveProbs[a] += inc; 
    }

    iss.put(infosetkey, is, actionshere, 0); 
  }
}


// Compute the weight for this action over all chance outcomes
// Used for determining probability of action
// Done only at fixed_player nodes
void computeActionDist(GameState gs, unsigned long long bidseq, int player, int fixed_player, 
                       NormalizerMap & oppActionDist, int action, FVector<double> & newOppReach, 
                       int actionshere)
{
  int updatePlayer = 3-fixed_player; 
  double weight = 0.0; 

  // for all possible chance outcomes
  for (unsigned int i = 0; i < oppChanceOutcomes.size(); i++) 
  {
    unsigned int CO = (fixed_player == 1 ? numChanceOutcomes(1) : numChanceOutcomes(2));
    assert(oppChanceOutcomes.size() == CO);

    int chanceOutcome = oppChanceOutcomes[i]; 
  
    // get the information set that corresponds to it
    Infoset is;
    unsigned long long infosetkey = 0; 
    getInfoset(infosetkey, is, gs, bidseq, player, actionshere, chanceOutcome, updatePlayer); 

    // apply out-of-date mccfr patch if needed. note: we know it's always the fixed player here
    // NOTE: fixAvStrat needs to be fixed (infosetkey now set to SSkey)
    if (mccfrAvgFix)
      fixAvStrat(infosetkey, is, actionshere, newOppReach[i]); 
   
    double oppProb = getMoveProb(is, action, actionshere); 
    CHKPROB(oppProb); 
    newOppReach[i] = newOppReach[i] * oppProb; 

    weight += getChanceProb(fixed_player, chanceOutcome)*newOppReach[i]; 
  }

  CHKDBL(weight);
  oppActionDist.add(action, weight); 
}

// Compute the weight for this action over all chance outcomes
// Used for determining probability of action
// Done only at fixed_player nodes
void absComputeActionDist(unsigned int * sequence, int player, int fixed_player, 
                          NormalizerMap & oppActionDist, int action, FVector<double> & newOppReach, 
                          int actionshere)
{
  //int updatePlayer = 3-fixed_player; 
  double weight = 0.0; 

  // for all possible chance outcomes
  for (unsigned int i = 0; i < oppChanceOutcomes.size(); i++) 
  {
    unsigned int CO = (fixed_player == 1 ? numChanceOutcomes(1) : numChanceOutcomes(2));
    assert(oppChanceOutcomes.size() == CO);

    int chanceOutcome = oppChanceOutcomes[i]; 
  
    // get the information set that corresponds to it
    Infoset is;
    //unsigned long long infosetkey = 0; 
    //getInfoset(infosetkey, is, gs, bidseq, player, actionshere, chanceOutcome, updatePlayer); 
    unsigned long long sskey = getAbsSSKey(player == 1, sequence);
    bool ret = seqstore.get(sskey, is, actionshere, 0, chanceOutcome);
    assert(ret);

    // apply out-of-date mccfr patch if needed. note: we know it's always the fixed player here
    // NOTE: fixAvStrat needs to be fixed (infosetkey now set to SSkey)
    //if (mccfrAvgFix)
    //  fixAvStrat(infosetkey, is, actionshere, newOppReach[i]); 
   
    double oppProb = getMoveProb(is, action, actionshere); 
    CHKPROB(oppProb); 
    newOppReach[i] = newOppReach[i] * oppProb; 

    weight += getChanceProb(fixed_player, chanceOutcome)*newOppReach[i]; 
  }

  CHKDBL(weight);
  oppActionDist.add(action, weight); 
}


      
// return the payoff of this game state if the opponent's chance outcome is replaced by specified 
double getPayoff(GameState gs, int fixed_player, int oppChanceOutcome)
{
  int updatePlayer = 3-fixed_player; 
  int & oppRoll = (updatePlayer == 1 ? gs.p2roll : gs.p1roll); 
  //oppRoll = oppChanceOutcomes[oppChanceOutcome]; 
  oppRoll = oppChanceOutcome;

  return payoff(gs, updatePlayer); 
}

// return the payoff of this game state if the opponent's chance outcome is replaced by specified 
double getPayoff(unsigned int * sequence, int bidder, int p1roll, int p2roll, int fixed_player, int oppChanceOutcome)
{
  int updatePlayer = 3-fixed_player; 
  
  int & oppRoll = (updatePlayer == 1 ? p2roll : p1roll); 
  oppRoll = oppChanceOutcome;

  //return payoff(gs, updatePlayer); 
  return payoff(sequence[RECALL-2], bidder, 3-bidder, p1roll, p2roll, updatePlayer);
}
      
double expectimaxbr(GameState gs, unsigned long long bidseq, int player, int fixed_player, int depth, FVector<double> & oppReach)
{
  assert(fixed_player == 1 || fixed_player == 2); 

  int updatePlayer = 3-fixed_player;

  // opponent never players here, don't choose this!
  if (player == updatePlayer && oppReach.allEqualTo(0.0))
    return -100000000;
  
  if (terminal(gs))
  {
    if (oppReach.allEqualTo(0.0))
      return -10000000;

    //cout << "Terminal. " << infoset << endl;
    NormalizerVector oppDist; 
  
    for (unsigned int i = 0; i < oppChanceOutcomes.size(); i++) 
      oppDist.push_back(getChanceProb(fixed_player, oppChanceOutcomes[i])*oppReach[i]); 

    oppDist.normalize(); 

    double expPayoff = 0.0; 

    for (unsigned int i = 0; i < oppChanceOutcomes.size(); i++) 
    {
      double payoff = getPayoff(gs, fixed_player, oppChanceOutcomes[i]); 

      CHKPROB(oppDist[i]); 
      CHKDBL(payoff); 

      expPayoff += (oppDist[i] * payoff); 
    }

    //cout << "expected Payoff: " << expPayoff << endl;
    return expPayoff; 
  }
  
  // check for chance node
  if (gs.p1roll == 0) 
  {
    // on fixed player chance nodes, just use any die roll
    if (fixed_player == 1) 
    {
      GameState ngs = gs; 
      ngs.p1roll = 1; // bogus, but never used

      FVector<double> newOppReach = oppReach; 
      return expectimaxbr(ngs, bidseq, player, fixed_player, depth+1, newOppReach);
    }
    else
    {
      double EV = 0.0; 

      for (int i = 1; i <= numChanceOutcomes(1); i++) 
      {
        GameState ngs = gs; 
        ngs.p1roll = i; 

        FVector<double> newOppReach = oppReach; 
        EV += getChanceProb(1,i) * expectimaxbr(ngs, bidseq, player, fixed_player, depth+1, newOppReach);
      }

      return EV;
    }
  }
  else if (gs.p2roll == 0)
  {
    // on fixed player chance nodes, just use any die roll
    if (fixed_player == 2)
    {
      GameState ngs = gs; 
      ngs.p2roll = 1; // bogus, but never used

      FVector<double> newOppReach = oppReach; 
      return expectimaxbr(ngs, bidseq, player, fixed_player, depth+1, newOppReach);
    }
    else
    {
      double EV = 0.0; 

      for (int i = 1; i <= numChanceOutcomes(2); i++)
      {
        GameState ngs = gs; 
        ngs.p2roll = i; 
        
        FVector<double> newOppReach = oppReach; 
        EV += getChanceProb(2,i) * expectimaxbr(ngs, bidseq, player, fixed_player, depth+1, newOppReach);
      }

      return EV;
    }
  }

  // declare variables and get # actions available
  double EV = 0.0; 
  
  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 
  assert(actionshere > 0);

  // iterate over the moves 
  double maxEV = -100000000;
  double childEVs[actionshere];
  int action = -1;
  NormalizerMap oppActionDist;
  //vector<int> maxActions;
  int maxAction = -1;

  for (int i = gs.curbid+1; i <= maxBid; i++) 
  {
    action++;    

    double childEV = 0;
    FVector<double> newOppReach = oppReach;
      
    if (player == fixed_player) 
      computeActionDist(gs, bidseq, player, fixed_player, oppActionDist, action, newOppReach, actionshere); 

    // state transition + recursion
    GameState ngs = gs; 
    ngs.prevbid = gs.curbid;
    ngs.curbid = i; 
    ngs.callingPlayer = player;
    unsigned long long newbidseq = bidseq; 
    newbidseq |= (1ULL << (BLUFFBID-i)); 

    childEV = expectimaxbr(ngs, newbidseq, 3-player, fixed_player, depth+1, newOppReach);
    
    // post recurse
    if (player == updatePlayer) 
    {
      // check if higher than current max
      if (childEV >= maxEV)
      {
        maxEV = childEV;
        
        maxAction = action; 

        //maxActions.clear();
        //maxActions.push_back(action);

        //if (childEV > maxEV) 
        //  maxActions.clear();
        //maxActions.push_back(action);
      }
    }
    else if (player == fixed_player)
    {
      assert(action >= 0 && action < actionshere);
      childEVs[action] = childEV; 
    }
  }

  // post move iteration
  if (player == updatePlayer)
  {
    EV = maxEV;

    // are we saving the max action?
    if (cfrbr) 
    {
      unsigned long long infosetkey = bidseq;
      infosetkey <<= iscWidth;
      if (player == 1)
        infosetkey |= gs.p1roll;
      else 
        infosetkey |= gs.p2roll;
      infosetkey <<= 1; 
      if (player == 2) 
        infosetkey |= 1; 

      Infoset is;
      bool ret = iss.get(infosetkey, is, actionshere, 0); 
      if (!ret) cerr << "infoset get failed, infosetkey = " << infosetkey << endl;
      assert(ret);  

      //unsigned int index = static_cast<unsigned int>(drand48() * maxActions.size());      
      //assert(index >= 0 && index < maxActions.size()); 
      //is.lastUpdate = maxActions[index];

      is.lastUpdate = maxAction;

      iss.put(infosetkey, is, actionshere, 0);
    }
  }
  else if (player == fixed_player)
  {
    assert(static_cast<int>(oppActionDist.size()) == actionshere);
    oppActionDist.normalize(); 
    
    for (int i = 0; i < actionshere; i++) 
    {
      CHKPROB(oppActionDist[i]); 
      CHKDBL(childEVs[i]); 

      EV += (oppActionDist[i] * childEVs[i]); 
    }
    
  }

  assert(updatePlayer != fixed_player); 
  assert(updatePlayer + fixed_player == 3); 

  return EV; 
}

double absexpectimaxbr(int p1roll, int p2roll, unsigned int * sequence, int player, int fixed_player, int depth, FVector<double> & oppReach)
{
  assert(fixed_player == 1 || fixed_player == 2); 

  int updatePlayer = 3-fixed_player;

  // opponent never players here, don't choose this!
  if (player == updatePlayer && oppReach.allEqualTo(0.0))
    return -100000000;
  
  if (sequence[RECALL-1] == BLUFFBID)
  {
    if (oppReach.allEqualTo(0.0))
      return -10000000;

    //cout << "Terminal. " << infoset << endl;
    NormalizerVector oppDist; 
  
    for (unsigned int i = 0; i < oppChanceOutcomes.size(); i++) 
      oppDist.push_back(getChanceProb(fixed_player, oppChanceOutcomes[i])*oppReach[i]); 

    oppDist.normalize(); 

    double expPayoff = 0.0; 

    for (unsigned int i = 0; i < oppChanceOutcomes.size(); i++) 
    {
      //double payoff = getPayoff(gs, fixed_player, oppChanceOutcomes[i]); 
      //double payoff = getPayoff(gs, fixed_player, oppChanceOutcomes[i]); 
      double payoff = getPayoff(sequence, player, p1roll, p2roll, fixed_player, oppChanceOutcomes[i]);

      CHKPROB(oppDist[i]); 
      CHKDBL(payoff); 

      expPayoff += (oppDist[i] * payoff); 
    }

    //cout << "expected Payoff: " << expPayoff << endl;
    return expPayoff; 
  }
  
  // check for chance node
  if (p1roll == 0) 
  {
    // on fixed player chance nodes, just use any die roll
    if (fixed_player == 1) 
    {
      p1roll = 1; // bogus, but never used

      FVector<double> newOppReach = oppReach; 
      return absexpectimaxbr(p1roll, p2roll, sequence, player, fixed_player, depth+1, newOppReach);
    }
    else
    {
      double EV = 0.0; 

      for (int i = 1; i <= numChanceOutcomes(1); i++) 
      {
        p1roll = i; 

        FVector<double> newOppReach = oppReach; 
        EV += getChanceProb(1,i) * absexpectimaxbr(p1roll, p2roll, sequence, player, fixed_player, depth+1, newOppReach);
      }

      return EV;
    }
  }
  else if (p2roll == 0)
  {
    // on fixed player chance nodes, just use any die roll
    if (fixed_player == 2)
    {
      p2roll = 1; // bogus, but never used

      FVector<double> newOppReach = oppReach; 
      return absexpectimaxbr(p1roll, p2roll, sequence, player, fixed_player, depth+1, newOppReach);
    }
    else
    {
      double EV = 0.0; 

      for (int i = 1; i <= numChanceOutcomes(2); i++)
      {
        p2roll = i; 
        
        FVector<double> newOppReach = oppReach; 
        EV += getChanceProb(2,i) * absexpectimaxbr(p1roll, p2roll, sequence, player, fixed_player, depth+1, newOppReach);
      }

      return EV;
    }
  }

  // declare variables and get # actions available
  double EV = 0.0; 

  int curbid = sequence[RECALL-1];
  
  unsigned int maxBid = (curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - curbid; 
  assert(actionshere > 0);

  // iterate over the moves 
  double maxEV = -100000000;
  double childEVs[actionshere];
  int action = -1;
  NormalizerMap oppActionDist;
  //vector<int> maxActions;
  //int maxAction = -1;

  unsigned int origSequence[RECALL];
  for(int r = 0; r < RECALL; r++) origSequence[r] = sequence[r];
  
  unsigned int oldestbid = sequence[0];

  // left-shift to make space for new bid
  for (int j = 0; j < RECALL-1; j++) { sequence[j] = sequence[j+1]; }

  for (unsigned int i = curbid+1; i <= maxBid; i++) 
  {
    // do stuff
    action++;    

    double childEV = 0;
    FVector<double> newOppReach = oppReach;
      
    if (player == fixed_player) 
      absComputeActionDist(origSequence, player, fixed_player, oppActionDist, action, newOppReach, actionshere); 

    // state transition + recursion
    sequence[RECALL-1] = i; 

    childEV = absexpectimaxbr(p1roll, p2roll, sequence, 3-player, fixed_player, depth+1, newOppReach);
    
    // post recurse
    if (player == updatePlayer) 
    {
      // check if higher than current max
      if (childEV >= maxEV)
      {
        maxEV = childEV;
        
        //maxAction = action; 

        //maxActions.clear();
        //maxActions.push_back(action);

        //if (childEV > maxEV) 
        //  maxActions.clear();
        //maxActions.push_back(action);
      }
    }
    else if (player == fixed_player)
    {
      assert(action >= 0 && action < actionshere);
      childEVs[action] = childEV; 
    }
  }

  // put the bid back normal (right-shift)
  
  for (int j = RECALL-1; j >= 1; j--) { sequence[j] = sequence[j-1]; }
  sequence[0] = oldestbid;
  
  // post move iteration
  if (player == updatePlayer)
  {
    EV = maxEV;
  }
  else if (player == fixed_player)
  {
    assert(static_cast<int>(oppActionDist.size()) == actionshere);
    oppActionDist.normalize(); 
    
    for (int i = 0; i < actionshere; i++) 
    {
      CHKPROB(oppActionDist[i]); 
      CHKDBL(childEVs[i]); 

      EV += (oppActionDist[i] * childEVs[i]); 
    }
    
  }

  assert(updatePlayer != fixed_player); 
  assert(updatePlayer + fixed_player == 3); 

  return EV; 
}

double computeHalfBR(int fixed_player, bool abs, bool avgFix, bool _cfrbr)
{
  useAbstraction = abs; 
  mccfrAvgFix = avgFix;
  cfrbr = _cfrbr;

  int CO = (fixed_player == 1 ? numChanceOutcomes(1) : numChanceOutcomes(2)); 

  // fill chance outcomes for opponent
  oppChanceOutcomes.clear();
  for (int i = 1; i <= CO; i++)
    oppChanceOutcomes.push_back(i); 

  GameState gs; 
  unsigned long long bidseq = 0; 
  FVector<double> oppreach(CO, 1.0); 

  //double expectimaxbr(GameState gs, unsigned long long bidseq, int player, int fixed_player, int depth, FVector<double> & oppReach)
  double value = expectimaxbr(gs, bidseq, 1, fixed_player, 0, oppreach);

  return value;
}

double computeBestResponses(bool abs, bool avgFix)
{
  double p1value = 0.0;
  double p2value = 0.0;
  return computeBestResponses(abs, avgFix, p1value, p2value);
}

double computeBestResponses(bool abs, bool avgFix, double & p1value, double & p2value)
{
  useAbstraction = abs; 
  mccfrAvgFix = avgFix;
  cfrbr = false;

  cout << "Computing ISS bounds... "; cout.flush(); 
  double b1 = 0.0, b2 = 0.0;
  iss.computeBound(b1, b2); 
  cout << " b1 = " << b1 << ", b2 = " << b2 << ", bound = " << (2.0*MAX(b1,b2)) << endl;
  
  cout << "Computing SS bounds... "; cout.flush(); 
  b1 = 0.0, b2 = 0.0;
  seqstore.computeBound(b1, b2); 
  cout << " b1 = " << b1 << ", b2 = " << b2 << ", bound = " << (2.0*MAX(b1,b2)) << endl;

  // fill chance outcomes for player 1
  oppChanceOutcomes.clear();
  for (int i = 1; i <= numChanceOutcomes(1); i++)
  {
    oppChanceOutcomes.push_back(i); 
  }

  cout << "Running best response, fp = 1 ... "; cout.flush(); 

  StopWatch sw; 

  GameState gs1; unsigned long long bidseq = 0; 
  FVector<double> reach1(numChanceOutcomes(1), 1.0); 
  p2value = expectimaxbr(gs1, bidseq, 1, 1, 0, reach1);

  cout << "time taken: " << sw.stop() << " seconds." << endl; 
  cout.precision(15);
  cout << "p2value = " << p2value << endl; 

  // fill chance outcomes for player 2
  oppChanceOutcomes.clear();
  for (int i = 1; i <= numChanceOutcomes(2); i++)
  {
    oppChanceOutcomes.push_back(i); 
  }

  cout << "Running best response, fp = 2 ... "; cout.flush(); 

  sw.reset(); 

  GameState gs2; bidseq = 0;
  FVector<double> reach2(numChanceOutcomes(2), 1.0); 
  p1value = expectimaxbr(gs2, bidseq, 1, 2, 0, reach2);
 
  cout << "time taken: " << sw.stop() << " seconds." << endl; 
  cout.precision(15);
  cout << "p1value = " << p1value << endl; 

  double conv = p1value + p2value; 

  cout.precision(15);
  cout << "**brline: iter = " << iter << " nodes = " << nodesTouched << " conv = " << conv << endl; 

  return conv;
}

double searchComputeHalfBR(int fixed_player, InfosetStore * _issPtr, bool _ismcts)
{
  ismcts = _ismcts;
  issPtr = _issPtr;
  stitchedInfosets = 0;
  stopwatch.reset();

  int CO = (fixed_player == 1 ? numChanceOutcomes(1) : numChanceOutcomes(2)); 

  // fill chance outcomes for opponent
  oppChanceOutcomes.clear();
  for (int i = 1; i <= CO; i++)
    oppChanceOutcomes.push_back(i); 

  GameState gs; 
  unsigned long long bidseq = 0; 
  FVector<double> oppreach(CO, 1.0); 

  //double expectimaxbr(GameState gs, unsigned long long bidseq, int player, int fixed_player, int depth, FVector<double> & oppReach)
  double value = expectimaxbr(gs, bidseq, 1, fixed_player, 0, oppreach);

  return value;
}



#if FSICFR
double absComputeBestResponses(bool abs, bool avgFix)
{
  double p1value = 0.0;
  double p2value = 0.0;
  return absComputeBestResponses(abs, avgFix, p1value, p2value);
}

double absComputeBestResponses(bool abs, bool avgFix, double & p1value, double & p2value)
{
  useAbstraction = abs; 
  mccfrAvgFix = avgFix;
  cfrbr = false;

  cout << "Computing SS bounds... "; cout.flush(); 
  double b1 = 0.0, b2 = 0.0;
  seqstore.computeBound(b1, b2); 
  cout << " b1 = " << b1 << ", b2 = " << b2 << ", bound = " << (2.0*MAX(b1,b2)) << endl;

  // fill chance outcomes for player 1
  oppChanceOutcomes.clear();
  for (int i = 1; i <= numChanceOutcomes(1); i++)
  {
    oppChanceOutcomes.push_back(i); 
  }

  cout << "Running best response, fp = 1 ... "; cout.flush(); 

  StopWatch sw; 

  unsigned int sequence[RECALL];
  initSequenceForward(sequence);

  FVector<double> reach1(numChanceOutcomes(1), 1.0); 
  p2value = absexpectimaxbr(0, 0, sequence, 1, 1, 0, reach1);

  cout << "time taken: " << sw.stop() << " seconds." << endl; 
  cout.precision(15);
  cout << "p2value = " << p2value << endl; 

  // fill chance outcomes for player 2
  oppChanceOutcomes.clear();
  for (int i = 1; i <= numChanceOutcomes(2); i++)
  {
    oppChanceOutcomes.push_back(i); 
  }

  cout << "Running best response, fp = 2 ... "; cout.flush(); 

  sw.reset(); 

  initSequenceForward(sequence);
  FVector<double> reach2(numChanceOutcomes(2), 1.0); 
  p1value = absexpectimaxbr(0, 0, sequence, 1, 2, 0, reach2);
 
  cout << "time taken: " << sw.stop() << " seconds." << endl; 
  cout.precision(15);
  cout << "p1value = " << p1value << endl; 

  double conv = p1value + p2value; 

  cout.precision(15);
  cout << "iter = " << iter << " nodes = " << nodesTouched << " absconv = " << conv << endl; 

  return conv;
}

void estimateValue()
{
  StatCounter sc; 
  unsigned long long games = 0;

  while (true)
  {
    GameState gs;
    
    double prob = 0;
    sampleChanceEvent(1, gs.p1roll, prob);
    sampleChanceEvent(2, gs.p2roll, prob);

    unsigned long long bidseq = 0; 
    int player = 1; 

    do
    {
      Infoset is;
      unsigned long long sskey = 0;
    
      int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
      int actionshere = maxBid - gs.curbid; 

      getAbsSSKey(gs, sskey, player, bidseq);
      bool ret = seqstore.get(sskey, is, actionshere, 0, (player == 1 ? gs.p1roll : gs.p2roll));
      assert(ret);

      int action = 0;
      double prob = 0;
      sampleMoveAvg(is, actionshere, action, prob);

      assert(action >= 0 && action < actionshere);

      int newbid = gs.curbid + 1 + action;

      unsigned long long newbidseq = bidseq;

      GameState ngs = gs; 
      ngs.prevbid = gs.curbid;
      ngs.curbid = newbid;
      ngs.callingPlayer = player;
      newbidseq |= (1ULL << (BLUFFBID-newbid)); 
      
      bidseq = newbidseq;
      gs = ngs; 
      player = 3-player;
    }
    while(!terminal(gs)); 

    sc.push(payoff(gs, 1));
    games++; 

    if (games % 1000000 == 0) 
      cout << "Games = " << games << "; mean payoff = " << sc.mean() << " +/- " << sc.ci95() << " (95\% ci)" << endl;
  }
}

void loadUCTValues(Infoset & is, int actions)
{
  assert(sizeof(float) == sizeof(double)/2); 

  for (int a = 0; a < actions; a++) 
  {
    float T = 0;
    float X = 0;

    double value = is.cfr[a];    
    char * addr = reinterpret_cast<char*>(&value); 
          
    memcpy(&T, &value, sizeof(float)); 
    memcpy(&X, static_cast<void *>(addr + sizeof(float)), sizeof(float)); 

    is.T[a] = T;
    is.X[a] = X;
  }
}

void saveUCTValues(Infoset & is, int actions)
{
  assert(sizeof(float) == sizeof(double)/2); 
  
  for (int a = 0; a < actions; a++) 
  {
    float T = is.T[a];
    float X = is.X[a];

    double value = 0;
    char * addr = reinterpret_cast<char *>(&value); 
          
    memcpy(&value, &T, sizeof(float)); 
    memcpy(static_cast<void *>(addr + sizeof(float)), &X, sizeof(float)); 

    is.cfr[a] = value; 
  }
}

double UCTBR(GameState & gs, int player, unsigned long long bidseq, int fixed_player, bool chooseMaxNoUpdates)
{
  if (terminal(gs))
    return payoff(gs, 1); // always return payoff in view of p1

  Infoset is;
  unsigned long long sskey = 0;

  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 

  int action = 0;

  if (player != fixed_player)
  {
    getAbsSSKey(gs, sskey, player, bidseq);
    bool ret = seqstore.get(sskey, is, actionshere, 0, (player == 1 ? gs.p1roll : gs.p2roll));
    assert(ret);

    // select using UCT
    loadUCTValues(is, actionshere); 
   
    vector<int> untaken;
    float T = 0;

    for (int a = 0; a < actionshere; a++) {
      if (is.T[a] == flzero) 
        untaken.push_back(a); 
      T += is.T[a];
    }

    if (untaken.size() > 0) { 
      unsigned int idx = static_cast<unsigned int>(drand48() * untaken.size()); 
      assert(idx < untaken.size()); 
      action = untaken[idx]; 
    }
    else {
      
      float bestVal = -1; // values will be in 0-2
    
      for (int a = 0; a < actionshere; a++) 
      {
        // C = 1.0
        //float expl = (player == 1 ? sqrt(log(T) / is.T[a]) : -sqrt(log(T) / is.T[a])); 
        float expl = sqrt(log(T) / is.T[a]);
        float uctVal = (chooseMaxNoUpdates ? (is.X[a] / is.T[a])
                        : (is.X[a] / is.T[a] + expl)); 

        if (uctVal > bestVal) {
          bestVal = uctVal;
          action = a; 
        }
        /*else if (player == 2 && uctVal < bestVal)
        {
          bestVal = uctVal;
          action = a; 
        }*/
      }
    }
  }
  else 
  {
    getAbsSSKey(gs, sskey, player, bidseq);
    bool ret = seqstore.get(sskey, is, actionshere, 0, (player == 1 ? gs.p1roll : gs.p2roll));
    assert(ret);

    // Select by polling the average

    double prob = 0;
    sampleMoveAvg(is, actionshere, action, prob);
  }

  assert(action >= 0 && action < actionshere);

  int newbid = gs.curbid + 1 + action;
  unsigned long long newbidseq = bidseq;

  GameState ngs = gs; 
  ngs.prevbid = gs.curbid;
  ngs.curbid = newbid;
  ngs.callingPlayer = player;
  newbidseq |= (1ULL << (BLUFFBID-newbid)); 

  double retVal = UCTBR(ngs, 3-player, newbidseq, fixed_player, chooseMaxNoUpdates);

  if (!chooseMaxNoUpdates)
  {
    float reward = static_cast<float>(retVal + 1.0); // 0-2
    assert(reward >= 0.0 && reward <= 2.0);

    if (player == 2) reward = 2.0-reward;

    assert(reward >= 0.0 && reward <= 2.0);

    is.X[action] += reward;
    is.T[action] += 1.0;

    saveUCTValues(is, actionshere); 
    seqstore.put(sskey, is, actionshere, 0, (player == 1 ? gs.p1roll : gs.p2roll));
  }

  return retVal;
  
}

void UCTBR(int fixed_player)
{
  StatCounter sc; 
  unsigned long long games = 0;
  seqstore.zeroCFR(); 

  while (true)
  {
    GameState gs;
    
    double prob = 0;
    sampleChanceEvent(1, gs.p1roll, prob);
    sampleChanceEvent(2, gs.p2roll, prob);

    unsigned long long bidseq = 0; 
    int player = 1; 

    double val = UCTBR(gs, player, bidseq, fixed_player, false); 

    sc.push(fixed_player == 2 ? val : -val); 

    games++; 

    if (games % 1000000 == 0) 
      cout << "Games = " << games << "; mean payoff = " << sc.mean() << " +/- " << sc.ci95() << " (95\% ci)" << endl;

    if (games % 10000000 == 0) 
    { 
      StatCounter sc2;

      for (int evalgames = 0; evalgames < 10000000; evalgames++) 
      {
        GameState gs;
        
        double prob = 0;
        sampleChanceEvent(1, gs.p1roll, prob);
        sampleChanceEvent(2, gs.p2roll, prob);

        unsigned long long bidseq = 0; 
        int player = 1; 

        double val = UCTBR(gs, player, bidseq, fixed_player, true); 

        sc2.push(fixed_player == 2 ? val : -val); 
      }

      cout << "Eval games = " << games << "; mean payoff = " << sc2.mean() << " +/- " << sc2.ci95() << " (95\% ci)" << endl;
    }
  }
}


#endif

