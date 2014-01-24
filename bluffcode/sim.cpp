
#include <cassert>
#include <iostream>
#include <cstdlib>

#include "bluff.h"

using namespace std; 

int main(int argc, char ** argv)
{
  init();
  
  simgame = true; 
  timeLimit = 1.0;

  if (argc < 2)
  {
    initInfosets();
    exit(-1);
  }
  else 
  { 
    if (argc < 5) { 
      cout << "Usage: sim isfile1 isfile2 ptype1 ptype2" << endl;
      exit(-1);
    }

    string filename = argv[1];
    cout << "Reading the infosets from " << filename << "..." << endl;
    sgiss1.readFromDisk(filename);

    string filename2 = argv[2];
    cout << "Reading the infosets from " << filename2 << "..." << endl;
    sgiss2.readFromDisk(filename2);

    p1type = to_int(argv[3]);   
    p2type = to_int(argv[4]);   
  }  

  double payoff = simloop();

  if (payoff > 0) 
    cout << "Game over. P1 wins!" << endl; 
  else if (payoff == 0) 
    cout << "game over. draw" << endl;
  else if (payoff < 0) 
    cout << "Game over. P2 wins!" << endl;

  cout << "Single-match exploitability." << endl; 
  double v1 = 0;
  double v2 = 0;

  if (p1type == PLYR_MCTS) 
    v2 = searchComputeHalfBR(1, &sgiss1, true);
  else if (p1type == PLYR_OOS) 
    v2 = searchComputeHalfBR(1, &sgiss1, false);

  if (p2type == PLYR_MCTS) 
    v1 = searchComputeHalfBR(2, &sgiss2, true);
  else if (p2type == PLYR_OOS)
    v1 = searchComputeHalfBR(2, &sgiss2, false);

  cout << "v1 (expl of P2 strategy) = " << v1 << endl; 
  cout << "v2 (expl of P1 strategy) = " << v2 << endl;
}
  
