
#include <iostream>
#include <iostream>
#include <cassert>
#include <cstdlib>

#include "bluff.h"
#include "seqstore.h"

using namespace std;

extern int RECALL; 
extern SequenceStore seqstore;

extern bool uctbr;

int main(int argc, char ** argv)
{
  init();
  //RECALL = 12; // HACK

  assert(argc >= 2);
  
  cout << "Reading infosets from " << argv[1] << endl;
  if (!seqstore.readFromDisk(argv[1]))
  {
    cerr << "Problem reading file: " << argv[1] << endl; 
    exit(-1); 
  }

  // get the iteration
  string filename = argv[1];
  vector<string> parts; 
  split(parts, filename, '.'); 
  iter = to_ull(parts[1]); 
  cout << "Set iteration to " << iter << endl;

  // try loading metadeta. files have form scratch/iss-runname.iter.dat
  string filename2 = filename;
  replace(filename2, "iss", "metainfo"); 
  cout << "Loading metadata from file " << filename2 << "..." << endl;
  loadMetaData(filename2); 
  
  string opts = "";
  int fixed_player = 0; 

  if (argc >= 3)
    opts = argv[2]; 

  if (argc >= 4) 
    RECALL = to_int(argv[3]); 

  if (argc >= 5)
    fixed_player = to_int(argv[4]); 

  bool abs = false; 
  bool os = false;
  bool pr = false;
  bool twofiles = false;
  bool estval = false;
  bool fsibr = false;

  for (unsigned int i = 0; i < opts.length(); i++)
  {
    switch(opts[i])
    {
      case 'a': abs = true; cout << "Enabling abstraction.." << endl; break;
      case 'o': os = true; cout << "Out-of-date sampling fix enabled." << endl; break;
      case 'p': pr = true; cout << "Printing enabled." << endl; break;
      case 'n': break;
      case '2': twofiles = true; cout << "Importing strategy from two separate files (p1 p2) enabled." << endl; break;
      case 'u': uctbr = true; break;
      case 'f': fsibr = true; break;
      case 'v': estval = true; cout << "Estimating value." << endl; break; 
      default: cerr << "Unrecognized option: " << opts[i] << endl; exit(-1); 
    }
  }

  if (pr) { 
    iss.printValues();
    exit(-1);
  }

  if (abs) { 
    //assert(argc >= 4);
    //brImportAbsStrategy(argv[3]);  
    
    std::cerr << "Abstract best response from command-line disabled for now until FSICFR mess properly cleaned up" << endl; 
    //absComputeBestResponses(false, false); 
    exit(-1);
  }

  if (twofiles) { 
    // import player 2's strategy from the second file
    assert(argc >= 4);
    // takes too long on grex, let's use breezy
    //string file = argv[3];
    //iss.importValues(2, file); 

    cout << "Reading infosets from " << argv[3] << endl;
    if (!briss2.readFromDisk(argv[3]))
    {
      cerr << "Problem reading file: " << argv[3] << endl; 
      exit(-1); 
    }
    setBRTwoFiles(); 
  }

  #if FSICFR
  if (estval) { 
    unsigned int sequence[RECALL];
    initSequenceForward(sequence);
    double avStratEV = fsiAvgStratEV(true, sequence, 0, 0);
    cout << "expected value of av strat = " << avStratEV << endl;

    cout << "Starting estimate value, RECALL = " << RECALL << endl;
    estimateValue();
    return 0;
  }

  if (uctbr) { 
    assert(fixed_player != 0);
    cout << "Running UCT-based BR.. fp = " << fixed_player << endl;
    UCTBR(fixed_player); 
    exit(-1); 
  }
  
  if (fsibr) { 
    assert(fixed_player == 1 || fixed_player == 2);
    cout << "Running FSI-based BR.. fp = " << fixed_player << endl;
    fsiBR(fixed_player); 
    exit(-1); 
  }
  #endif  

  computeBestResponses(abs, os); 
}


