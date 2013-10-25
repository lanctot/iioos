
#include <iostream>
#include <cstdlib>
#include <cassert>

#include "bluff.h"

using namespace std;

InfosetStore evaliss2;

// returns the exact expected value to player when players
// use strategies inside these information set stores
double evaluate(GameState gs, int player, int depth, unsigned long long bidseq)
{
  // at terminal node?
  if (terminal(gs))
  {
    return payoff(gs, 1);
  }
  
  // chance nodes
  if (gs.p1roll == 0) 
  {
    double EV = 0.0; 

    for (int i = 1; i <= P1CO; i++) 
    {
      GameState ngs = gs; 
      ngs.p1roll = i; 

      EV +=  getChanceProb(1,i)
           * evaluate(ngs, player, depth+1, bidseq); 
    }

    return EV;
  }
  else if (gs.p2roll == 0)
  {
    double EV = 0.0; 

    for (int i = 1; i <= P2CO; i++)
    {
      GameState ngs = gs; 
      ngs.p2roll = i; 

      EV +=   getChanceProb(2,i)
            * evaluate(ngs, player, depth+1, bidseq); 
    }

    return EV;
  }
  
  // declare the variables
  Infoset is;
  unsigned long long infosetkey = 0;
  double EV = 0.0;
  int action = -1;

  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 
  assert(actionshere > 0);
  
  // get the info set
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
    bool ret = evaliss2.get(infosetkey, is, actionshere, 0); 
    assert(ret);
  }

  // we need to pull out the average strategy probs. so we'll need the normalizer
  double denom = 0.0;
  for (int a = 0; a < actionshere; a++)
    denom += is.totalMoveProbs[a];

  CHKDBL(denom);
  assert(denom > 0.0);
  
  // iterate over the actions
  for (int i = gs.curbid+1; i <= maxBid; i++) 
  {
    // there is a valid action here
    action++;
    assert(action < actionshere);

    unsigned long long newbidseq = bidseq;
    double moveProb = (denom <= 0.0 ? (1.0/actionshere) : is.totalMoveProbs[action] / denom); 

    GameState ngs = gs; 
    ngs.prevbid = gs.curbid;
    ngs.curbid = i; 
    ngs.callingPlayer = player;
    newbidseq |= (1ULL << (BLUFFBID-i)); 
    
    double payoff = evaluate(ngs, 3-player, depth+1, newbidseq); 
   
    EV += moveProb*payoff; 
  }

  return EV;
}

double evaluate()
{
  GameState gs;
  unsigned long long bidseq = 0;
  return evaluate(gs, 1, 0, bidseq); 
}


