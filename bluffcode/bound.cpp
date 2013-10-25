
#include <iostream>
#include <cassert>
#include <cstdlib>

#include "bluff.h"

// computes the bound

using namespace std; 

int main(int argc, char ** argv)
{
  init();

  if (argc != 2) { 
    cerr << "Usage: absbound iss.iter.dat" << endl; 
    exit(0); 
  }

  cout << "Reading infosets from " << argv[1] << endl;
  if (!iss.readFromDisk(argv[1]))
  {
    cerr << "Problem reading file. " << endl; 
    exit(-1); 
  }

  // get the iteration
  string filename = argv[1];
  vector<string> parts; 
  split(parts, filename, '.'); 
  iter = to_ull(parts[1]); 
  cout << "Set iteration to " << iter << endl;

  cout << "Computing the bound... "; cout.flush(); 

  double b1 = 0, b2 = 0;
  StopWatch sw; 
  iss.computeBound(b1, b2); 
  double time = sw.stop(); 
  cout << "time taken: " << time << " seconds." << endl;
  cout << "Iter = " << iter << ", abstract bound: b1 = " << b1 << ", b2 = " << b2 << ", max = " 
       << MAX(b1,b2) << ", bound = " << 2*MAX(b1,b2) << endl; 

}
