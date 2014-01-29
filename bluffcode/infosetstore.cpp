
#include <iostream>
#include <cassert>
#include <cstring>
#include <cstdlib>

#include "infosetstore.h"

// For Bluff(1,1) use 100. For (2,2) use 1000
#define ROWS 100
//#define ROWS 1000

using namespace std;

static unsigned long long totalLookups = 0; 
static unsigned long long totalMisses = 0; 

// First param: total # of doubles needed. 
//   Should be the total # of (infoset,action) pairs times 2 (2 doubles each)
// Second param: size of index. 
//   Should be the max number taken by an infoset key represented as an integer + 1
void InfosetStore::init(unsigned long long _size, unsigned long long _indexsize)
{
  cout << "IS: init" << endl; 

  size = _size;
  indexSize = _indexsize; 

  rowsize = size / (ROWS-1); 
  lastRowSize = size - rowsize*(ROWS-1);
  rows = ROWS;

  int i = 0; 
  while (lastRowSize > rowsize) // will sometimes happen when _size is small 
  { 
    i++;
    rows = ROWS-i;
    rowsize = size / (rows-1); 
    lastRowSize = size - rowsize*(rows-1);
  }

  assert(i >= 0 && i <= 99); 

  cout << "IS: stats " << getStats() << endl;
  cout << "IS: allocating memory.. " << endl;

  // allocate the index
  indexKeys = new unsigned long long [indexSize];
  indexVals = new unsigned long long [indexSize];
  for (unsigned long long i = 0; i < indexSize; i++) 
    indexKeys[i] = indexVals[i] = size;   // used to indicate that no entry is present

  // allocate the rows 
  tablerows = new double* [rows];
  assert(tablerows != NULL);
  for (unsigned long long i = 0; i < rows; i++) 
  {
    if (i != (rows-1))
    {
      tablerows[i] = new double[rowsize];
      assert(tablerows[i] != NULL);
      for (unsigned long long j = 0; j < rowsize; j++)
        tablerows[i][j] = 0.0;
    }
    else 
    {
      tablerows[i] = new double[lastRowSize];
      assert(tablerows[i] != NULL);
      for (unsigned int j = 0; j < lastRowSize; j++)
        tablerows[i][j] = 0.0;
    }
  }

  // set to adding information sets
  addingInfosets = true;
  nextInfosetPos = 0;
  added = 0;
  
  cout << "IS: init done. " << endl;
}

string InfosetStore::getStats() 
{
  string str; 
  str += (to_string(size) + " "); 
  str += (to_string(rowsize) + " "); 
  str += (to_string(rows) + " "); 
  str += (to_string(lastRowSize) + " "); 
  str += (to_string(added) + " "); 
  str += (to_string(nextInfosetPos) + " "); 
  str += (to_string(totalLookups) + " "); 
  str += (to_string(totalMisses) + " "); 

  double avglookups =   static_cast<double>(totalLookups + totalMisses) 
                      / static_cast<double>(totalLookups); 

  double percent_full = static_cast<double>(nextInfosetPos) /  static_cast<double>(size) * 100.0;  

  str += (to_string(avglookups) + " ");
  str += (to_string(percent_full) + "\% full"); 
  return str;
}

void InfosetStore::next(unsigned long long & row, unsigned long long & col, unsigned long long & pos, unsigned long long & curRowSize)
{
  pos++;
  col++; 
  
  if (col >= curRowSize) {
    col = 0; 
    row++;
    curRowSize = (row < (rows-1) ? rowsize : lastRowSize);
  }
}

bool InfosetStore::get(unsigned long long infoset_key, Infoset & infoset, int moves, int firstmove)
{
  return get_priv(infoset_key, infoset, moves, firstmove);
}

void InfosetStore::put(unsigned long long infoset_key, Infoset & infoset, int moves, int firstmove)
{
  put_priv(infoset_key, infoset, moves, firstmove);
}

void InfosetStore::add(unsigned long long infoset_key, Infoset & infoset, int moves, int firstmove, double factor)
{
  add_priv(infoset_key, infoset, moves, firstmove, factor);
}


bool InfosetStore::contains(unsigned long long infoset_key)
{
  assert(infoset_key < indexSize); 
  //unsigned long long pos = index[infoset_key];
  unsigned long long pos = getPosFromIndex(infoset_key); 
  return (pos >= size ? false : true);
}
  
unsigned long long InfosetStore::getPosFromIndex(unsigned long long infoset_key)
{
  unsigned long long hi = 0;
  return getPosFromIndex(infoset_key, hi); 
}
  
unsigned long long InfosetStore::getPosFromIndex(unsigned long long infoset_key, unsigned long long & hashIndex)
{
  unsigned long long start = infoset_key % indexSize; 
  unsigned long long misses = 0; 

  for (unsigned long long i = start; misses < indexSize; misses++)
  {
    if (indexKeys[i] == infoset_key && indexVals[i] < size) 
    {
      // cache hit 
      totalLookups++; 
      totalMisses += misses;
      hashIndex = i; 
      return indexVals[i]; 
    }
    else if (indexVals[i] >= size) // index keys can be >= size since they're arbitrary, but not values!
    {
      totalLookups++; 
      totalMisses += misses;
      hashIndex = i;
      return size; 
    }

    i = i+1; 
    if (i >= indexSize)
      i = 0; 
  }

  // should be large enough to hold everything
  assert(false); 
  return 0;
}

bool InfosetStore::get_priv(unsigned long long infoset_key, Infoset & infoset, int moves, int firstmove)
{
  unsigned long long row, col, pos, curRowSize;

  // need no longer be the case
  //assert(infoset_key < indexSize);

  //pos = index[infoset_key];
  pos = getPosFromIndex(infoset_key);  // uses a hash table
  if (pos >= size) return false;

  row = pos / rowsize;
  col = pos % rowsize;
  curRowSize = (row < (rows-1) ? rowsize : lastRowSize);

  //infoset.init_mem(moves);
  
  /*
  #if (CFRMC2EXT == 1 || CFRMC2 == 1) && (CFRMC_AVGMETHOD == 1)
  // the first one is an integer
  long long x; 
  assert(sizeof(x) == sizeof(double));
  memcpy(&x, &tablerows[row][col], sizeof(x)); 
  infoset.updateProbsIter = static_cast<int>(x);
  next(row, col, pos, curRowSize);
  #endif
  */

  // get the number of moves
  assert(row < rows); assert(col < curRowSize); assert(pos < size); 
  unsigned long long x; 
  double y = tablerows[row][col];
  assert(sizeof(x) == sizeof(double));
  memcpy(&x, &y, sizeof(x)); 
  infoset.actionshere = static_cast<int>(x); 
  assert(infoset.actionshere > 0);
  next(row, col, pos, curRowSize);
  
  // get the lastupdate
  assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
  y = tablerows[row][col];
  assert(sizeof(x) == sizeof(double));
  memcpy(&x, &y, sizeof(x)); 
  infoset.lastUpdate = x; 
  next(row, col, pos, curRowSize);

  #if FSICFR
  // reach1
  assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
  infoset.reach1 = tablerows[row][col];
  next(row, col, pos, curRowSize);
  
  // reach2
  assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
  infoset.reach2 = tablerows[row][col];
  next(row, col, pos, curRowSize);
  
  // value
  assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
  infoset.value = tablerows[row][col];
  next(row, col, pos, curRowSize);
  #endif 

  for (int i = 0, m = firstmove; i < moves; i++,m++) 
  {
    assert(row < rows);
    assert(col < curRowSize); 
    assert(pos < size); 
    infoset.cfr[m] = tablerows[row][col];

    next(row, col, pos, curRowSize);

    assert(row < rows);
    assert(col < curRowSize); 
    assert(pos < size); 
    infoset.totalMoveProbs[m] = tablerows[row][col];

    next(row, col, pos, curRowSize); 
    
    // ADA 
    assert(row < rows);
    assert(col < curRowSize); 
    assert(pos < size); 
    infoset.weight[m] = tablerows[row][col];

    next(row, col, pos, curRowSize); 
  }

  // now do the usual regret matching to get the curMoveProbs
  double totPosReg = 0.0;
  bool all_negative = true;
  
  for (int i = 0, m = firstmove; i < moves; i++, m++) 
  {
    int movenum = m;
    double cfr = infoset.cfr[movenum];
    CHKDBL(cfr);

    if (cfr > 0.0)
    {
      totPosReg = totPosReg + cfr;
      all_negative = false;
    }
  }

  double probSum = 0.0;

  for (int i = 0, m = firstmove; i < moves; i++, m++) 
  {
    int movenum = m;

    if (!all_negative)
    {
      if (infoset.cfr[movenum] <= 0.0)
      {
        infoset.curMoveProbs[movenum] = 0.0;
      }
      else
      {
        assert(totPosReg >= 0.0);
        if (totPosReg > 0.0)  // regret-matching
          infoset.curMoveProbs[movenum] = infoset.cfr[movenum] / totPosReg;
      }
    }
    else
    {
      infoset.curMoveProbs[movenum] = 1.0/moves;
    }

    CHKPROB(infoset.curMoveProbs[movenum]);
    probSum += infoset.curMoveProbs[movenum];
  }

  #if !USERAT
  if (! (ABS(1-probSum) < 0.00000001))
  {
    cout << "infoset store 1, get probSum = " << probSum << endl;
    exit(-1);
  }
  #endif

  // Added for OOS
  // This is a fix for the 'irrationality problem'.
  probSum = 0; 
  double uniform = 1.0 / moves; 

  for (int i = 0, m = firstmove; i < moves; i++, m++) 
  {
    int movenum = m;
    
    infoset.curMoveProbs[movenum] = randMixRM*uniform + (1.0 - randMixRM)*infoset.curMoveProbs[movenum]; 
     
    probSum += infoset.curMoveProbs[movenum];
  }

  #if !USERAT
  if (! (ABS(1-probSum) < 0.00000001))
  {
    cout << "infoset store 2, get probSum = " << probSum << endl;
    exit(-1);
  }
  #endif

  return true;
}

void InfosetStore::put_priv(unsigned long long infoset_key, Infoset & infoset, int moves, int firstmove)
{
  unsigned long long row, col, pos, curRowSize;

  // the key can be big now since it is hashed
  //assert(infoset_key < size);

  assert(moves > 0);
  bool newinfoset = false; 

  unsigned long long hashIndex = 0;
  unsigned long long thepos = getPosFromIndex(infoset_key, hashIndex);  
  if (addingInfosets && thepos >= size)
  {
    newinfoset = true; 

    // new infoset to be added at the end
    assert(nextInfosetPos < size); 

    // only add it if it's a new info set
    pos = nextInfosetPos;
    row = nextInfosetPos / rowsize;
    col = nextInfosetPos % rowsize;
    curRowSize = (row < (rows-1) ? rowsize : lastRowSize);
    
    //index[infoset_key] = pos;
    assert(pos < size); 
    indexKeys[hashIndex] = infoset_key;
    indexVals[hashIndex] = pos;

    //cout << "Adding infosetkey: " << infoset_key << endl; 
  }
  else 
  {
    // we've seen this one before, load it
    newinfoset = false; 

    //pos = index[infoset_key];
    //pos = indexVals[hashIndex]; 
    assert(thepos < size); 
    pos = thepos; 
    row = pos / rowsize;
    col = pos % rowsize;
    curRowSize = (row < (rows-1) ? rowsize : lastRowSize);
  }
  
  //int moves = infoset.curMoveProbs.get_cap(); 
  //gMoveMap<double>::valuesType::iterator iter = infoset.curMoveProbs.begin(); 
  //int firstmove = iter->first; 
 
  /*
  #if (CFRMC2EXT == 1 || CFRMC2 == 1) && (CFRMC_AVGMETHOD == 1)
  // the first one is an integer
  long long x; 
  assert(sizeof(x) == sizeof(double));
  x = infoset.updateProbsIter; 
  memcpy(&tablerows[row][col], &x, sizeof(x)); 
  next(row, col, pos, curRowSize);
  #endif
  */

  // store the number of moves at this infoset
  assert(row < rows);
  assert(col < curRowSize); 
  assert(pos < size); 
  unsigned long long x = moves;
  double y; 
  assert(sizeof(x) == sizeof(double));
  memcpy(&y, &x, sizeof(x));
  tablerows[row][col] = y; 
  next(row, col, pos, curRowSize);

  // store the last update iter of this infoset
  assert(row < rows);
  assert(col < curRowSize); 
  assert(pos < size); 
  x = infoset.lastUpdate;
  assert(sizeof(x) == sizeof(double));
  memcpy(&y, &x, sizeof(x));
  tablerows[row][col] = y; 
  next(row, col, pos, curRowSize);

  #if FSICFR
  // reach1
  assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
  tablerows[row][col] = infoset.reach1;
  next(row, col, pos, curRowSize);
  
  // reach2
  assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
  tablerows[row][col] = infoset.reach2;
  next(row, col, pos, curRowSize);
  
  // value
  assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
  tablerows[row][col] = infoset.value;
  next(row, col, pos, curRowSize);
  #endif
  
  // moves are from 1 to moves, so write them in order. 
  // first, regret, then avg. strat
  for (int i = 0, m = firstmove; i < moves; i++, m++) 
  { 
    //cout << "pos = " << pos << ", row = " << row << endl;
    if (row >= rows) {
      cout << "iss stats: " << iss.getStats() << endl;
    }

    assert(row < rows);
    assert(col < curRowSize); 
    assert(pos < size); 
    CHKDBL(infoset.cfr[m]); 
    tablerows[row][col] = infoset.cfr[m];

    next(row, col, pos, curRowSize);

    assert(row < rows);
    assert(col < curRowSize); 
    assert(pos < size); 
    tablerows[row][col] = infoset.totalMoveProbs[m];

    next(row, col, pos, curRowSize); 
    
    // ADA
    assert(row < rows);
    assert(col < curRowSize); 
    assert(pos < size); 
    tablerows[row][col] = infoset.weight[m];

    next(row, col, pos, curRowSize); 
    
  }

  if (newinfoset && addingInfosets)
  {
    nextInfosetPos = pos;
    //assert(nextInfosetPos < size); <-- can't have this
    //put it back up to the start
    added++;
  }
}

void InfosetStore::add_priv(unsigned long long infoset_key, Infoset & infoset, int moves, int firstmove, double factor)
{
  unsigned long long row, col, pos, curRowSize;

  // the key can be big now since it is hashed
  //assert(infoset_key < size);

  assert(moves > 0);
  bool newinfoset = false; 

  unsigned long long hashIndex = 0;
  unsigned long long thepos = getPosFromIndex(infoset_key, hashIndex);  
  if (addingInfosets && thepos >= size)
  {
    newinfoset = true; 

    // new infoset to be added at the end
    assert(nextInfosetPos < size); 

    // only add it if it's a new info set
    pos = nextInfosetPos;
    row = nextInfosetPos / rowsize;
    col = nextInfosetPos % rowsize;
    curRowSize = (row < (rows-1) ? rowsize : lastRowSize);
    
    //index[infoset_key] = pos;
    assert(pos < size); 
    indexKeys[hashIndex] = infoset_key;
    indexVals[hashIndex] = pos;

    //cout << "Adding infosetkey: " << infoset_key << endl; 
  }
  else 
  {
    // we've seen this one before, load it
    newinfoset = false; 

    //pos = index[infoset_key];
    //pos = indexVals[hashIndex]; 
    assert(thepos < size); 
    pos = thepos; 
    row = pos / rowsize;
    col = pos % rowsize;
    curRowSize = (row < (rows-1) ? rowsize : lastRowSize);
  }
  
  //int moves = infoset.curMoveProbs.get_cap(); 
  //gMoveMap<double>::valuesType::iterator iter = infoset.curMoveProbs.begin(); 
  //int firstmove = iter->first; 
 
  /*
  #if (CFRMC2EXT == 1 || CFRMC2 == 1) && (CFRMC_AVGMETHOD == 1)
  // the first one is an integer
  long long x; 
  assert(sizeof(x) == sizeof(double));
  x = infoset.updateProbsIter; 
  memcpy(&tablerows[row][col], &x, sizeof(x)); 
  next(row, col, pos, curRowSize);
  #endif
  */

  // store the number of moves at this infoset
  assert(row < rows);
  assert(col < curRowSize); 
  assert(pos < size); 
  unsigned long long x = moves;
  double y; 
  assert(sizeof(x) == sizeof(double));
  memcpy(&y, &x, sizeof(x));
  tablerows[row][col] = y; 
  next(row, col, pos, curRowSize);

  // store the last update iter of this infoset
  assert(row < rows);
  assert(col < curRowSize); 
  assert(pos < size); 
  x = infoset.lastUpdate;
  assert(sizeof(x) == sizeof(double));
  memcpy(&y, &x, sizeof(x));
  tablerows[row][col] = y; 
  next(row, col, pos, curRowSize);

  #if FSICFR
  // reach1
  assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
  tablerows[row][col] = infoset.reach1;
  next(row, col, pos, curRowSize);
  
  // reach2
  assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
  tablerows[row][col] = infoset.reach2;
  next(row, col, pos, curRowSize);
  
  // value
  assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
  tablerows[row][col] = infoset.value;
  next(row, col, pos, curRowSize);
  #endif
  
  // moves are from 1 to moves, so write them in order. 
  // first, regret, then avg. strat
  for (int i = 0, m = firstmove; i < moves; i++, m++) 
  { 
    //cout << "pos = " << pos << ", row = " << row << endl;
    if (row >= rows) {
      cout << "iss stats: " << iss.getStats() << endl;
    }

    assert(row < rows);
    assert(col < curRowSize); 
    assert(pos < size); 
    CHKDBL(infoset.cfr[m]); 
    tablerows[row][col] += infoset.cfr[m];

    next(row, col, pos, curRowSize);

    assert(row < rows);
    assert(col < curRowSize); 
    assert(pos < size); 
    tablerows[row][col] += factor*infoset.totalMoveProbs[m];

    next(row, col, pos, curRowSize); 
    
    // ADA
    assert(row < rows);
    assert(col < curRowSize); 
    assert(pos < size); 
    tablerows[row][col] += infoset.weight[m];

    next(row, col, pos, curRowSize); 
    
  }

  if (newinfoset && addingInfosets)
  {
    nextInfosetPos = pos;
    //assert(nextInfosetPos < size); <-- can't have this
    //put it back up to the start
    added++;
  }
}
  
void InfosetStore::printValues()
{
  for (unsigned int i = 0; i < indexSize; i++) 
  {
    if (indexVals[i] < size) 
    {
      // this is a valid position
      unsigned long long row, col, pos, curRowSize;
      pos = indexVals[i];
      row = pos / rowsize;
      col = pos % rowsize;
      curRowSize = (row < (rows-1) ? rowsize : lastRowSize);

      cout << "infosetkey = " << indexKeys[i]; 
      cout << ", infosetkey_str = " << infosetkey_to_string(indexKeys[i]);

      // read # actions
      unsigned long long actionshere = 0;
      assert(sizeof(actionshere) == sizeof(double)); 
      memcpy(&actionshere, &tablerows[row][col], sizeof(actionshere)); 
      next(row, col, pos, curRowSize);
      
      // read the integer
      unsigned long long lastUpdate = 0;
      double x = tablerows[row][col];
      memcpy(&lastUpdate, &x, sizeof(actionshere)); 
      next(row, col, pos, curRowSize);

      cout << ", actions = " << actionshere << ", lastUpdate = " << lastUpdate << endl;

      for (unsigned long long a = 0; a < actionshere; a++) 
      {
        // cfr
        assert(row < rows);
        assert(col < curRowSize); 
        cout << "  cfr[" << a << "]=" << tablerows[row][col]; 
      
        next(row, col, pos, curRowSize);
        // total move probs
        cout << "  tmp[" << a << "]=" << tablerows[row][col]; 
        cout << endl;

        // cfrdelta
        //next(row, col, pos, curRowSize);
        
        next(row, col, pos, curRowSize);
        // next cfr
      }

      cout << endl;
    }
  }
}

void InfosetStore::clear()
{
  for (unsigned int i = 0; i < indexSize; i++) 
  {
    if (indexVals[i] < size) 
    {
      // which player is it?
      //unsigned long long key = indexKeys[i]; 

      // this is a valid position
      unsigned long long row, col, pos, curRowSize;
      pos = indexVals[i];
      row = pos / rowsize;
      col = pos % rowsize;
      curRowSize = (row < (rows-1) ? rowsize : lastRowSize);

      // read # actions
      unsigned long long actionshere = 0;
      assert(sizeof(actionshere) == sizeof(double)); 
      memcpy(&actionshere, &tablerows[row][col], sizeof(actionshere)); 
      next(row, col, pos, curRowSize);
      
      // read the integer
      unsigned long long lastUpdate = 0;
      double x = tablerows[row][col];
      memcpy(&lastUpdate, &x, sizeof(actionshere)); 
      tablerows[row][col] = 0.0;
      next(row, col, pos, curRowSize);

      for (unsigned long long a = 0; a < actionshere; a++) 
      {
        // cfr
        assert(row < rows); assert(col < curRowSize); assert(pos < size); 
        tablerows[row][col] = 0.0;
      
        next(row, col, pos, curRowSize);
        // total move probs
        
        assert(row < rows); assert(col < curRowSize); assert(pos < size); 
        tablerows[row][col] = 0.0;

        next(row, col, pos, curRowSize);
        // next ADA
        
        assert(row < rows); assert(col < curRowSize); assert(pos < size); 
        tablerows[row][col] = 0.0;
        
        next(row, col, pos, curRowSize);
        // next cfr
      }
    }
  }
}

void InfosetStore::aggregate(InfosetStore & from, int agplayer) { 
  for (unsigned int i = 0; i < indexSize; i++) 
  {
    if (indexVals[i] < size) 
    {
      // which player is it?
      unsigned long long key = indexKeys[i]; 
      int player = ((key & 1ULL) == 1ULL ? 2 : 1); 
      if (player != agplayer)
        continue;

      // this is a valid position
      unsigned long long row, col, pos, curRowSize;
      pos = indexVals[i];
      row = pos / rowsize;
      col = pos % rowsize;
      curRowSize = (row < (rows-1) ? rowsize : lastRowSize);

      // read # actions
      unsigned long long actionshere = 0;
      assert(sizeof(actionshere) == sizeof(double)); 
      memcpy(&actionshere, &tablerows[row][col], sizeof(actionshere)); 
      next(row, col, pos, curRowSize);
      
      // read the integer
      unsigned long long lastUpdate = 0;
      double x = tablerows[row][col];
      memcpy(&lastUpdate, &x, sizeof(actionshere)); 
      tablerows[row][col] = 0.0;
      next(row, col, pos, curRowSize);

      for (unsigned long long a = 0; a < actionshere; a++) 
      {
        // cfr
        assert(row < rows);
        assert(col < curRowSize); 

        tablerows[row][col] += from.tablerows[row][col]; 
      
        next(row, col, pos, curRowSize);
        // total move probs
        
        tablerows[row][col] += from.tablerows[row][col];

        next(row, col, pos, curRowSize);
        // next ADA
        
        tablerows[row][col] += from.tablerows[row][col];
        
        next(row, col, pos, curRowSize);
        // next cfr
      }
    }
  }
}

// chose max mean reward. set one of the totalMoveProbs to 1.0 and the rest to 0
void InfosetStore::mctsToCFR_pure()
{
  for (unsigned int i = 0; i < indexSize; i++) 
  {
    if (indexVals[i] < size) 
    {
      unsigned int maxact = -1; 
      double maxval = -1000.0;
      double totalDenom = 0.0;

      // which player is it?
      //unsigned long long key = indexKeys[i]; 

      // this is a valid position
      unsigned long long row, col, pos, curRowSize;
      pos = indexVals[i];
      row = pos / rowsize;
      col = pos % rowsize;
      curRowSize = (row < (rows-1) ? rowsize : lastRowSize);

      // read # actions
      unsigned long long actionshere = 0;
      assert(sizeof(actionshere) == sizeof(double)); 
      memcpy(&actionshere, &tablerows[row][col], sizeof(actionshere)); 
      next(row, col, pos, curRowSize);
      
      // read the integer
      unsigned long long lastUpdate = 0;
      double x = tablerows[row][col];
      memcpy(&lastUpdate, &x, sizeof(actionshere)); 
      next(row, col, pos, curRowSize);

      for (unsigned long long a = 0; a < actionshere; a++) 
      {
        // cfr
        assert(row < rows);
        assert(col < curRowSize); 

        double num = tablerows[row][col];
      
        next(row, col, pos, curRowSize);
        // total move probs
        
        double den = tablerows[row][col];
        totalDenom += den;

        next(row, col, pos, curRowSize);
        // next cfr
        
        if (den > 0.0) {
          double val = num / den; 
          if (val > maxval) { 
            maxval = val; 
            maxact = a; 
          }
        }
      }

      assert(maxact >= 0); 

      // now start over
      pos = indexVals[i];
      row = pos / rowsize;
      col = pos % rowsize;
      curRowSize = (row < (rows-1) ? rowsize : lastRowSize);

      // read # actions
      actionshere = 0;
      assert(sizeof(actionshere) == sizeof(double)); 
      memcpy(&actionshere, &tablerows[row][col], sizeof(actionshere)); 
      next(row, col, pos, curRowSize);
      
      // read the integer
      lastUpdate = 0;
      x = tablerows[row][col];
      memcpy(&lastUpdate, &x, sizeof(actionshere)); 
      next(row, col, pos, curRowSize);

      for (unsigned long long a = 0; a < actionshere; a++) 
      {
        // cfr
        assert(row < rows);
        assert(col < curRowSize); 

        next(row, col, pos, curRowSize);
        // total move probs
       
        if (totalDenom > 0.0 && a == maxact) 
          tablerows[row][col] = 1.0;
        else if (totalDenom > 0.0 && a != maxact)
          tablerows[row][col] = 0.0;
        else
          tablerows[row][col] = 1.0 / static_cast<double>(actionshere);

        next(row, col, pos, curRowSize);
        // next cfr
      }
    }
  }
}

void InfosetStore::mctsToCFR_mixed()
{
  for (unsigned int i = 0; i < indexSize; i++) 
  {
    if (indexVals[i] < size) 
    {
      double totalDenom = 0.0;
      double probSum = 0.0;

      // this is a valid position
      unsigned long long row, col, pos, curRowSize;
      pos = indexVals[i];
      row = pos / rowsize;
      col = pos % rowsize;
      curRowSize = (row < (rows-1) ? rowsize : lastRowSize);

      // read # actions
      unsigned long long actionshere = 0;
      assert(sizeof(actionshere) == sizeof(double)); 
      memcpy(&actionshere, &tablerows[row][col], sizeof(actionshere)); 
      next(row, col, pos, curRowSize);
      
      // read the integer
      unsigned long long lastUpdate = 0;
      double x = tablerows[row][col];
      memcpy(&lastUpdate, &x, sizeof(actionshere)); 
      next(row, col, pos, curRowSize);

      for (unsigned long long a = 0; a < actionshere; a++) 
      {
        // cfr
        assert(row < rows);
        assert(col < curRowSize); 

        next(row, col, pos, curRowSize);
        // total move probs
        
        double den = tablerows[row][col];

        assert(den >= 0.0);
        totalDenom += den;

        next(row, col, pos, curRowSize);
        // next cfr
      }

      // now start over
      pos = indexVals[i];
      row = pos / rowsize;
      col = pos % rowsize;
      curRowSize = (row < (rows-1) ? rowsize : lastRowSize);

      // read # actions
      actionshere = 0;
      assert(sizeof(actionshere) == sizeof(double)); 
      memcpy(&actionshere, &tablerows[row][col], sizeof(actionshere)); 
      next(row, col, pos, curRowSize);
      
      // read the integer
      lastUpdate = 0;
      x = tablerows[row][col];
      memcpy(&lastUpdate, &x, sizeof(actionshere)); 
      next(row, col, pos, curRowSize);

      for (unsigned long long a = 0; a < actionshere; a++) 
      {
        // cfr
        assert(row < rows);
        assert(col < curRowSize); 

        next(row, col, pos, curRowSize);
        // total move probs
        
        if (totalDenom > 0.0)
        {
          tablerows[row][col] = tablerows[row][col] / totalDenom;
          probSum += tablerows[row][col];
        }
        else
        {
          assert(actionshere > 0);
          tablerows[row][col] = 1.0 / static_cast<double>(actionshere);
          probSum += tablerows[row][col];
        }

        next(row, col, pos, curRowSize);
        // next cfr
      }
 
      ASSERTEQZERO(1.0 - probSum);
    }
  }
}


void InfosetStore::computeBound(double & sum_RTimm1, double & sum_RTimm2)
{
  for (unsigned int i = 0; i < indexSize; i++) 
  {
    if (indexVals[i] < size) 
    {
      // which player is it?
      unsigned long long key = indexKeys[i]; 
      double & b = (key % 2 == 0 ? sum_RTimm1 : sum_RTimm2); 

      // this is a valid position
      unsigned long long row, col, pos, curRowSize;
      pos = indexVals[i];
      row = pos / rowsize;
      col = pos % rowsize;
      curRowSize = (row < (rows-1) ? rowsize : lastRowSize);

      // read # actions
      unsigned long long actionshere = 0;
      assert(sizeof(actionshere) == sizeof(double)); 
      memcpy(&actionshere, &tablerows[row][col], sizeof(actionshere)); 
      next(row, col, pos, curRowSize);
      
      // read the integer
      unsigned long long lastUpdate = 0;
      double x = tablerows[row][col];
      memcpy(&lastUpdate, &x, sizeof(actionshere)); 
      next(row, col, pos, curRowSize);

      #if FSICFR
      // reach1
      //assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
      //infoset.reach1 = tablerows[row][col];
      next(row, col, pos, curRowSize);
      
      // reach2
      //assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
      //infoset.reach2 = tablerows[row][col];
      next(row, col, pos, curRowSize);
      
      // value
      //assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
      //infoset.value = tablerows[row][col];
      next(row, col, pos, curRowSize);
      #endif 

      double max = -100000000000.0;
      for (unsigned long long a = 0; a < actionshere; a++) 
      {
        // cfr
        assert(row < rows);
        assert(col < curRowSize); 

        double cfr = tablerows[row][col]; 
        CHKDBL(cfr);
        if (cfr > max)
          max = cfr; 
      
        next(row, col, pos, curRowSize);
        // total move probs
        next(row, col, pos, curRowSize);
        // next cfr
      }

      assert(max > -100000000000.0);

      double delta = max; 
      delta = MAX(0.0, delta); 

      b += delta; 
    }
  }

  sum_RTimm1 /= static_cast<double>(iter); 
  sum_RTimm2 /= static_cast<double>(iter); 
}



void InfosetStore::writeBytes(std::ofstream & out, void * addr, unsigned int num)
{
  out.write(reinterpret_cast<const char *>(addr), num); 
}

void InfosetStore::readBytes(std::ifstream & in, void * addr, unsigned int num)  
{
  in.read(reinterpret_cast<char *>(addr), num); 
}

void InfosetStore::dumpToDisk(std::string filename) 
{
  ofstream out(filename.c_str(), ios::out | ios::binary); 
  assert(out.is_open()); 

  assert(sizeof(unsigned long long) == 8); 
  assert(sizeof(double) == 8);

  // some integers
  writeBytes(out, &indexSize, 8);
  writeBytes(out, &size, 8);
  writeBytes(out, &rowsize, 8);
  writeBytes(out, &rows, 8);
  writeBytes(out, &lastRowSize, 8);

  // the index
  for (unsigned long long i = 0; i < indexSize; i++)
  {
    writeBytes(out, indexKeys + i, 8); 
    writeBytes(out, indexVals + i, 8); 
  }

  // the table
  unsigned long long pos = 0, row = 0, col = 0, curRowSize = rowsize; 
  while (pos < size) 
  {
    writeBytes(out, tablerows[row] + col, 8);  
    next(row, col, pos, curRowSize); 
  }

  out.close();
}

void InfosetStore::copy(InfosetStore & dest, bool allocateDest)
{
  if (allocateDest)
    dest.destroy();

  dest.indexSize = indexSize;
  dest.size = size;
  dest.rowsize = rowsize;
  dest.rows = rows;
  dest.lastRowSize = lastRowSize;

  if (allocateDest) {
    dest.indexKeys = new unsigned long long [indexSize];
    dest.indexVals = new unsigned long long [indexSize];
  }
  else { 
    //assert(sizeof(dest.indexKeys) == indexSize); 
    //assert(sizeof(dest.indexVals) == indexSize); 
  }

  for (unsigned long long i = 0; i < indexSize; i++)
  {
    dest.indexKeys[i] = indexKeys[i];
    dest.indexVals[i] = indexVals[i];
  }

  if (allocateDest) 
      dest.tablerows = new double* [rows];

  assert(dest.tablerows != NULL);

  if (allocateDest) { 
    for (unsigned long long i = 0; i < rows; i++) 
    {
      if (i != (rows-1))
      {
        dest.tablerows[i] = new double[rowsize];
        assert(dest.tablerows[i] != NULL);
      }
      else 
      {
        dest.tablerows[i] = new double[lastRowSize];
        assert(dest.tablerows[i] != NULL);
      }
    }
  } 

  unsigned long long pos = 0, row = 0, col = 0, curRowSize = rowsize; 
  while (pos < size) 
  {
    dest.tablerows[row][col] = tablerows[row][col];
    next(row, col, pos, curRowSize); 
  }
}


bool InfosetStore::readFromDisk(std::string filename, bool allocate)
{
  addingInfosets = false; 
  nextInfosetPos = 0; 
  added = 0; 

  ifstream in(filename.c_str(), ios::in | ios::binary); 
  //assert(in.is_open());  
  if (!in.is_open())
    return false; 

  // some integers
  readBytes(in, &indexSize, 8);        
  readBytes(in, &size, 8);        
  readBytes(in, &rowsize, 8);        
  readBytes(in, &rows, 8);        
  readBytes(in, &lastRowSize, 8);        
 
  // the index
  if (allocate) { 
    indexKeys = new unsigned long long [indexSize]; 
    indexVals = new unsigned long long [indexSize]; 
  }

  for (unsigned long long i = 0; i < indexSize; i++)
  {
    readBytes(in, indexKeys + i, 8); 
    readBytes(in, indexVals + i, 8); 
  }

  // table rows (allocation)
  if (allocate)
      tablerows = new double* [rows];

  assert(tablerows != NULL);
  for (unsigned long long i = 0; i < rows; i++) 
  {
    if (i != (rows-1))
    {
      if (allocate) 
          tablerows[i] = new double[rowsize];

      assert(tablerows[i] != NULL);
      for (unsigned long long j = 0; j < rowsize; j++)
        tablerows[i][j] = 0.0;
    }
    else 
    {
      if (allocate)
          tablerows[i] = new double[lastRowSize];

      assert(tablerows[i] != NULL);
      for (unsigned int j = 0; j < lastRowSize; j++)
        tablerows[i][j] = 0.0;
    }
  }

  // tablerows (read from disk)
  unsigned long long pos = 0, row = 0, col = 0, curRowSize = rowsize; 
  while (pos < size) 
  {
    readBytes(in, tablerows[row] + col, 8);  
    next(row, col, pos, curRowSize); 
  }

  in.close();

  return true;
}

// replace one of the players strategies with one loaded from a 
// different file
void InfosetStore::importValues(int player, string filename)
{
  ifstream in(filename.c_str(), ios::in | ios::binary);

  unsigned long long oIndexSize = 0, osize = 0, orowsize = 0, orows = 0, olastRowSize = 0;
  
  readBytes(in, &oIndexSize, 8);        
  readBytes(in, &osize, 8);        
  readBytes(in, &orowsize, 8);        
  readBytes(in, &orows, 8);        
  readBytes(in, &olastRowSize, 8);        

  assert(oIndexSize == indexSize);
  assert(osize == size);
  assert(orowsize == rowsize);
  assert(orows == rows);
  assert(olastRowSize == lastRowSize);

  unsigned long long maskresult = player - 1; 

  for (unsigned int i = 0; i < oIndexSize; i++)
  {
    Infoset is;

    // next index element
    streampos sp = (5 + i*2); sp = sp*8;
    in.seekg(sp);

    unsigned long long key = 0, val = 0;
    readBytes(in, &key, 8);
    readBytes(in, &val, 8);

    if ((key & 1ULL) == maskresult && val < size)
    {
      streampos fp = 5;
      fp += oIndexSize*2;
      fp += val;
      fp = fp*8;

      in.seekg(fp);

      unsigned long long actionshere = 0;
      unsigned long long lastUpdate = 0;
      
      readBytes(in, &actionshere, 8);
      readBytes(in, &lastUpdate, 8);

      is.actionshere = static_cast<int>(actionshere);
      is.lastUpdate = lastUpdate;

      assert(actionshere >= 0 && actionshere <= BLUFFBID);

      for (unsigned long long a = 0; a < actionshere; a++)
      {
        double * cfrptr = is.cfr;
        double * tmpptr = is.totalMoveProbs;
        readBytes(in, cfrptr + a, 8);
        readBytes(in, tmpptr + a, 8);
      }

      put(key, is, static_cast<int>(actionshere), 0); 
    }
  }
}
  
void InfosetStore::absImportStrategy(InfosetStore & abs_iss)
{
  for (unsigned long long i = 0; i < indexSize; i++) 
  {
    if (indexVals[i] < size) 
    {
      unsigned long long frow, fcol, fpos = indexVals[i], fcurRowSize;
      frow = fpos / rowsize;
      fcol = fpos % rowsize;
      fcurRowSize = (frow < rows-1 ? rowsize : lastRowSize);

      // which player is it?
      unsigned long long fullkey = indexKeys[i]; 
      unsigned long long key = absConvertKey(fullkey);

      // this is a valid position. 
      unsigned long long row, col, pos = abs_iss.size, curRowSize;
      unsigned long long num = 0, j = key % abs_iss.indexSize;
      for (num = 0; num < abs_iss.indexSize; num++) 
      {
        if (abs_iss.indexKeys[j] == key) {
          pos = abs_iss.indexVals[j]; 
          break; 
        }

        j++; if (j >= abs_iss.indexSize) j = 0;
      }
      //pos = abs_iss.indexVals[i];
      assert(pos < abs_iss.size); 

      row = pos / abs_iss.rowsize;
      col = pos % abs_iss.rowsize;
      curRowSize = (row < (abs_iss.rows-1) ? abs_iss.rowsize : abs_iss.lastRowSize);

      // read # actions (abstract)
      unsigned long long actionshere = 0;
      assert(sizeof(actionshere) == sizeof(double)); 
      memcpy(&actionshere, &abs_iss.tablerows[row][col], sizeof(actionshere)); 
      abs_iss.next(row, col, pos, curRowSize);
      
      // read the integer (abstract)
      unsigned long long lastUpdate = 0;
      double x = abs_iss.tablerows[row][col];
      memcpy(&lastUpdate, &x, sizeof(actionshere)); 
      abs_iss.next(row, col, pos, curRowSize);

      // read # actions (full)
      unsigned long long factionshere = 0;
      assert(sizeof(factionshere) == sizeof(double)); 
      memcpy(&factionshere, &tablerows[frow][fcol], sizeof(factionshere)); 
      next(frow, fcol, fpos, fcurRowSize);
      
      // read the integer (full)
      unsigned long long flastUpdate = 0;
      double fx = tablerows[frow][fcol];
      memcpy(&flastUpdate, &fx, sizeof(factionshere)); 
      next(frow, fcol, fpos, fcurRowSize);

      if (actionshere != factionshere) 
        cerr << fullkey << " " << key << " " << actionshere << " " << factionshere << endl;
      assert(actionshere == factionshere);

      double data[2]; 

      for (unsigned long long a = 0; a < actionshere; a++) 
      {
        // get them from abstract strategy 

        // cfr
        assert(row < abs_iss.rows);
        assert(col < curRowSize); 
        data[0] = abs_iss.tablerows[row][col]; 
      
        abs_iss.next(row, col, pos, curRowSize);
        // total move probs
        
        assert(row < abs_iss.rows);
        assert(col < curRowSize); 
        data[1] = abs_iss.tablerows[row][col]; 

        abs_iss.next(row, col, pos, curRowSize);
        // next cfr
        
        // now put them into full strategy
        
        // cfr
        assert(frow < rows);
        assert(fcol < fcurRowSize); 
        tablerows[frow][fcol] = data[0];

        next(frow, fcol, fpos, fcurRowSize); 
        // totalMoveProbs
        
        assert(frow < rows);
        assert(fcol < fcurRowSize); 
        tablerows[frow][fcol] = data[1];

        next(frow, fcol, fpos, fcurRowSize); 
        // next cfr
      }

    }
  }
}

// looks up the max value in the abstract infoset strategy instead
void InfosetStore::absComputeBound(InfosetStore & abs_iss, double & sum_RTimm1, double & sum_RTimm2)
{
  for (unsigned long long i = 0; i < indexSize; i++) 
  {
    if (indexVals[i] < size) 
    {
      // which player is it?
      unsigned long long fullkey = indexKeys[i]; 

      unsigned long long key = absConvertKey(fullkey);
      double & b = (key % 2 == 0 ? sum_RTimm1 : sum_RTimm2); 

      // this is a valid position. 
      unsigned long long row, col, pos = abs_iss.size, curRowSize;
      unsigned long long num = 0, j = key % abs_iss.indexSize;
      for (num = 0; num < abs_iss.indexSize; num++) 
      {      

        if (abs_iss.indexKeys[j] == key) {
          pos = abs_iss.indexVals[j]; 
          break; 
        }

        if (abs_iss.indexVals[j] == abs_iss.size)
          break; // empty spot -- should not happen

        j++; if (j >= abs_iss.indexSize) j = 0;
      }
      //pos = abs_iss.indexVals[i];
      //cout << abs_iss.indexSize << " " << pos << " " << abs_iss.size << endl;
      assert(num != abs_iss.indexSize);
      assert(pos < abs_iss.size); 

      row = pos / abs_iss.rowsize;
      col = pos % abs_iss.rowsize;
      curRowSize = (row < (abs_iss.rows-1) ? abs_iss.rowsize : abs_iss.lastRowSize);

      // read # actions
      unsigned long long actionshere = 0;
      assert(sizeof(actionshere) == sizeof(double)); 
      memcpy(&actionshere, &abs_iss.tablerows[row][col], sizeof(actionshere)); 
      abs_iss.next(row, col, pos, curRowSize);
      
      // read the integer
      unsigned long long lastUpdate = 0;
      double x = abs_iss.tablerows[row][col];
      memcpy(&lastUpdate, &x, sizeof(actionshere)); 
      abs_iss.next(row, col, pos, curRowSize);

      double max = -100000000000.0;
      for (unsigned long long a = 0; a < actionshere; a++) 
      {
        // cfr
        assert(row < abs_iss.rows);
        assert(col < curRowSize); 

        double cfr = abs_iss.tablerows[row][col]; 
        if (cfr > max)
          max = cfr; 
      
        abs_iss.next(row, col, pos, curRowSize);
        // total move probs
        abs_iss.next(row, col, pos, curRowSize);
        // next cfr
      }

      assert(max > -100000000000.0);

      double delta = max; 
      delta = MAX(0.0, delta); 

      b += delta; 
    }
  }

  sum_RTimm1 /= static_cast<double>(iter); 
  sum_RTimm2 /= static_cast<double>(iter); 
}


/*
int main()
{
  InfosetStore is; 

  // numbers for bluff(2,2)
  // 2**30
  //is.init(1073741824); 
  //cout << is.getStats() << endl; 

  // 2**16 for bluff(1,1)
  is.init(65536); 
  
  cout << is.getStats() << endl; 

}
*/
