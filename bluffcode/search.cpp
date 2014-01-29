
#include <cassert>
#include <iostream>
#include <cstdlib>

#include "bluff.h"

using namespace std; 

int p1type = 0;
int p2type = 0;

// Declared in bluff.h, defined in bluff.cpp. Used in this file
//extern bool simgame;
//extern InfosetStore sgiss1;
//extern InfosetStore sgiss2;

int getMoveRandom(int player, GameState gs, unsigned long long bidseq, Infoset &is) {
  // count the number of actions. Can't call Bluff as first action when there is no bid
  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 
  assert(actionshere > 0);

  int action = static_cast<int>(drand48()*actionshere); 
  return (gs.curbid + 1 + action);
}

int getMoveKeyboard(int player, GameState gs, unsigned long long bidseq, Infoset & is)
{
  int seq[BLUFFBID] = {0};
  int j = 0;
  for (int i = 0; i < BLUFFBID; i++)
  {
    if ((bidseq & (1ULL << (BLUFFBID-i))) > 0ULL) {
      seq[j] = i; 
      j++;
    }
  }

  // print the sequence

  int roll = (player == 1 ? gs.p1roll : gs.p2roll); 
  cout << "Player: " << player << ", Roll: " << roll << ", Bid: ";
  for (int i = 0; i < j; i++) {
    int dice, face;
    convertbid(dice, face, seq[i]); 
    cout << dice << "-" << face << " ";
  }
  cout << endl; 

  // display moves
  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 
  assert(actionshere > 0);

  int action = -1; 
  for (int i = gs.curbid+1; i <= maxBid; i++) 
  {
    action++; 
   
    if (i < BLUFFBID) {  
      int dice, face;
      convertbid(dice, face, i); 
      cout << i << ": " << dice << "-" << face << endl; 
    }
    else { 
      cout << i << ": bluff!" << endl;
    }
  }

  cout << "Move? "; cout.flush();

  int move;
  cin >> move;
  return move;
}

int getMoveStrat(int player, GameState gs, unsigned long long bidseq, Infoset & is)
{
  cout << "Player " << player << ", my roll is: " << 
      (player == 1 ? gs.p1roll : gs.p2roll) << " ... sampling from lookup strategy." << endl;

  unsigned long long pisk = 0; 
  Infoset pis; 
  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 

  // first get the parent infoset
  pisk = bidseq;  
  pisk <<= iscWidth; 
  
  if (player == 1)
  {
    pisk |= gs.p1roll; 
    pisk <<= 1; 
    bool ret = sgiss1.get(pisk, pis, actionshere, 0); 
    assert(ret);
  }
  else if (player == 2)
  {
    pisk |= gs.p2roll; 
    pisk <<= 1; 
    pisk |= 1; 
    bool ret = sgiss2.get(pisk, pis, actionshere, 0); 
    assert(ret);
  }

  int index = -1;
  double prob = 0;

  sampleMoveAvg(pis, actionshere, index, prob);

  double den = 0;
  for (int i = 0; i < actionshere; i++)
    den += pis.totalMoveProbs[i];

  for (int i = 0; i < actionshere; i++) 
  {
    double val = (pis.totalMoveProbs[i] / den); 

    cout << "move " << i; 
    if (gs.curbid + 1 + i == BLUFFBID) cout << " (bluff)"; 
    else { 
      int quantity = 0;
      int face = 0;
      convertbid(quantity, face, gs.curbid + 1 + i);
      cout << " (" << quantity << "-" << face << ")"; 
    }
    cout << " = " << val << endl;
  }

  assert(index >= 0);
  CHKPROBNZ(prob);

  return (gs.curbid + 1 + index);
}

int getMove(int player, GameState gs, unsigned long long bidseq, Infoset & is)
{
  int ptype = (player == 1 ? p1type : p2type); 

  sg_curPlayer = player;
  randMixRM = 0.001;

  switch(ptype)
  {
    case PLYR_KEYBOARD: return getMoveKeyboard(player, gs, bidseq, is); 
    case PLYR_MCTS: return getMoveMCTS(player, gs, bidseq, is); 
    case PLYR_STRAT: return getMoveStrat(player, gs, bidseq, is); 
    case PLYR_OOS: return getMoveOOS(player, gs, bidseq, is); 
    case PLYR_RANDOM: return getMoveRandom(player, gs, bidseq, is); 
    default: cerr << "Error, unknown ptype" << endl; exit(-1);
  }
}


void saveMeAndTheChildren(GameState & match_gs, int player, int depth, unsigned long long bidseq, InfosetStore * issPtr) {
  if (terminal(match_gs)) {
    return;
  }

  // will not be called at chance nodes
 
  // declare the variables
  Infoset is;
  unsigned long long infosetkey = 0;
  int action = -1;

  int maxBid = (match_gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - match_gs.curbid; 
  
  assert(actionshere > 0);

  // compute the infoset key

  infosetkey = bidseq;  
  infosetkey <<= iscWidth; 
  if (player == 1)
  {
    infosetkey |= match_gs.p1roll; 
    infosetkey <<= 1; 
  }
  else if (player == 2)
  {
    infosetkey |= match_gs.p2roll; 
    infosetkey <<= 1; 
    infosetkey |= 1; 
  }
  
  // load it from the searcher, place it into the real thing

  InfosetStore & myloadISS = (sg_curPlayer == 1 ? sgiss1 : sgiss2); 
  bool succ = myloadISS.get(infosetkey, is, actionshere, 0); 
  assert(succ); 

  issPtr->add(infosetkey, is, actionshere, 0, 0.1);
  //issPtr->put(infosetkey, is, actionshere, 0);

  // iterate over the actions
  for (int i = match_gs.curbid+1; i <= maxBid; i++) 
  {
    // there is a valid action here
    action++;
    assert(action < actionshere);

    unsigned long long newbidseq = bidseq;

    GameState new_match_gs = match_gs; 
    new_match_gs.prevbid = match_gs.curbid;
    new_match_gs.curbid = i; 
    new_match_gs.callingPlayer = player;
    newbidseq |= (1ULL << (BLUFFBID-i)); 
    
    saveMeAndTheChildren(new_match_gs, 3-player, depth+1, newbidseq, issPtr); 
  }

  return;
}

void saveEverything(GameState & match_gs, int player, int depth, unsigned long long bidseq, InfosetStore * issPtr) {
  if (terminal(match_gs)) {
    return;
  }
  
  // use chance sampling for imperfect recall paper
  if (match_gs.p1roll == 0) 
  {
    for (int i = 1; i <= numChanceOutcomes(1); i++) 
    {
      GameState ngs = match_gs; 
      ngs.p1roll = i; 

      saveEverything(ngs, player, depth+1, bidseq, issPtr); 
    }

    return;
  }
  else if (match_gs.p2roll == 0)
  {
    for (int i = 1; i <= numChanceOutcomes(2); i++)
    {
      GameState ngs = match_gs; 
      ngs.p2roll = i; 

      saveEverything(ngs, player, depth+1, bidseq, issPtr); 
    }

    return;
  }

  // will not be called at chance nodes
 
  // declare the variables
  Infoset is;
  unsigned long long infosetkey = 0;
  int action = -1;

  int maxBid = (match_gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - match_gs.curbid; 
  
  assert(actionshere > 0);

  // compute the infoset key

  infosetkey = bidseq;  
  infosetkey <<= iscWidth; 
  if (player == 1)
  {
    infosetkey |= match_gs.p1roll; 
    infosetkey <<= 1; 
  }
  else if (player == 2)
  {
    infosetkey |= match_gs.p2roll; 
    infosetkey <<= 1; 
    infosetkey |= 1; 
  }
  
  // load it from the searcher, place it into the real thing

  InfosetStore & myloadISS = (sg_curPlayer == 1 ? sgiss1 : sgiss2); 
  bool succ = myloadISS.get(infosetkey, is, actionshere, 0); 
  assert(succ); 

  issPtr->add(infosetkey, is, actionshere, 0, 0.1);
  //issPtr->put(infosetkey, is, actionshere, 0);

  // iterate over the actions
  for (int i = match_gs.curbid+1; i <= maxBid; i++) 
  {
    // there is a valid action here
    action++;
    assert(action < actionshere);

    unsigned long long newbidseq = bidseq;

    GameState new_match_gs = match_gs; 
    new_match_gs.prevbid = match_gs.curbid;
    new_match_gs.curbid = i; 
    new_match_gs.callingPlayer = player;
    newbidseq |= (1ULL << (BLUFFBID-i)); 
    
    saveEverything(new_match_gs, 3-player, depth+1, newbidseq, issPtr); 
  }

  return;
}


double simloop(InfosetStore * saveISS1, InfosetStore * saveISS2) 
{
  unsigned long long bidseq = 0; 
    
  GameState gs;
  bidseq = 0; 
  int player = 1; 
  double p1prob, p2prob;

  bool firstmove1 = true;
  bool firstmove2 = true;
    
  sampleChanceEvent(1, gs.p1roll, p1prob);
  sampleChanceEvent(2, gs.p2roll, p2prob);

  sg_curPlayer = 1; 

  do 
  {
    Infoset is;
    unsigned long long infosetkey = 0;
    getInfosetKey(gs, infosetkey, player, bidseq);
    
    int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
    int actionshere = maxBid - gs.curbid; 

    int move = getMove(player, gs, bidseq, is);
    assert(move >= gs.curbid+1 && move <= BLUFFBID); 

    // add to the collected infosets if necessary
    if (sg_curPlayer == 1 && saveISS1 != NULL) {       
      
      if (firstmove1) { 
        //sgiss1.copy(*saveISS1, false);
        saveISS1->add(infosetkey, is, actionshere, 0, 1.0);
        //GameState newgs;
        //unsigned long long newbidseq = 0;
        //saveEverything(newgs, 1, 0, newbidseq, saveISS1);
        firstmove1 = false; 
      }
      else {
        saveISS1->add(infosetkey, is, actionshere, 0, 1.0);
        //GameState ngs = gs; 
        //saveMeAndTheChildren(ngs, player, 0, bidseq, saveISS1);
      }

      //saveISS1->add(infosetkey, is, actionshere, 0); 
    }
    if (sg_curPlayer == 2 && saveISS2 != NULL) { 
      
      if (firstmove2) { 
        //sgiss2.copy(*saveISS2, false); 
        saveISS2->add(infosetkey, is, actionshere, 0, 1.0);
        //GameState newgs;
        //unsigned long long newbidseq = 0;
        //saveEverything(newgs, 1, 0, newbidseq, saveISS2);
        firstmove2 = false;
      }
      else { 
        saveISS2->add(infosetkey, is, actionshere, 0, 1.0);
        
        //GameState ngs = gs; 
        //saveMeAndTheChildren(ngs, player, 0, bidseq, saveISS2);
      }      

      //saveISS2->add(infosetkey, is, actionshere, 0); 
    }

    // Explore the space; 
    double roll = drand48(); 
    if (roll < 0.5) { 
      move = gs.curbid+1+static_cast<int>(drand48()*actionshere);
    }

    if (move < BLUFFBID) {
      int quantity = 0;
      int face = 0;
      convertbid(quantity, face, move); 
      cout << "Player chooses move " << move << " (" << quantity << "-" << face << ")" << endl;
    }
    else { 
      cout << "Player calls bluff!" << endl;
    }
    
    GameState ngs = gs; 
    ngs.prevbid = gs.curbid;
    ngs.curbid = move; 
    ngs.callingPlayer = player;
    bidseq |= (1ULL << (BLUFFBID-move)); 
    player = 3-player;
    sg_curPlayer = player; 

    gs = ngs;
  }  
  while(!terminal(gs));

  //int winner = whowon(gs);
  //cout << "Winner: " << winner << endl;
  cout << "Game done. Rolls were " << gs.p1roll << " " << gs.p2roll << endl;
  
  return payoff(gs, 1);
}
