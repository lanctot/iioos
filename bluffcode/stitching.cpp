
#include <cassert>
#include <iostream>
#include <cstdlib>
#include <sstream>

#include "bluff.h"

using namespace std; 

static StopWatch stopwatch;
static int infosetsSearched = 0; 

bool fullload(unsigned long long infosetkey, Infoset & is, int actionshere, int player) { 
  InfosetStore & myISS = (player == 1 ? fsiss1 : fsiss2); 
  unsigned long long uniqueId = myISS.getPosFromIndex(infosetkey);
  int type = (player == 1 ? p1type : p2type);

  ostringstream oss; 
  oss << "scratch/stitched-iss-" << player << "-" << type << "-" << uniqueId << ".dat"; 

  string filename = oss.str(); 
  bool succ = myISS.readFromDisk(filename);
  return succ; 
}

bool fullsave(unsigned long long infosetkey, int actionshere, int player) { 
  InfosetStore & myISS = (player == 1 ? fsiss1 : fsiss2); 
  unsigned long long uniqueId = myISS.getPosFromIndex(infosetkey);
  int type = (player == 1 ? p1type : p2type);

  ostringstream oss; 
  oss << "scratch/stitched-iss-" << player << "-" << type << "-" << uniqueId << ".dat"; 

  string filename = oss.str(); 
  myISS.dumpToDisk(filename);
  return true;
}

void fullstitch(GameState & match_gs, int player, int depth, unsigned long long bidseq, int stitchingPlayer) 
{
  // at terminal node?
  if (terminal(match_gs))
  {
    cout << "Terminal!" << endl;
    return;
  }

  if (match_gs.p1roll == 0) 
  {
    if (stitchingPlayer == 1) { 
      for (int i = 1; i <= numChanceOutcomes(1); i++) 
      {
        GameState new_match_gs = match_gs; 
        new_match_gs.p1roll = i; 

        fullstitch(new_match_gs, player, depth+1, bidseq, stitchingPlayer); 
      }
    }
    else {
      // bogus value
      GameState new_match_gs = match_gs; 
      new_match_gs.p1roll = 1;
      fullstitch(new_match_gs, player, depth+1, bidseq, stitchingPlayer); 
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

        fullstitch(new_match_gs, player, depth+1, bidseq, stitchingPlayer); 
      }
    }
    else { 
      GameState new_match_gs = match_gs; 
      new_match_gs.p2roll = 1;
      fullstitch(new_match_gs, player, depth+1, bidseq, stitchingPlayer); 
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

  if (player == stitchingPlayer) { 

    cout << "Starting stitch of infoset #" << infosetsSearched << " with key " << infosetkey << endl;
      
    // try a full load from disk (should fail since we're doing a DFS) 
    bool succ = fullload(infosetkey, is, actionshere, player); 
    assert(!succ); 

    cout << "Loading and copying infoset store..." << endl; 

    // load from stitching iss's and copy them over to search iss's
    if (player == 1) { 
      bool ret = fsiss1.get(infosetkey, is, actionshere, 0); 
      assert(ret);

      fsiss1.copy(sgiss1, false);
    }
    else if (player == 2) {  
      bool ret = fsiss2.get(infosetkey, is, actionshere, 0); 
      assert(ret);
      
      fsiss2.copy(sgiss2, false);
    }

    cout << "Searching.." << endl;

    // need to set the searching player to know which infoset to load
    int move = getMove(player, match_gs, bidseq);

    // copy it back
    //
    if (player == 1) 
      sgiss1.copy(fsiss1, false);
    else 
      sgiss2.copy(fsiss2, false);

    cout << "Full save... " << endl; 

    // now do a full save
    succ = fullsave(infosetkey, actionshere, player);
    assert(succ); 

    infosetsSearched++; 
    double isPerSec = infosetsSearched / stopwatch.stop();
    double remaining = (24757 - infosetsSearched)*isPerSec;     

    cout << "Infosets searched: " << infosetsSearched << ", ispersec = " << isPerSec << ", remaining seconds = " << remaining << endl;
  }

    
  // iterate over the actions
  for (int i = match_gs.curbid+1; i <= maxBid; i++) 
  {
    if (player == stitchingPlayer) { 
      cout << "Reloading stitched strategy from disk!" << endl;

      // try a full load from disk (should fail since we're doing a DFS) 
      bool succ = fullload(infosetkey, is, actionshere, player); 
      assert(succ); 
    
      if (player == 1) { 
        bool ret = fsiss1.get(infosetkey, is, actionshere, 0); 
        assert(ret);
      }
      else if (player == 2) {  
        bool ret = fsiss2.get(infosetkey, is, actionshere, 0); 
        assert(ret);
      }
    }

    // there is a valid action here
    action++;
    assert(action < actionshere);
    
    unsigned long long newbidseq = bidseq;

    GameState new_match_gs = match_gs; 
    new_match_gs.prevbid = match_gs.curbid;
    new_match_gs.curbid = i; 
    new_match_gs.callingPlayer = player;
    newbidseq |= (1ULL << (BLUFFBID-i)); 
    
    fullstitch(new_match_gs, 3-player, depth+1, newbidseq, stitchingPlayer); 
    
    //iss.put(infosetkey, is, actionshere, 0); 
  }


  return;
}

int main(int argc, char ** argv)
{
  unsigned long long maxIters = 0; 
  init();

  if (argc < 2)
  {
    initInfosets();
    exit(-1);
  }
  else 
  { 
    if (argc < 4) { 
      cout << "Usage: sim init.issfile.dat ptype1 ptype2" << endl;
      exit(-1);
    }

    string filename = argv[1];

    cout << "Reading the infosets from " << filename << "..." << endl;

    sgiss1.readFromDisk(filename);
    sgiss2.readFromDisk(filename);
    fsiss1.readFromDisk(filename);
    fsiss2.readFromDisk(filename);

    p1type = to_int(argv[2]);   
    p2type = to_int(argv[3]);   
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

  unsigned long long bidseq = 0; 
    
  stopwatch.reset();
  double totaltime = 0; 

  cout << "Starting CFR iterations" << endl;

  for (; true; iter++)
  {
    GameState match_gs; 
    bidseq = 0; 

    cout << "Starting stitch for player 1!" << endl; 
    fullstitch(match_gs, 1, 0, bidseq, 1);
    
    cout << "Starting stitch for player 1!" << endl; 
    fullstitch(match_gs, 1, 0, bidseq, 2);
  }
}

