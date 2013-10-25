
#include <iostream>
#include <cassert>
#include <cstdlib>

#include "bluff.h"

using namespace std; 

static double ** moveVar; 

static bool probing = false;
static string variant = "";
static string runname = "";

void allocMoveVar() {
  cout << "allocation move var" << endl;
  moveVar = new double* [ISKMAX];
  for (int i = 0; i < ISKMAX; i++) { 
    moveVar[i] = new double[ACTMAX];
  }
}

void deallocMoveVar() { 
  for (int i = 0; i < ISKMAX; i++) { 
    delete [] moveVar[i];
  }
  delete [] moveVar;
}


void loadVarianceStats() {
  cout << "Loading variance statistics..." << endl;
  ifstream in("variance-bin.dat", ios::binary); 
  allocMoveVar(); 

  if (!in.is_open()) {
    cerr << "Could not open variance-bin.dat" << endl;
    exit(-1); 
  
    unsigned long long infosetkey;
    unsigned long long action;
    double var;

    in.read(reinterpret_cast<char *>(&infosetkey), 8); 
    in.read(reinterpret_cast<char *>(&action), 8); 
    in.read(reinterpret_cast<char *>(&var), 8); 

    assert(infosetkey < 131072);
    assert(action < 13);
    assert(var >= 0);

    moveVar[infosetkey][action] = var;
  }

  in.close();
}

// is this action a low-variance action?
bool isLVA(unsigned long long infosetkey, int action) {
  assert(infosetkey < 131072);
  assert(action < 13); 
  return (moveVar[infosetkey][action] <= 0.1);
}

/* sample actions individually with a Bernoulli dist */
void sampleActionsBern(vector<int> & sampledActions, 
                       vector<double> & sampledProbs, 
                       vector<bool> & sampledFlags, 
                       int curbid, 
                       int maxBid)
{
  for (int i = curbid+1; i <= maxBid; i++)
  {
    double roll = drand48();
    if (roll < 0.5) 
    {
      sampledActions.push_back(i);
      sampledFlags.push_back(true);
      sampledProbs.push_back(0.5);
    }
    else
    {
      sampledActions.push_back(i);
      sampledFlags.push_back(false);
      sampledProbs.push_back(0);
    }
  }
}

// suppose I roll X.
//
// always sample 1-X, 1-5, 1-6, and 2-X. 
// sample the rest individually with 0.5
void sampleActions11(vector<int> & sampledActions, 
                     vector<double> & sampledProbs, 
                     vector<bool> & sampledFlags, 
                     int curbid, 
                     int maxBid, 
                     int myChanceOutcome)
{
  for (int i = curbid+1; i <= maxBid; i++)
  {
    int dice = 0, face = 0;
    
    if (i != maxBid) 
      convertbid(dice, face, i);

    if (   (dice == 1 && face == myChanceOutcome)    // 1-X
        || (dice == 1 && face == 5)                  // 1-5
        || (dice == 1 && face == 6)                  // 1-6
        || (dice == 2 && face == myChanceOutcome)
        || i == 13 )  // 2-X
    {
      sampledActions.push_back(i);
      sampledFlags.push_back(true);
      sampledProbs.push_back(1.0);
    }
    else
    {
      double roll = drand48();
      if (roll < 0.5)
      {
        sampledActions.push_back(i);
        sampledFlags.push_back(true);
        sampledProbs.push_back(0.5);
      }
      else
      {
        sampledActions.push_back(i);
        sampledFlags.push_back(false);
        sampledProbs.push_back(0.0);
      }
    }
  }
}

// suppose I roll XY
// always sample N-X and N-Y where 1 <= N <= (P1DICE + P2DICE)
// 1-5, 1-6, 2-5, 2-6
void sampleActions(vector<int> & sampledActions, 
                   vector<double> & sampledProbs, 
                   vector<bool> & sampledFlags, 
                   int curbid, 
                   int maxBid, 
                   int myChanceOutcome, 
                   int player)
{
  int roll[3] = { 0, 0, 0 };
  int x = 0, y = 0;
  getRoll(roll, myChanceOutcome, player); 
  for (int i = 0; i < 3; i++) { 
    if (roll[i] != 0 && x == 0) x = roll[i];
    else if (roll[i] != 0 && y == 0) y = roll[i];
  }
  
  assert(x != 0 && y != 0);

  for (int i = curbid+1; i <= maxBid; i++)
  {
    int dice = 0, face = 0;
    
    if (i != maxBid) 
      convertbid(dice, face, i);

    if (   (face == x)                             // N-X
        || (face == y)                             // N-Y
        || (dice >= 1 && dice <= 2 && face == 5)   // 1-5 and 2-5
        || (dice >= 1 && dice <= 2 && face == 6))  // 1-6 and 2-6
    {
      sampledActions.push_back(i);
      sampledFlags.push_back(true);
      sampledProbs.push_back(1.0);
    }
    else
    {
      double roll = drand48();
      if (roll < 0.5)
      {
        sampledActions.push_back(i);
        sampledFlags.push_back(true);
        sampledProbs.push_back(0.5);
      }
      else
      {
        sampledActions.push_back(i);
        sampledFlags.push_back(false);
        sampledProbs.push_back(0.0);
      }
    }
  }
}

/* sample a single action */
void sampleSingleActionsEpsOnPolicy(vector<int> & sampledActions, 
                                    vector<double> & sampledProbs, 
                                    vector<bool> & sampledFlags, 
                                    int curbid, 
                                    int maxBid, 
                                    Infoset & is, 
                                    int actionshere, 
                                    double epsilon)
{
  double roll = drand48();
  double sum = 0.0;
  int hits = 0;
  int action = -1;

  for (int i = curbid+1; i <= maxBid; i++) 
  { 
    action++;
    assert(action < actionshere);

    double prob =          epsilon * (1.0 / actionshere) 
                  + (1.0 - epsilon) * is.curMoveProbs[action]; 

    if (roll >= sum && roll < sum + prob) 
    {
      // this is the sampled move
      sampledActions.push_back(i);
      sampledProbs.push_back(prob); 
      sampledFlags.push_back(true);    
      hits++;
    }
    else
    {
      sampledActions.push_back(i);
      sampledProbs.push_back(0.0);
      sampledFlags.push_back(false);
    }

    sum += prob;
    assert(sum <= 1.00000000001); 
  }
  
  assert(hits == 1); 
}


static double probe(GameState & gs, int player, int depth, unsigned long long bidseq, int updatePlayer)
{
  // check: at terminal node?
  if (terminal(gs))
  {
    double ui_z = 0.0;
    //double q_z = sprob1 * sprob2; 
    //CHKPROBNZ(q_z);

    int pwinner = whowon(gs); 
    if (pwinner == updatePlayer) 
      ui_z = 1.0; 
    else if (pwinner == (3-updatePlayer))
      ui_z = -1.0; 
   
    // when probing is enabled, don't correct 

    return ui_z;       
  }
  
  Infoset is;
  unsigned long long infosetkey = 0;
  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 
  
  // get the info set from the info set store (iss) 
  // infoset key is the (bid sequence) (my roll) (0|1) 
  infosetkey = bidseq;  
  infosetkey <<= iscWidth; 
  if (player == 1)
  {
    infosetkey |= gs.p1roll; 
    infosetkey <<= 1; 
    bool ret = iss.get(infosetkey, is, actionshere, 0); 
    assert(ret);
  }
  else if (player == 2)
  {
    infosetkey |= gs.p2roll; 
    infosetkey <<= 1; 
    infosetkey |= 1; 
    bool ret = iss.get(infosetkey, is, actionshere, 0); 
    assert(ret);
  }

  double sampleProb;
  int action = sampleAction(player, is, actionshere, sampleProb, 0.0, false);
  assert(action >= 0 && action < actionshere);
  //double lva = isLVA(infosetkey, action);

  //double lvaSampleProb = 0.5;
  //double newsprob1 = ( (lva && player == 1) ? lvaSampleProb*sprob1 : sprob1); 
  //double newsprob2 = ( (lva && player == 2) ? lvaSampleProb*sprob2 : sprob2); 

  GameState ngs = gs; 
  ngs.prevbid = gs.curbid;
  int i = gs.curbid+action+1;
  ngs.curbid = i;
  ngs.callingPlayer = player;
  unsigned long long newbidseq = bidseq;
  newbidseq |= (1 << (BLUFFBID-i)); 

  return probe(ngs, 3-player, depth+1, newbidseq, updatePlayer);
  //return is.curMoveProbs[action]*probe(ngs, 3-player, depth+1, newbidseq, newsprob1, newsprob2, updatePlayer);
}

double cfrprb(GameState & gs, int player, int depth, unsigned long long bidseq, 
              double reach1, double reach2, double sprob1, double sprob2, int updatePlayer)
{
  CHKPROB(reach1); 
  CHKPROB(reach2); 

  // check: at terminal node?
  if (terminal(gs))
  {
    double ui_z = 0.0;
    double q_z = sprob1 * sprob2; 
    CHKPROBNZ(q_z);

    int pwinner = whowon(gs); 
    if (pwinner == updatePlayer) 
      ui_z = 1.0; 
    else if (pwinner == (3-updatePlayer))
      ui_z = -1.0; 
   
    return (ui_z / q_z); 
  }

  nodesTouched++;

  // chance nodes
  if (gs.p1roll == 0) 
  {
    GameState ngs = gs; 
    double sampleProb;
    sampleChanceEvent(1, ngs.p1roll, sampleProb);

    // don't need to worry about keeping track of sampled chance probs for outcome sampling
    return cfrprb(ngs, player, depth+1, bidseq, reach1, reach2, sprob1, sprob2, updatePlayer); 
  }
  else if (gs.p2roll == 0)
  {
    GameState ngs = gs; 
    double sampleProb;
    sampleChanceEvent(2, ngs.p2roll, sampleProb);

    // don't need to worry about keeping track of sampled chance probs for outcome sampling
    return cfrprb(ngs, player, depth+1, bidseq, reach1, reach2, sprob1, sprob2, updatePlayer); 
  }

  //assert(gs.p1roll >= 1 && gs.p1roll <= 6);
  //assert(gs.p2roll >= 1 && gs.p2roll <= 6);

  // declare the variables
  Infoset is;
  unsigned long long infosetkey = 0;
  int action = -1;

  // count the number of actions. Can't call Bluff as first action when there is no bid
  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 
  // count the number of actions and initialize moveEVs
  // special case. can't just call bluff on the first move!
  assert(actionshere > 0);
  double moveEVs[actionshere]; 
  double stratEV = 0.0;

  // get the info set from the info set store (iss) 
  // infoset key is the (bid sequence) (my roll) (0|1) 
  infosetkey = bidseq;  
  infosetkey <<= iscWidth; 
  if (player == 1)
  {
    infosetkey |= gs.p1roll; 
    infosetkey <<= 1; 
    bool ret = iss.get(infosetkey, is, actionshere, 0); 
    assert(ret);
  }
  else if (player == 2)
  {
    infosetkey |= gs.p2roll; 
    infosetkey <<= 1; 
    infosetkey |= 1; 
    bool ret = iss.get(infosetkey, is, actionshere, 0); 
    assert(ret);
  }

  // note: regret-matching is done when we pull this infoset out of the infoset store
  // so here, the infoset will have the proper strategy distribution 

  // flip a coin. On heads, sample all the low-variance actions. On tails, do not sample any of them. 
  // always include the high variance actions

  vector<int> sampledActions;
  vector<double> sampledProbs;
  vector<bool> sampledFlags;
  int myChanceOutcome = (player == 1 ? gs.p1roll : gs.p2roll);

  if (player == updatePlayer)
    sampleActions11(sampledActions, sampledProbs, sampledFlags, gs.curbid, maxBid, myChanceOutcome);
    //sampleActionsBern(sampledActions, sampledProbs, sampledFlags, gs.curbid, maxBid);
    //sampleActionsBern(sampledActions, sampledProbs, sampledFlags, gs.curbid, maxBid);
/*  if (player == updatePlayer) 
  {
    if (variant == "os") 
      sampleSingleActionsEpsOnPolicy(sampledActions, sampledProbs, sampledFlags, gs.curbid, maxBid, is, actionshere, 0.6);
    else 
      sampleActions(sampledActions, sampledProbs, sampledFlags, gs.curbid, maxBid, myChanceOutcome, player);
  }*/
  else
    sampleSingleActionsEpsOnPolicy(sampledActions, sampledProbs, sampledFlags, gs.curbid, maxBid, is, actionshere, 0.0);

  for (int i = gs.curbid+1; i <= maxBid; i++) 
  {
    action++;
    bool sampled = sampledFlags[action];

    // if we're not sampling the low-variance actions and this is a low-variance action

    if (!sampled) 
    {
      if (player == updatePlayer && probing) {        
   
        //double newsprob1 = ( (lva && player == 1) ? sampleprob*sprob1 : sprob1); 
        //double newsprob2 = ( (lva && player == 2) ? sampleprob*sprob2 : sprob2); 
        GameState ngs = gs; 
        ngs.prevbid = gs.curbid;
        ngs.curbid = i; 
        ngs.callingPlayer = player;
        unsigned long long newbidseq = bidseq; 
        newbidseq |= (1 << (BLUFFBID-i));

        double moveProb = is.curMoveProbs[action];

        double payoff = probe(ngs, 3-player, depth+1, newbidseq, updatePlayer) / (sprob1*sprob2);

        moveEVs[action] = payoff;
        stratEV += moveProb*payoff;
      }
      else {
        moveEVs[action] = 0; 
        
        // stratEV doesn't change since this action has payoff 0
        // stratEV += moveProb*payoff;
      }

    }
    else   // otherwise sample it with all the rest
    {
      double sampleprob = sampledProbs[action];
      CHKPROBNZ(sampleprob);

      double newsprob1 = ( player == 1 ? sampleprob*sprob1 : sprob1); 
      double newsprob2 = ( player == 2 ? sampleprob*sprob2 : sprob2); 

      unsigned long long newbidseq = bidseq;
      double moveProb = is.curMoveProbs[action]; 
   
      CHKPROB(moveProb); 
   
      double newreach1 = (player == 1 ? moveProb*reach1 : reach1); 
      double newreach2 = (player == 2 ? moveProb*reach2 : reach2); 
      double payoff = 0.0;

      // take this move, make a new state for it  
      GameState ngs = gs; 
      ngs.prevbid = gs.curbid;
      ngs.curbid = i; 
      ngs.callingPlayer = player;
      newbidseq |= (1 << (BLUFFBID-i)); 

      // recursive call
      if (player == updatePlayer && probing)
        payoff = cfrprb(ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, newsprob1, newsprob2, updatePlayer)*sampledProbs[action];
      else
        payoff = cfrprb(ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, newsprob1, newsprob2, updatePlayer);
    
      // post recursion
      //
      // I don't think this is right for opponent nodes! See cfrabm.cpp
      // I think this file is out of date. 
      //
      moveEVs[action] = payoff;
      stratEV += moveProb*payoff;
    }
  }

  double myreach = (player == 1 ? reach1 : reach2); 
  double oppreach = (player == 1 ? reach2 : reach1); 
 
  if (player == updatePlayer)
  {
    // update regrets
    for (int a = 0; a < actionshere; a++)
    {
      is.cfr[a] += oppreach*(moveEVs[a] - stratEV); 
    }
  }
   
  if (player != updatePlayer)
  {
    // update av. strat
    for (int a = 0; a < actionshere; a++)
    {
      double inc = myreach*is.curMoveProbs[a] / (sprob1*sprob2);
      is.totalMoveProbs[a] += inc; 
    }
  }
    
  // save the infoset back to the store
  iss.put(infosetkey, is, actionshere, 0); 

  return stratEV;
}

int main(int argc, char ** argv)
{
  init();
  unsigned long long maxNodesTouched = 0; 

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
  }

  if (argc >= 3) 
  {
    string argstr = argv[2];
    int prbint = to_int(argstr); 
    if (prbint == 0) 
      probing = false;
    else
      probing = true;
  }

  if (argc >= 4) 
  {
    runname = argv[3];
  }


  if (argc >= 5) 
  {
    variant = argv[4];
  }
    
  if (argc >= 6) {
    string argstr = argv[5];
    maxNodesTouched = to_ull(argstr); 
  }

  cout << "probing = " << probing << ", runname = " << runname << ", variant = " << variant << endl;
  //loadVarianceStats();

  unsigned long long bidseq = 0; 
    
  double totaltime = 0; 
  StopWatch stopwatch;

  for (iter = 1; true; iter++)
  {
    GameState gs1; bidseq = 0;
    cfrprb(gs1, 1, 0, bidseq, 1.0, 1.0, 1.0, 1.0, 1);
    
    GameState gs2; bidseq = 0;
    cfrprb(gs2, 1, 0, bidseq, 1.0, 1.0, 1.0, 1.0, 2);

    if (iter % 1000 == 0) 
    { 
      totaltime += stopwatch.stop();
      stopwatch.reset(); 
      cout << "."; cout.flush(); 
    }

    if (   (maxNodesTouched > 0 && nodesTouched >= maxNodesTouched)
        || (maxNodesTouched == 0 && nodesTouched >= ntNextReport))
    {
      cout << endl;      
      cout << "total time: " << totaltime << " seconds. ";
      
      double b1 = 0.0, b2 = 0.0;
      iss.computeBound(b1, b2); 
      cout << "b1 = " << b1 << ", b2 = " << b2 << ", sum = " << (b1+b2) << endl;

      
      double conv = computeBestResponses(false, false);
      /*
      string str = "cfrprb"; 
      str += (probing ? "1" : "0"); 
      if (runname == "") 
        str += "report.txt";
      else
        str += "." + runname + ".report.txt";

      report(str, totaltime, 2.0*MAX(b1,b2), 0, 0, conv);
      */
      //dumpInfosets("issos"); 
      string str = "cfrprb."; str += runname; str += ".report.txt";
      report(str, totaltime, 0, 0, 0, conv);
      
      //if (iter > 1) nextCheckpoint += cpWidth; 
      ntNextReport *= ntMultiplier;

      cout << endl;
      
      stopwatch.reset(); 
    }

    if (maxNodesTouched > 0 && nodesTouched >= maxNodesTouched) 
      break;
  }
  
  //deallocMoveVar();
  return 0;
}

