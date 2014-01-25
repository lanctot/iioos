
/** 
 * A very memory-efficent hash table for storing information sets. 
 * Grabbed from Bluffpt experiments. 
 */
#ifndef __INFOSETSTORE_H__
#define __INFOSETSTORE_H__

#include <string>
#include <fstream>

#include "goof.h"

struct Infoset; 

class InfosetStore
{
  // stores the position of each infoset in the large table
  // unlike in bluffpt, this is a hash table that uses linear probing
  unsigned long long * indexKeys; 
  unsigned long long * indexVals; 
  unsigned long long indexSize;
 
  // To avoid large contiguous portions of memory, store as rows of bitsets
  //DBitset * tablerows; 
  double ** tablerows;

  // total items to be stored
  // size in bytes of each
  // # bytes per row
  // # rows
  unsigned long long size; 
  unsigned long long rowsize;
  unsigned long long rows;

  // last row is the leftover (smaller)
  unsigned long long lastRowSize;

  // are we added infosets to this store? when doing so, we update the infoset counter
  // and add info to the index. when not doing so, we assume the index will get us our
  // position and simply replace what's there
  bool addingInfosets;
  unsigned long long nextInfosetPos;
  unsigned long long added;

  bool get_priv(unsigned long long infoset_key, Infoset & infoset, int moves, int firstmove); 
  void put_priv(unsigned long long infoset_key, Infoset & infoset, int moves, int firstmove); 

  void next(unsigned long long & row, unsigned long long & col, unsigned long long & pos, unsigned long long & curRowSize); 

public:

  InfosetStore()
  {
    tablerows = NULL;
  }

  void destroy()
  {
    if (tablerows != NULL)
    {
      delete [] indexKeys; 
      delete [] indexVals;

      for (unsigned int i = 0; i < rows; i++) 
        delete [] tablerows[i];
    
      delete [] tablerows;
    }

    tablerows = NULL;
  }

  ~InfosetStore()
  {
    destroy(); 
  }
  
  // returns the position into the large table or indexSize if not found
  // hashIndex is set to the index of the hash table where this key would go
  unsigned long long getPosFromIndex(unsigned long long infoset_key, unsigned long long & hashIndex); 
  // use this one if you don't care about the hashIndex
  unsigned long long getPosFromIndex(unsigned long long infoset_key);

  unsigned long long getSize() { return size; }

  // First param: total # of doubles needed. 
  //   Should be the total # of (infoset,action) pairs times 2 (2 doubles each)
  // Second param: size of index. 
  //   Should be the max number taken by an infoset key represented as an integer + 1
  void init(unsigned long long _size, unsigned long long _indexsize);
  std::string getStats();

  void stopAdding() { addingInfosets = false; } 
  unsigned long long getNextPos() { return nextInfosetPos; }
  unsigned long long getAdded() { return added; }

  bool get(unsigned long long infoset_key, Infoset & infoset, int moves, int firstmove); 
  void put(unsigned long long infoset_key, Infoset & infoset, int moves, int firstmove); 

  void writeBytes(std::ofstream & out, void * addr, unsigned int num);  
  void readBytes(std::ifstream & in, void * addr, unsigned int num); 

  void dumpToDisk(std::string filename);
  bool readFromDisk(std::string filename, bool allocate = true);

  bool contains(unsigned long long infoset_key);

  void printValues(); 
  void computeBound(double & sum_RTimm1, double & sum_RTimm2); 

  // used to save memory when evaluation strategies from 2 diff strat files
  void importValues(int player, std::string filename);

  // abstraction-specific functions
  void absComputeBound(InfosetStore & abs_iss, double & sum_RTimm1, double & sum_RTimm2); 
  void absImportStrategy(InfosetStore & abs_iss); 

  // used by MCTS
  void clear(); 
  void copy(InfosetStore & dest, bool allocateDest);
  void mctsToCFR_pure();   // chose max mean reward
  void mctsToCFR_mixed();  // normalize visit counts
};

#endif


