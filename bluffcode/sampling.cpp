
#include <cassert>
#include <cstdlib>
#include <iostream>

#include "bluff.h"

using namespace std;

void sampleChanceEvent(int player, int & outcome, double & prob)
{
  int co = (player == 1 ? numChanceOutcomes(1) : numChanceOutcomes(2)); 

  double roll = drand48();
  double sum = 0.0;

  for (int i = 0; i < co; i++)
  {
    double pr = getChanceProb(player, i+1);

    if (roll >= sum && roll < sum+pr) {
      outcome = (i+1); 
      prob = pr;
      return;
    }

    sum += pr;
  }

  assert(false);
}

void sampleMoveAvg(Infoset & is, int actionshere, int & index, double & prob)
{
  double den = 0;

  for (int i = 0; i < actionshere; i++)
    den += is.totalMoveProbs[i];

  double roll = drand48();
  double sum = 0.0;
  
  for (int i = 0; i < actionshere; i++)
  {
    double iprob = (den > 0.0 ? (is.totalMoveProbs[i] / den) : (1.0 / actionshere));
    CHKPROB(prob);

    if (roll >= sum && roll < sum+iprob) {
      index = i; 
      prob = iprob;
      return;
    }

    sum += iprob;
  }

  assert(false);
  
}

int sampleAction(int player, Infoset & is, int actionshere, double & sampleprob, double epsilon, bool firstTimeUniform)
{

  // **Only do this when enabled by firstTimeUniform:
  //      if this infoset has never been updated: choose entirely randomly
  //      reason: there is no regret yet, hence no strategy. 
  //
  double eps = 0.0; 
  if (firstTimeUniform)
    eps = (is.lastUpdate == 0 ? 1.0 : epsilon); 
  else
    eps = epsilon;

  // build the distribution to sample from
  double dist[actionshere];
  for (int a = 0; a < actionshere; a++) 
    dist[a] = eps*(1.0 / actionshere) + (1.0-eps)*is.curMoveProbs[a];
  
  double roll = drand48(); 
  double sum = 0.0; 
  for (int a = 0; a < actionshere; a++) 
  {
    if (roll >= sum && roll < sum+dist[a])
    {
      sampleprob = dist[a]; 
      return a; 
    }

    sum += dist[a];
  }

  assert(false);
  return -1; 
}

int sampleActionBiased(int player, Infoset & is, int actionshere, double & sampleprob, double epsilon, 
                       double oos_gamma, int action) { 
  
  // build the distribution to sample from
  double dist[actionshere];
  for (int a = 0; a < actionshere; a++) {
    dist[a] =         oos_gamma*(a == action ? 1.0 : 0.0)
              + (1.0-oos_gamma)*( epsilon*(1.0 / actionshere) + (1.0-epsilon)*is.curMoveProbs[a] ); 
  } 
  
  double roll = drand48(); 
  double sum = 0.0; 
  for (int a = 0; a < actionshere; a++) 
  {
    if (roll >= sum && roll < sum+dist[a])
    {
      sampleprob = dist[a]; 
      return a; 
    }

    sum += dist[a];
  }

  assert(false);
  return -1; 
}

