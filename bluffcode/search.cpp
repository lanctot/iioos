
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

int getMoveRandom(int player, GameState gs, unsigned long long bidseq) {
  // count the number of actions. Can't call Bluff as first action when there is no bid
  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 
  assert(actionshere > 0);

  int action = static_cast<int>(drand48()*actionshere); 
  return (gs.curbid + 1 + action);
}

int getMoveKeyboard(int player, GameState gs, unsigned long long bidseq)
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

int getMoveStrat(int player, GameState gs, unsigned long long bidseq)
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

int getMove(int player, GameState gs, unsigned long long bidseq)
{
  int ptype = (player == 1 ? p1type : p2type); 

  sg_curPlayer = player;
  randMixRM = 0.01;

  switch(ptype)
  {
    case PLYR_KEYBOARD: return getMoveKeyboard(player, gs, bidseq); 
    case PLYR_MCTS: return getMoveMCTS(player, gs, bidseq); 
    case PLYR_STRAT: return getMoveStrat(player, gs, bidseq); 
    case PLYR_OOS: return getMoveOOS(player, gs, bidseq); 
    case PLYR_RANDOM: return getMoveRandom(player, gs, bidseq); 
    default: cerr << "Error, unknown ptype" << endl; exit(-1);
  }
}

double simloop()
{
  unsigned long long bidseq = 0; 
    
  GameState gs;
  bidseq = 0; 
  int player = 1; 
  double p1prob, p2prob;
    
  sampleChanceEvent(1, gs.p1roll, p1prob);
  sampleChanceEvent(2, gs.p2roll, p2prob);

  sg_curPlayer = 1; 

  do 
  {
    int move = getMove(player, gs, bidseq);
    assert(move >= gs.curbid+1 && move <= BLUFFBID); 

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
