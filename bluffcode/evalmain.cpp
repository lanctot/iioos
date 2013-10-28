
#include <iostream>
#include <cstdlib>
#include <string>

#include "bluff.h"

using namespace std;

int main(int argc, char ** argv)
{
  init();

  if (argc < 3)
  {
    cout << "Usage: ./eval <iss file for player 1> <iss file for player 2>" << endl;
    exit(-1);
  }

  StopWatch stopwatch;

  stopwatch.reset();
  string filename1 = argv[1];
  cout << "Reading the infosets from " << filename1 << "..."; cout.flush();
  iss.readFromDisk(filename1);
  cout << "time taken: " << stopwatch.stop() << " seconds. " << endl;

  stopwatch.reset();
  string filename2 = argv[2];
  cout << "Reading the infosets from " << filename2 << "..."; cout.flush();
  evaliss2.readFromDisk(filename2);
  //cout << "Importing values from " << filename2 << "..."; cout.flush();
  //iss.importValues(2, filename2);
  cout << "time taken: " << stopwatch.stop() << " seconds. " << endl;

  stopwatch.reset();
  cout << "Evaluating strategies..."; cout.flush();
  double ev = evaluate();
  cout << "time taken: " << stopwatch.stop() << " seconds. " << endl;
  cout.precision(15);
  cout << "ev = " << ev << endl;

}

