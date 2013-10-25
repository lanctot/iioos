
#include <iostream>
#include <cassert>
#include <cstring>
#include <cstdlib>

#include "seqstore.h"

// For Bluff(1,1) use 100. For (2,2) use 1000
#define ROWS 100
//#define ROWS 1000

using namespace std;

extern bool uctbr;

static unsigned long long totalLookups = 0; 
static unsigned long long totalMisses = 0; 

void ssRegretMatching(Infoset & infoset, int moves)
{
  //  do the usual regret matching to get the curMoveProbs
  int firstmove = 0;
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
    cout << "infoset store get probSum = " << probSum << endl;
    exit(-1);
  }
  #endif
}

unsigned long long getSSKey(int player, unsigned long long bidseq)
{
  // intended to be used by non-FSI algorithms
  unsigned long long key = bidseq;
  key <<= 1; 
  if (player == 2) { 
    key |= 1ULL;  
  }
  return key; 
}

// First param: total # of doubles needed. 
//   Should be the total # of (infoset,action) pairs times 2 (2 doubles each)
// Second param: size of index. 
//   Should be the max number taken by an infoset key represented as an integer + 1
void SequenceStore::init(unsigned long long _size, unsigned long long _indexsize)
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
        tablerows[i][j] = 0;
    }
    else 
    {
      tablerows[i] = new double[lastRowSize];
      assert(tablerows[i] != NULL);
      for (unsigned int j = 0; j < lastRowSize; j++)
        tablerows[i][j] = 0;
    }
  }

  // set to adding information sets
  addingEntries = true;
  nextEntryPos = 0;
  added = 0;
  
  cout << "IS: init done. " << endl;
}

string SequenceStore::getStats() 
{
  string str; 
  str += (to_string(size) + " "); 
  str += (to_string(rowsize) + " "); 
  str += (to_string(rows) + " "); 
  str += (to_string(lastRowSize) + " "); 
  str += (to_string(added) + " "); 
  str += (to_string(nextEntryPos) + " "); 
  str += (to_string(totalLookups) + " "); 
  str += (to_string(totalMisses) + " "); 

  double avglookups =   static_cast<double>(totalLookups + totalMisses) 
                      / static_cast<double>(totalLookups); 

  str += (to_string(avglookups));

  double percentfull = static_cast<double>(nextEntryPos) / static_cast<double>(size) * 100.0;
  
  str += (" (" + to_string(percentfull) + "\% full)");

  return str;
}

void SequenceStore::next(unsigned long long & row, unsigned long long & col, unsigned long long & pos, unsigned long long & curRowSize)
{
  pos++;
  col++; 
  
  if (col >= curRowSize) {
    col = 0; 
    row++;
    curRowSize = (row < (rows-1) ? rowsize : lastRowSize);
  }
}

bool SequenceStore::get(unsigned long long entry_key, SSEntry & ssentry, int moves, int firstmove)
{
  return get_priv(entry_key, ssentry, moves, firstmove);
}

void SequenceStore::put(unsigned long long entry_key, SSEntry & ssentry, int moves, int firstmove)
{
  put_priv(entry_key, ssentry, moves, firstmove);
}

bool SequenceStore::get(unsigned long long entry_key, Infoset & infoset, int moves, int firstmove, int chanceOutcome, bool regretmatching)
{
  return get_priv(entry_key, infoset, moves, firstmove, chanceOutcome, regretmatching);
}

void SequenceStore::put(unsigned long long entry_key, Infoset & infoset, int moves, int firstmove, int chanceOutcome)
{
  put_priv(entry_key, infoset, moves, firstmove, chanceOutcome);
}


bool SequenceStore::contains(unsigned long long entry_key)
{
  assert(entry_key < indexSize); 
  //unsigned long long pos = index[entry_key];
  unsigned long long pos = getPosFromIndex(entry_key); 
  return (pos >= size ? false : true);
}
  
unsigned long long SequenceStore::getPosFromIndex(unsigned long long entry_key)
{
  unsigned long long hi = 0;
  return getPosFromIndex(entry_key, hi); 
}
  
unsigned long long SequenceStore::getPosFromIndex(unsigned long long entry_key, unsigned long long & hashIndex)
{
  unsigned long long start = entry_key % indexSize; 
  unsigned long long misses = 0; 

  for (unsigned long long i = start; misses < indexSize; misses++)
  {
    if (indexKeys[i] == entry_key && indexVals[i] < size) 
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

bool SequenceStore::get_priv(unsigned long long entry_key, SSEntry & ssentry, int moves, int firstmove)
{
  unsigned long long row, col, pos, curRowSize;

  // need no longer be the case
  //assert(entry_key < indexSize);

  //pos = index[entry_key];
  pos = getPosFromIndex(entry_key);  // uses a hash table
  
  if (pos >= size && !dynamic) { return false; }
  else if (pos >= size && dynamic) 
  {
    ssentry.actionshere = moves;
    int NCO = (entry_key & 1ULL ? numChanceOutcomes(2) : numChanceOutcomes(1)); 
    
    for (int co = 0; co < NCO; co++) {
      Infoset & is = ssentry.infosets[co];
      newInfoset(is, moves);
    }

    put_priv(entry_key, ssentry, moves, firstmove); 

    return get_priv(entry_key, ssentry, moves, firstmove);
  }

  row = pos / rowsize;
  col = pos % rowsize;
  curRowSize = (row < (rows-1) ? rowsize : lastRowSize);

  // get the number of moves
  assert(row < rows); assert(col < curRowSize); assert(pos < size); 
  unsigned long long x; 
  double y = tablerows[row][col];
  assert(sizeof(x) == sizeof(double));
  memcpy(&x, &y, sizeof(x)); 
  ssentry.actionshere = static_cast<int>(x); 
  assert(ssentry.actionshere > 0);
  next(row, col, pos, curRowSize);
  
  int NCO = (entry_key & 1ULL ? numChanceOutcomes(2) : numChanceOutcomes(1)); 
  
  for (int co = 0; co < NCO; co++) 
  {
    Infoset & infoset = ssentry.infosets[co];
    // NOTE: no regret-matching done!

    #if FSICFR
    // reach1
    assert(row < rows); assert(col < curRowSize); assert(pos < size); 
    infoset.reach1 = tablerows[row][col];
    next(row, col, pos, curRowSize);
    
    // reach2
    assert(row < rows); assert(col < curRowSize); assert(pos < size); 
    infoset.reach2 = tablerows[row][col];
    next(row, col, pos, curRowSize);
    
    // value
    assert(row < rows); assert(col < curRowSize); assert(pos < size); 
    infoset.value = tablerows[row][col];
    next(row, col, pos, curRowSize);
   
    #if FSIPCS
    int OPPCO = (entry_key & 1ULL ? numChanceOutcomes(1) : numChanceOutcomes(2)); 
    // values for FSIPCS version
    for (int opco = 0; opco < OPPCO; opco++) { 
      assert(row < rows); assert(col < curRowSize); assert(pos < size); 
      infoset.reaches1[opco] = tablerows[row][col];
      next(row, col, pos, curRowSize);      
      
      assert(row < rows); assert(col < curRowSize); assert(pos < size); 
      infoset.reaches2[opco] = tablerows[row][col];
      next(row, col, pos, curRowSize);      

      assert(row < rows); assert(col < curRowSize); assert(pos < size); 
      infoset.values[opco] = tablerows[row][col];
      next(row, col, pos, curRowSize);      
    }
    #endif
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
    }
  }

  return true;
}

bool SequenceStore::fsiIncReaches(unsigned long long sskey, int moves, int firstmove, int chanceOutcome, double amount1, double amount2)
{
  unsigned long long row, col, pos, curRowSize;

  pos = getPosFromIndex(sskey);  // uses a hash table
  if (pos >= size) return false;

  // zoom to info set
  pos += (1 + (chanceOutcome-1)*(3 + 2*moves));

  row = pos / rowsize;
  col = pos % rowsize;
  curRowSize = (row < (rows-1) ? rowsize : lastRowSize);

  // reach1
  assert(row < rows); assert(col < curRowSize); assert(pos < size); 
  tablerows[row][col] += amount1;
  next(row, col, pos, curRowSize);
  
  // reach2
  assert(row < rows); assert(col < curRowSize); assert(pos < size); 
  tablerows[row][col] += amount2;

  return true;
}

bool SequenceStore::fsiGetValue(unsigned long long sskey, double & value, int moves, int firstmove, int chanceOutcome)
{
  unsigned long long row, col, pos, curRowSize;

  pos = getPosFromIndex(sskey);  // uses a hash table
  if (pos >= size) return false;

  // zoom to info set
  pos += (1 + (chanceOutcome-1)*(3 + 2*moves));

  // zoom to value
  pos += 2;

  if (pos >= size) pos -= size;
  assert(pos < size); 
  
  row = pos / rowsize;
  col = pos % rowsize;
  curRowSize = (row < (rows-1) ? rowsize : lastRowSize);

  assert(row < rows); assert(col < curRowSize); assert(pos < size);
  value = tablerows[row][col];
  
  return true;
}


bool SequenceStore::get_priv(unsigned long long entry_key, Infoset & infoset, int moves, int firstmove, int chanceOutcome, bool regretmatching)
{
  unsigned long long row, col, pos, curRowSize;

  pos = getPosFromIndex(entry_key);  // uses a hash table

  if (pos >= size && !dynamic) { return false; }
  else if (pos >= size && dynamic) 
  {
    SSEntry ssentry;
    ssentry.actionshere = moves;
    int NCO = (entry_key & 1ULL ? numChanceOutcomes(2) : numChanceOutcomes(1)); 
    
    for (int co = 0; co < NCO; co++) {
      Infoset & is = ssentry.infosets[co];
      newInfoset(is, moves);
    }

    put_priv(entry_key, ssentry, moves, firstmove); 

    return get_priv(entry_key, infoset, moves, firstmove, chanceOutcome, regretmatching); 
  }

  row = pos / rowsize;
  col = pos % rowsize;
  curRowSize = (row < (rows-1) ? rowsize : lastRowSize);

  assert(row < rows); assert(col < curRowSize); assert(pos < size); 
  unsigned long long x; 
  double y = tablerows[row][col];
  assert(sizeof(x) == sizeof(double));
  memcpy(&x, &y, sizeof(x)); 
  
  //infoset.actionshere = static_cast<int>(x); 
  infoset.actionshere = moves;

  assert(infoset.actionshere > 0); 

  // instead of calling next here, skip ahead to the appropriate infoset
  // skip the initial actionshere, then each infoset stores 3 doubles + 2 doubles per action
  assert(chanceOutcome - 1 >= 0); 
  pos += (1 + (chanceOutcome-1)*(3 + 2*infoset.actionshere)); 
  if (pos >= size) pos -= size;
  assert(pos < size); 
  
  row = pos / rowsize;
  col = pos % rowsize;
  curRowSize = (row < (rows-1) ? rowsize : lastRowSize);

  #if FSICFR
  // reach1
  assert(row < rows); assert(col < curRowSize); assert(pos < size); 
  infoset.reach1 = tablerows[row][col];
  next(row, col, pos, curRowSize);
  
  // reach2
  assert(row < rows); assert(col < curRowSize); assert(pos < size); 
  infoset.reach2 = tablerows[row][col];
  next(row, col, pos, curRowSize);
  
  // value
  assert(row < rows); assert(col < curRowSize); assert(pos < size); 
  infoset.value = tablerows[row][col];
  next(row, col, pos, curRowSize);
 
  #if FSIPCS
  int OPPCO = (entry_key & 1ULL ? numChanceOutcomes(1) : numChanceOutcomes(2)); 
  // values for FSIPCS version
  for (int opco = 0; opco < OPPCO; opco++) { 
    assert(row < rows); assert(col < curRowSize); assert(pos < size); 
    infoset.reaches1[opco] = tablerows[row][col];
    next(row, col, pos, curRowSize);      
    
    assert(row < rows); assert(col < curRowSize); assert(pos < size); 
    infoset.reaches2[opco] = tablerows[row][col];
    next(row, col, pos, curRowSize);      

    assert(row < rows); assert(col < curRowSize); assert(pos < size); 
    infoset.values[opco] = tablerows[row][col];
    next(row, col, pos, curRowSize);      
  }
  #endif
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
  }

  // do the usual regret-matching
  if (!uctbr && regretmatching) { ssRegretMatching(infoset, infoset.actionshere); }

  return true;
}

void SequenceStore::put_priv(unsigned long long entry_key, SSEntry & ssentry, int moves, int firstmove)
{
  unsigned long long row, col, pos, curRowSize;

  // the key can be big now since it is hashed
  //assert(entry_key < size);

  assert(moves > 0);
  bool newinfoset = false; 

  unsigned long long hashIndex = 0;
  unsigned long long thepos = getPosFromIndex(entry_key, hashIndex);  
  if (addingEntries && thepos >= size)
  {
    newinfoset = true; 

    // new infoset to be added at the end
    assert(nextEntryPos < size); 

    if (nextEntryPos >= size) { 
      cerr << "Error: next entry pos greater or equal to size. " << endl;
      exit(-1);
    }

    // only add it if it's a new info set
    pos = nextEntryPos;
    row = nextEntryPos / rowsize;
    col = nextEntryPos % rowsize;
    curRowSize = (row < (rows-1) ? rowsize : lastRowSize);
    
    //index[entry_key] = pos;
    assert(pos < size); 
    indexKeys[hashIndex] = entry_key;
    indexVals[hashIndex] = pos;

    //cout << "Adding infosetkey: " << entry_key << endl; 
  }
  else 
  {
    // we've seen this one before, load it
    newinfoset = false; 

    //pos = index[entry_key];
    //pos = indexVals[hashIndex]; 
    assert(thepos < size); 
    pos = thepos; 
    row = pos / rowsize;
    col = pos % rowsize;
    curRowSize = (row < (rows-1) ? rowsize : lastRowSize);
  }
  
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
  /*assert(row < rows);
  assert(col < curRowSize); 
  assert(pos < size); 
  x = infoset.lastUpdate;
  assert(sizeof(x) == sizeof(double));
  memcpy(&y, &x, sizeof(x));
  tablerows[row][col] = y; 
  next(row, col, pos, curRowSize);*/

  int NCO = (entry_key & 1ULL ? numChanceOutcomes(2) : numChanceOutcomes(1)); 

  for (int co = 0; co < NCO; co++) 
  {
    Infoset & infoset = ssentry.infosets[co];

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

    #if FSIPCS
    int OPPCO = (entry_key & 1ULL ? numChanceOutcomes(1) : numChanceOutcomes(2)); 
    // values for SS-version
    for (int opco = 0; opco < OPPCO; opco++) { 
      assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
      tablerows[row][col] = infoset.reaches1[opco];
      next(row, col, pos, curRowSize);      

      assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
      tablerows[row][col] = infoset.reaches2[opco];
      next(row, col, pos, curRowSize);      

      assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
      tablerows[row][col] = infoset.values[opco];
      next(row, col, pos, curRowSize);      
    }
    #endif
    #endif
  
    // moves are from 1 to moves, so write them in order. 
    // first, regret, then avg. strat
    for (int i = 0, m = firstmove; i < moves; i++, m++) 
    { 
      //cout << "pos = " << pos << ", row = " << row << endl;
      if (row >= rows) {
        cout << "fsiss stats: " << getStats() << endl;
      }

      assert(row < rows); assert(col < curRowSize); assert(pos < size); 
      CHKDBL(infoset.cfr[m]); 
      tablerows[row][col] = infoset.cfr[m];

      next(row, col, pos, curRowSize);

      assert(row < rows);
      assert(col < curRowSize); 
      assert(pos < size); 
      tablerows[row][col] = infoset.totalMoveProbs[m];

      next(row, col, pos, curRowSize); 
    }
  }

  if (newinfoset && addingEntries)
  {
    nextEntryPos = pos;
    //assert(nextEntryPos < size); <-- can't have this
    //put it back up to the start
    added++;
  }
}

void SequenceStore::put_priv(unsigned long long entry_key, Infoset & infoset, int moves, int firstmove, int chanceOutcome)
{
  unsigned long long row, col, pos, curRowSize;

  assert(moves > 0);
  
  if (!dynamic) { assert(!addingEntries); } // use the SSEntry put_priv to initiate it 

  unsigned long long hashIndex = 0;
  pos = getPosFromIndex(entry_key, hashIndex);  
    
  assert(pos < size); 
  row = pos / rowsize;
  col = pos % rowsize;
  curRowSize = (row < (rows-1) ? rowsize : lastRowSize);

  assert(row < rows); assert(col < curRowSize); assert(pos < size); 
  unsigned long long x = moves;
  double y; 
  assert(sizeof(x) == sizeof(double));
  memcpy(&y, &x, sizeof(x));
  tablerows[row][col] = y; 

  // instead of calling next here, skip ahead to the appropriate infoset
  // skip the initial actionshere, then each infoset stores 3 doubles + 2 doubles per action
  assert(chanceOutcome - 1 >= 0); 
  pos += (1 + (chanceOutcome-1)*(3 + 2*moves)); 
  if (pos >= size) pos -= size;
  assert(pos < size); 

  row = pos / rowsize;
  col = pos % rowsize;
  curRowSize = (row < (rows-1) ? rowsize : lastRowSize);
    
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

  #if FSIPCS
  int OPPCO = (entry_key & 1ULL ? numChanceOutcomes(1) : numChanceOutcomes(2)); 
  // values for SS-version
  for (int opco = 0; opco < OPPCO; opco++) { 
    assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
    tablerows[row][col] = infoset.reaches1[opco];
    next(row, col, pos, curRowSize);      

    assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
    tablerows[row][col] = infoset.reaches2[opco];
    next(row, col, pos, curRowSize);      

    assert(row < rows); assert(col < curRowSize);  assert(pos < size); 
    tablerows[row][col] = infoset.values[opco];
    next(row, col, pos, curRowSize);      
  }
  #endif
  #endif

  // moves are from 1 to moves, so write them in order. 
  // first, regret, then avg. strat
  for (int i = 0, m = firstmove; i < moves; i++, m++) 
  { 
    //cout << "pos = " << pos << ", row = " << row << endl;
    if (row >= rows) {
      cout << "fsiss stats: " << getStats() << endl;
    }

    assert(row < rows); assert(col < curRowSize); assert(pos < size); 
    if (!uctbr) { CHKDBL(infoset.cfr[m]); }
    tablerows[row][col] = infoset.cfr[m];

    next(row, col, pos, curRowSize);

    assert(row < rows);
    assert(col < curRowSize); 
    assert(pos < size); 
    tablerows[row][col] = infoset.totalMoveProbs[m];

    next(row, col, pos, curRowSize); 
  }
}
  
void SequenceStore::printValues()
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

void SequenceStore::clear()
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
        assert(row < rows);
        assert(col < curRowSize); 

        tablerows[row][col] = 0.0;
      
        next(row, col, pos, curRowSize);
        // total move probs
        
        tablerows[row][col] = 0.0;

        next(row, col, pos, curRowSize);
        // next cfr
      }
    }
  }
}

// chose max mean reward. set one of the totalMoveProbs to 1.0 and the rest to 0
void SequenceStore::mctsToCFR_pure()
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

void SequenceStore::mctsToCFR_mixed()
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

void SequenceStore::zeroCFR()
{
  for (unsigned int i = 0; i < indexSize; i++) 
  {
    if (indexVals[i] < size) 
    {
      // which player is it?
      unsigned long long key = indexKeys[i]; 
      int player = (key % 2 == 0 ? 1 : 2); 

      unsigned long long row, col, pos, curRowSize;

      pos = indexVals[i];  // uses a hash table

      row = pos / rowsize;
      col = pos % rowsize;
      curRowSize = (row < (rows-1) ? rowsize : lastRowSize);

      assert(row < rows); assert(col < curRowSize); assert(pos < size); 
      unsigned long long x; 
      double y = tablerows[row][col];
      assert(sizeof(x) == sizeof(double));
      memcpy(&x, &y, sizeof(x)); 
      unsigned int actionshere = static_cast<int>(x); 

      assert(actionshere > 0); 

      SSEntry ssentry;
      bool ret = get_priv(key, ssentry, actionshere, 0);
      assert(ret);

      int NCO = (player == 1 ? numChanceOutcomes(1) : numChanceOutcomes(2));

      for (int co = 1; co <= NCO; co++)
      {
        Infoset & is = ssentry.infosets[co-1];

        for (unsigned long long a = 0; a < actionshere; a++) 
        {
          float part1 = 0;
          float part2 = 0;
          double value = 0; 
          char * addr = reinterpret_cast<char *>(&value); 
          
          memcpy(&value, &part1, sizeof(float)); 
          memcpy(static_cast<void *>(addr + sizeof(float)), &part2, sizeof(float)); 

          //is.cfr[a] = 0; 
          is.cfr[a] = value; 
        }
      }

      put_priv(key, ssentry, actionshere, 0); 
    }
  }

}


void SequenceStore::computeBound(double & sum_RTimm1, double & sum_RTimm2)
{
  sum_RTimm1 = sum_RTimm2 = 0.0;

  for (unsigned int i = 0; i < indexSize; i++) 
  {
    if (indexVals[i] < size) 
    {
      // which player is it?
      unsigned long long key = indexKeys[i]; 
      int player = (key % 2 == 0 ? 1 : 2); 

      double & b = (player == 1 ? sum_RTimm1 : sum_RTimm2); 

      unsigned long long row, col, pos, curRowSize;

      pos = indexVals[i];  // uses a hash table

      row = pos / rowsize;
      col = pos % rowsize;
      curRowSize = (row < (rows-1) ? rowsize : lastRowSize);

      assert(row < rows); assert(col < curRowSize); assert(pos < size); 
      unsigned long long x; 
      double y = tablerows[row][col];
      assert(sizeof(x) == sizeof(double));
      memcpy(&x, &y, sizeof(x)); 
      unsigned int actionshere = static_cast<int>(x); 

      assert(actionshere > 0); 

      SSEntry ssentry;
      bool ret = get_priv(key, ssentry, actionshere, 0);
      assert(ret);

      int NCO = (player == 1 ? numChanceOutcomes(1) : numChanceOutcomes(2));

      for (int co = 1; co <= NCO; co++)
      {
        Infoset & is = ssentry.infosets[co-1];

        double max = -100000000000.0;
        for (unsigned long long a = 0; a < actionshere; a++) 
        {
          double cfr = is.cfr[a];
          CHKDBL(cfr);
          if (cfr > max)
            max = cfr; 
        }

        assert(max > -100000000000.0);
  
        double delta = max; 
        delta = MAX(0.0, delta); 

        b += delta; 
      }
    }
  }

  sum_RTimm1 /= static_cast<double>(iter); 
  sum_RTimm2 /= static_cast<double>(iter); 
}



void SequenceStore::writeBytes(std::ofstream & out, void * addr, unsigned int num)
{
  out.write(reinterpret_cast<const char *>(addr), num); 
}

void SequenceStore::readBytes(std::ifstream & in, void * addr, unsigned int num)  
{
  in.read(reinterpret_cast<char *>(addr), num); 
}

void SequenceStore::dumpToDisk(std::string filename) 
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

void SequenceStore::copy(SequenceStore & dest)
{
  dest.destroy();

  dest.indexSize = indexSize;
  dest.size = size;
  dest.rowsize = rowsize;
  dest.rows = rows;
  dest.lastRowSize = lastRowSize;

  dest.indexKeys = new unsigned long long [indexSize];
  dest.indexVals = new unsigned long long [indexSize];
  for (unsigned long long i = 0; i < indexSize; i++)
  {
    dest.indexKeys[i] = indexKeys[i];
    dest.indexVals[i] = indexVals[i];
  }

  dest.tablerows = new double* [rows];
  assert(dest.tablerows != NULL);
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

  unsigned long long pos = 0, row = 0, col = 0, curRowSize = rowsize; 
  while (pos < size) 
  {
    dest.tablerows[row][col] = tablerows[row][col];
    next(row, col, pos, curRowSize); 
  }
}


bool SequenceStore::readFromDisk(std::string filename)
{
  addingEntries = false; 
  nextEntryPos = 0; 
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
  indexKeys = new unsigned long long [indexSize]; 
  indexVals = new unsigned long long [indexSize]; 
  for (unsigned long long i = 0; i < indexSize; i++)
  {
    readBytes(in, indexKeys + i, 8); 
    readBytes(in, indexVals + i, 8); 
  }

  // table rows (allocation)
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

