
#include <cassert>
#include <iostream>
#include <cstdlib>

#include "bluff.h"

static InfosetStore psGlobalISS;  
static int infosetsSearched = 0;

static int runMatches = 0;

using namespace std; 

void single_match() { 
  double payoff = simloop(NULL, NULL);

  if (payoff > 0) 
    cout << "Game over. P1 wins!" << endl; 
  else if (payoff == 0) 
    cout << "game over. draw" << endl;
  else if (payoff < 0) 
    cout << "Game over. P2 wins!" << endl;
  
  double exp1 = searchComputeHalfBR(1, &sgiss1, p1type == PLYR_MCTS);
  double exp2 = searchComputeHalfBR(2, &sgiss2, p1type == PLYR_MCTS); 
  cout << "expl1 = " << exp1 << ", exp2 = " << exp2 << endl;
}

void multi_match() { 

  int matches = 1;
  double sumExpl1 = 0;
  double sumExpl2 = 0;
  double played1 = 0;
  double played2 = 0;

  InfosetStore matchISS1;
  InfosetStore matchISS2;
  sgiss1.copy(matchISS1, true); 
  sgiss2.copy(matchISS2, true); 

  assert(p1type == p2type); 

  int testType = p1type; 

  // 1: testType random
  // 2: testType mcts
  // 3: testType oos
  // 4: random   testType
  // 5: mcts     testType
  // 6: oos      testType
  
  StopWatch sw;

  //for (int scenario = 1; scenario <= 6; scenario++) { 

    /*if      (scenario == 1) { p1type = testType; p2type = PLYR_RANDOM; }
    else if (scenario == 2) { p1type = testType; p2type = PLYR_MCTS; }
    else if (scenario == 3) { p1type = testType; p2type = PLYR_OOS; }
    else if (scenario == 4) { p1type = PLYR_RANDOM; p2type = testType; }
    else if (scenario == 5) { p1type = PLYR_MCTS; p2type = testType; }
    else if (scenario == 6) { p1type = PLYR_OOS; p2type = testType; }*/

    for (int match = 1; match <= matches; match++) {
      cout << endl;
      cout << "Starting mtch " << match << endl;
      //cout << "Starting scenario " << scenario << ", match " << match << endl;

      sgiss1.clear();
      sgiss2.clear(); 
      matchISS1.clear();
      matchISS2.clear();

      simloop(&matchISS1, &matchISS2);
   
      sumExpl1 += searchComputeHalfBR(1, &matchISS1, p1type == PLYR_MCTS);
      played1++;

      sumExpl2 += searchComputeHalfBR(2, &matchISS2, p2type == PLYR_MCTS); 
      played2++;

      double gamesPerSec = (played1+played2) / sw.stop(); 
      double timeRemaining = (6*matches - (played1+played2)) / gamesPerSec;
      cout << "Remaining seconds: " << timeRemaining << endl;
    }
  //}
  
  double ttlAvgExpl = (sumExpl1/played1) + (sumExpl2/played2);

  cout << "matches played " << played1 << " " << played2 << ", avg. expl1 = " 
       << (sumExpl1/played1) << ", exp2 = " << (sumExpl2/played2) 
       << ", ttlAvgExpl = " << ttlAvgExpl << endl;

}

void multi_match_old() { 
 
  // multiple matches; 50 for each combo: 
  //   combo1: p1type p1type 
  //   combo2: p2type p2type
  //   combo3: p1type p2type
  //   combo4: p2type p1type
  
  int actualP1type = p1type;
  int actualP2type = p2type; 
  
  InfosetStore averageISS1; // iss for player type 1 
  InfosetStore averageISS2; // iss for player type 2

  sgiss1.copy(averageISS1, true); // allocate
  sgiss2.copy(averageISS2, true); // allocate

  StopWatch sw;
  int runs = 0;
  int simsPerCombo = 25;
  
  for (int combo = 1; combo <= 4; combo++) { 

    if (combo == 1) { 
      //   combo1: p1type p1type 
      p1type = actualP1type; 
      p2type = actualP1type;
    }
    else if (combo == 2) { 
      //   combo4: p2type p2type
      p1type = actualP2type; 
      p2type = actualP2type;
    }
    else if (combo == 3) { 
      //   combo2: p1type p2type
      p1type = actualP1type; 
      p2type = actualP2type;
    }
    else if (combo == 4) { 
      //   combo3: p2type p1type
      p1type = actualP2type; 
      p2type = actualP1type;
    }

    for (int sim = 0; sim < simsPerCombo; sim++) { 
      sgiss1.clear();
      sgiss2.clear();
      double payoff = 0;
      
      cout << "Sim Starting combo " << combo << " match " << sim << endl;

      // check the combo again
      if (combo == 1) {
        payoff = simloop(&averageISS1, &averageISS1);
        //averageISS1.aggregate(sgiss1, 1); 
        //averageISS1.aggregate(sgiss2, 2); 
      }
      else if (combo == 2) {
        payoff = simloop(&averageISS2, &averageISS2);
        //averageISS2.aggregate(sgiss1, 1); 
        //averageISS2.aggregate(sgiss2, 2); 
      }
      else if (combo == 3) { 
        payoff = simloop(&averageISS1, &averageISS2);
        //averageISS1.aggregate(sgiss1, 1); 
        //averageISS2.aggregate(sgiss2, 2); 
      }
      else if (combo == 4) { 
        payoff = simloop(&averageISS2, &averageISS1);
        //averageISS2.aggregate(sgiss1, 1); 
        //averageISS1.aggregate(sgiss2, 2); 
      }

      cout << "Sim ended, payoff = " << payoff << endl;

      runs++; 
      double runsPerSec = runs / sw.stop();
      double remaining = (4*simsPerCombo - runs) / runsPerSec; 
      cout << "Remining seconds: " << remaining << endl;
      cout << endl;
    }
  }

  double p1typeExp1 = 0;
  double p1typeExp2 = 0; 

  double p2typeExp1 = 0;
  double p2typeExp2 = 0; 

  p1type = actualP1type; 
  p2type = actualP2type; 

  p1typeExp1 = searchComputeHalfBR(1, &averageISS1, p1type == PLYR_MCTS);
  p1typeExp2 = searchComputeHalfBR(2, &averageISS1, p1type == PLYR_MCTS);
  
  p2typeExp1 = searchComputeHalfBR(1, &averageISS2, p2type == PLYR_MCTS);
  p2typeExp2 = searchComputeHalfBR(2, &averageISS2, p2type == PLYR_MCTS);

  double p1totalExp = p1typeExp1 + p1typeExp2;
  double p2totalExp = p2typeExp1 + p2typeExp2;

  cout << "Player type " << p1type << " expl as player 1 is " << p1typeExp1
       << " expl as player 2 is " << p1typeExp2 << ", total expl is " << p1totalExp 
       << endl;
  
  cout << "Player type " << p2type << " expl as player 1 is " << p2typeExp1
       << " expl as player 2 is " << p2typeExp2 << ", total expl is " << p2totalExp 
       << endl;
}

void multi_aggregate() { 
  InfosetStore averageISS1; // iss for player type 1 
  InfosetStore averageISS2; // iss for player type 2

  sgiss1.copy(averageISS1, true); // allocate
  sgiss2.copy(averageISS2, true); // allocate

  StopWatch sw;
    
  for (int sim = 0; sim < runMatches; sim++) { 
    cout << "Starting match " << sim << endl;
    sgiss1.clear();
    sgiss2.clear();
    simloop(&averageISS1, &averageISS2);

    double simsPerSec = (sim+1) / sw.stop();
    double remTime = (runMatches - (sim+1)) / simsPerSec; 
    cout << "Remaining time: " << remTime << endl << endl;
  }
  
  double expl1 = searchComputeHalfBR(1, &averageISS1, p1type == PLYR_MCTS);
  double expl2 = searchComputeHalfBR(2, &averageISS2, p2type == PLYR_MCTS);
  
  cout << "Exploitabilities: " << expl1 << " " << expl2 << " " << (expl1+expl2) << endl;
}

void partialstitch(GameState & match_gs, int player, int depth, unsigned long long bidseq, int stitchingPlayer, int depthLimit, 
  InfosetStore * parentStore) {
  // at terminal node?
  if (terminal(match_gs)) {
    return;
  }

  if (match_gs.p1roll == 0) 
  {
    if (stitchingPlayer == 1) { 
      for (int i = 1; i <= numChanceOutcomes(1); i++) 
      {
        GameState new_match_gs = match_gs; 
        new_match_gs.p1roll = i; 
        partialstitch(new_match_gs, player, depth, bidseq, stitchingPlayer, depthLimit, parentStore); 
      }
    }
    else {
      // bogus value
      GameState new_match_gs = match_gs; 
      new_match_gs.p1roll = 1;
      partialstitch(new_match_gs, player, depth, bidseq, stitchingPlayer, depthLimit, parentStore); 
    }

    return;
  }
  else if (match_gs.p2roll == 0)
  {
    double EV = 0.0; 

    if (stitchingPlayer == 2) { 
      for (int i = 1; i <= numChanceOutcomes(2); i++)
      {
        GameState new_match_gs = match_gs; 
        new_match_gs.p2roll = i; 
        partialstitch(new_match_gs, player, depth, bidseq, stitchingPlayer, depthLimit, parentStore); 
      }
    }
    else { 
      GameState new_match_gs = match_gs; 
      new_match_gs.p2roll = 1;
      partialstitch(new_match_gs, player, depth, bidseq, stitchingPlayer, depthLimit, parentStore); 
    }

    return;
  }
 
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

  InfosetStore tmpStore; 

  if (player == stitchingPlayer && depth < depthLimit) { 

    sgiss1.clear();
    sgiss2.clear();

    if (parentStore == NULL)
      sgiss1.copy(tmpStore, true);
    else
      parentStore->copy(tmpStore, true); 

    cout << "Starting stitch of infoset #" << infosetsSearched << " with key " << infosetkey << endl;
    
    // load this from the global one (will be uninitlizied when we get here)
    bool succ = tmpStore.get(infosetkey, is, actionshere, 0); 
    assert(succ); 


    // copy over to the searching ISS's
    if (player == 1) { 
      tmpStore.copy(sgiss1, false);
    }
    else if (player == 2) {  
      tmpStore.copy(sgiss2, false);
    }

    // need to set the searching player to know which infoset to load
    Infoset resultIS = is;
    int move = getMove(player, match_gs, bidseq, resultIS);

    // save it to the global one back
    psGlobalISS.put(infosetkey, resultIS, actionshere, 0); 

    if (player == 1) { 
      sgiss1.copy(tmpStore, false);
    }
    else if (player == 2) { 
      sgiss2.copy(tmpStore, false);
    }
      
    infosetsSearched++; 
    //double isPerSec = infosetsSearched / stopwatch.stop();
    //double remaining = (24576 - infosetsSearched)/isPerSec;     

    cout << "Infosets searched: " << infosetsSearched << endl; 
  }
  else if (player == stitchingPlayer && depth >= depthLimit) { 
    cout << "Saving suffix infoset, key = " << infosetkey << endl; 

    // simply get it from the searching one
    bool succ = parentStore->get(infosetkey, is, actionshere, 0); 
    assert(succ); 
    
    // and save it back to the 
    psGlobalISS.put(infosetkey, is, actionshere, 0); 
  }

    
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
    
    if (player == stitchingPlayer && depth < depthLimit)
      partialstitch(new_match_gs, 3-player, depth+1, newbidseq, stitchingPlayer, depthLimit, &tmpStore); 
    else
      partialstitch(new_match_gs, 3-player, depth+1, newbidseq, stitchingPlayer, depthLimit, parentStore); 
    
  }

  return;
}

void partial_stitch_matches() { 

  assert(p1type == p2type); 

  sgiss1.copy(psGlobalISS, true); // allocate

  int depthLimit = 2; 
  StopWatch stopwatch;

  InfosetStore finalISS1; 
  InfosetStore finalISS2; 
    
  GameState match_gs1; 
  unsigned long long bidseq = 0; 

  cout << "Starting stitch for player 1!" << endl; 
  partialstitch(match_gs1, 1, 0, bidseq, 1, depthLimit, NULL);

  psGlobalISS.copy(finalISS1, true); // allocate
  psGlobalISS.clear(); 
  
  GameState match_gs2;
  bidseq = 0; 

  cout << "Starting stitch for player 2!" << endl; 
  partialstitch(match_gs2, 1, 0, bidseq, 2, depthLimit, NULL);
  
  psGlobalISS.copy(finalISS2, true); // allocate
  psGlobalISS.clear(); 

  cout << "Done stitching; seconds taken: " << stopwatch.stop() << endl;
  
  double expl1 = searchComputeHalfBR(1, &finalISS1, p1type == PLYR_MCTS);
  double expl2 = searchComputeHalfBR(2, &finalISS2, p2type == PLYR_MCTS);
  double ttlexpl = expl1 + expl2; 

  cout << "depthLimit = " << depthLimit << ", player type = " << p1type << ", expl1 = " << expl1 
       << ", expl2 = " << expl2 << ", ttlexpl = " << ttlexpl << endl;
}

int main(int argc, char ** argv)
{
  init();
  
  simgame = true; 
  timeLimit = 5.0;
  string simtype = "";

  if (argc < 2)
  {
    initInfosets();
    exit(-1);
  }
  else 
  { 
    if (argc < 6) { 
      cout << "Usage: sim isfile1 isfile2 ptype1 ptype2 [single|multimatch|partstitch|agg] runmatches" << endl;
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

    simtype = argv[5]; 

    if (simtype == "agg") {
      runMatches = to_int(argv[6]); 
      timeLimit = to_double(argv[7]); 
    }
  }  

  if (simtype == "single") 
    single_match();
  else if (simtype == "multi")
    multi_match();
  else if (simtype == "partstitch")
    partial_stitch_matches();
  else if (simtype == "agg")
    multi_aggregate();
  else { 
    cout << "Unknown simulation type!" << endl;
    cout << "Usage: sim isfile1 isfile2 ptype1 ptype2 [single|multi|partstitch|agg] runmatches timelimit" << endl;
    exit(-1);
  }

}
  
