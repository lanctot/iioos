
#include <iostream>
#include <map>
#include <cassert>
#include <cmath>

using namespace std;

int NUMCARDS 5

unsigned long p1totalTurns = 0;
unsigned long p2totalTurns = 0;
unsigned long p1totalActions = 0;
unsigned long p2totalActions = 0;
unsigned long totalLeaves = 0;


map<unsigned long long,int> iapairs;

int iscWidth = 3;

string binrep(unsigned long long num) 
{
  string s = "";
  for (int i = 0; i < 64; i++) 
  {
    unsigned long long bit = 1; 
    bit <<= i;
    unsigned long long n = num & bit; 
    s = (n > 0 ? "1" : "0") + s; 
  }

  return s;
}

unsigned long long pow2(int i)
{
  unsigned long long answer = 1;
  return (answer << i); 
}

void bcount(int cbid, int player, int depth, int actions)
{
  if (depth >= NUMCARDS*3)
  {
    totalLeaves++;
    return;
  }

  int numbids = (cbid == 0 ? BIDS : BIDS+1);
  
  int actions = numbids - (cbid+1) + 1;
  unsigned long & totalTurns = (player == 1 ? p1totalTurns : p2totalTurns); 
  unsigned long & totalActions = (player == 1 ? p1totalActions : p2totalActions); 
  totalTurns++; 
  totalActions += actions; 

  for (int b = cbid+1; b <= numbids; b++)
  {
    if (depth == 0)
      cout << "Depth 0, b = " << b << endl;

    bcount(b, 3-player, depth+1);
  }
}

void countnoabs()
{
  bcount(0, 1, 0); 

  cout << "for public tree:" << endl;
  cout << "  p1totalTurns = " << p1totalTurns << endl;
  cout << "  p2totalTurns = " << p2totalTurns << endl;
  cout << "  totalLeaves = " << totalLeaves << endl;
  unsigned long long totalseqs = (p1totalTurns + p2totalTurns);
  cout << "total sequences = " << totalseqs << endl;
  cout << "p1 info sets = " << (p1totalTurns*P1CO) << endl;
  cout << "p2 info sets = " << (p2totalTurns*P2CO) << endl;
  unsigned long long ttlinfosets = (p1totalTurns*P1CO) + (p2totalTurns*P2CO);
  cout << "total info sets = " << ttlinfosets << endl;
  cout << "p1totalActions = " << p1totalActions << endl;
  cout << "p2totalActions = " << p2totalActions << endl;


  unsigned long iapairs1 = (p1totalActions*P1CO);
  unsigned long iapairs2 = (p1totalActions*P2CO);
  unsigned long totaliap = (iapairs1+iapairs2);
  cout << "infoset actions pairs, p1 p2 total = " 
       << iapairs1 << " " << iapairs2 << " " << totaliap << endl;

  //unsigned long td = (totaliap*2 + ttlinfosets*2);
  unsigned long td = (totaliap*3 + ttlinfosets*2);
  cout << "total doubles needed = " << td
       << ", bytes needed = " << (td*8) << endl;

  cout << "cache size ~ " << (ttlinfosets*4) << " entries =~ " << (ttlinfosets*4*2)*8 << " bytes" << endl;

  unsigned long long ttlbytes = ((td*8) + (ttlinfosets*4*2)*8); 
  cout << "total bytes = " << ttlbytes << " ( = " << (ttlbytes / (1024.0*1024.0*1024.0)) << " GB)" << endl;
}

int main()
{
  if (P1D == 3 || P2D == 3)
    iscWidth = 6;
  else if (P1D == 2 || P2D == 2) 
    iscWidth = 5;
  else
    iscWidth = 3;

  countnoabs();

  return 0;
}
