
#include <cassert>
#include <iostream>
#include <cstdlib>

#include "bluff.h"
#include "seqstore.h"

using namespace std; 

static bool runBR = true;

unsigned int wthreads = 4;

typedef struct __threaddata {
  unsigned int id; 
  unsigned int iters; 
  bool alive;
  bool active; 
  pthread_mutex_t pmutex;
  pthread_cond_t pcv;
  double worker_time;

} tData; 

// Used to ensure that no 2 chance outcomes overlap
pthread_mutex_t comutex;
vector<bool> p1coInUse; 
vector<bool> p2coInUse; 

static tData * threadData;

// num each thread does before communicating with the parent
unsigned int batchcount = 100; 

// threads should not touch these!
// reportInc should a multiple of (wthreads*batchcount)
// should also be a multiple of maxIters
unsigned long lastReport = 0; 
static unsigned long long totalIters = 0;
unsigned long reportInc = 4000;
unsigned long nextReport = reportInc;
static unsigned long maxIters = 0;
double elapsed_time = 0;
StopWatch stopwatch;

double reportIncTime = 10.0;
double lastReportTime = 0.0;
double nextReportTime = reportIncTime;
unsigned int reports = 0;

extern int RECALL;
extern SequenceStore seqstore;

#if FSICFR

unsigned int fsiForwardPass(int p1roll, int p2roll, unsigned int * sequence)
{
  // sampled version. does not use SS
  initSequenceForward(sequence); 
  bool pass = true;
  bool initial = true; 
  unsigned int n = 0;
  
  do { 
  
    int curbid = sequence[RECALL-1];
    int maxBid = (curbid == 0 ? BLUFFBID-1 : BLUFFBID);
    int actionshere = maxBid - curbid; 
    
    unsigned long long sskey = 0; 

    for (int player = 1; player <= 2; player++) 
    {
      bool p1 = (player == 1 ? true : false); 
      unsigned int roll = static_cast<unsigned int>(player == 1 ? p1roll : p2roll); 
      
      Infoset is; 
      //sskey = fsiGetSSKey(p1, sequence); 
      sskey = getAbsSSKey(p1, sequence); 
      bool ret = seqstore.get(sskey, is, actionshere, 0, roll); 
      if (!ret) continue;  // failed; this can happen if the information set does not exist for this player

      /*cout << "fsi forward pass, infoset: " << p1 << " " << roll << " actions here = " << actionshere << " seq = "; 
      for (int j = 0; j < RECALL; j++) cout << sequence[j] << " "; 
      cout << endl;*/

      if (initial) 
      { 
        is.reach1 = 1.0; 
        is.reach2 = 1.0; 
      }

      CHKDBL(is.reach1); 
      CHKDBL(is.reach2); 

      if (p1 && is.reach1 <= 0.0) continue;
      if (!p1 && is.reach2 <= 0.0) continue;

      // prepare to traverse all the actions
      int action = -1; 

      bool childp1 = !p1; 
      unsigned int childroll = (childp1 ? p1roll : p2roll); 
      unsigned long long childkey = 0;      

      unsigned int oldestbid = sequence[0];
        
      // left-shift to make space for new bid
      for (int j = 0; j < RECALL-1; j++) { sequence[j] = sequence[j+1]; }

      // traverse all valid actions (valid bids). One-step lookahead
      //
      for (int i = curbid+1; i <= maxBid; i++) 
      {
        action++;
        assert(action < actionshere); 
        
        // update the average strategy (line 12 part of the FSICFR paper)
        if (p1) { is.totalMoveProbs[action] += is.reach1*is.curMoveProbs[action]; }
        else { is.totalMoveProbs[action] += is.reach2*is.curMoveProbs[action]; }

        if (i == BLUFFBID) continue; 

        sequence[RECALL-1] = i; 

        //Infoset cis;           
        //childkey = fsiGetSSKey(childp1, sequence); 
        childkey = getAbsSSKey(childp1, sequence); 

        int childMaxBid = BLUFFBID;
        int child_actionshere = childMaxBid - i;

        /*cout << "child infoset: " << childp1 << " " << childroll << " child actions here = " << child_actionshere << " seq = "; 
        for (int j = 0; j < RECALL; j++) cout << sequence[j] << " "; 
        cout << "(isk = " << childkey << ")" << endl;
        cout << endl; */
      
        /*bool cret = seqstore.get(childkey, cis, child_actionshere, 0, childroll); 
        assert(cret); 

        cis.reach1 += (p1 ? is.curMoveProbs[action]*is.reach1 : is.reach1); 
        cis.reach2 += (p1 ? is.reach2 : is.curMoveProbs[action]*is.reach2); 
        
        seqstore.put(childkey, cis, child_actionshere, 0, childroll);*/
        
        double crInc1 = (p1 ? is.curMoveProbs[action]*is.reach1 : is.reach1); 
        double crInc2 = (p1 ? is.reach2 : is.curMoveProbs[action]*is.reach2); 

        bool cret = seqstore.fsiIncReaches(childkey, child_actionshere, 0, childroll, crInc1, crInc2);
        assert(cret);
        
      }
      
      assert(action == actionshere-1);

      // put the bid back normal (right-shift)
      for (int j = RECALL-1; j >= 1; j--) { sequence[j] = sequence[j-1]; }
      sequence[0] = oldestbid;
      
      // the average strategy was updated for this node, so must save
      seqstore.put(sskey, is, actionshere, 0, roll); 
    
      initial = false; 
    }
  
    n++;

    pass = addOne(sequence);     
  }
  while(pass); 

  return n;
}


unsigned int fsiBackwardPass(int p1roll, int p2roll, unsigned int * sequence)
{
  // sampled version. does not use SS
  initSequenceBackward(sequence); 
  bool pass = true;
  unsigned int n = 0; 
  
  do { 
  
    int curbid = sequence[RECALL-1];
    int maxBid = (curbid == 0 ? BLUFFBID-1 : BLUFFBID);
    int actionshere = maxBid - curbid; 
    
    unsigned long long sskey = 0; 

    for (int player = 2; player >= 1; player--) 
    {
      bool p1 = (player == 1 ? true : false); 
      unsigned int roll = static_cast<unsigned int>(player == 1 ? p1roll : p2roll); 
      
      Infoset is; 
      //sskey = fsiGetSSKey(p1, sequence); 
      sskey = getAbsSSKey(p1, sequence); 
      bool ret = seqstore.get(sskey, is, actionshere, 0, roll); 
      
      if (!ret) continue;  // failed; this can happen if the information set does not exist for this player

      /*cout << "fsi backward pass, infoset: " << p1 << " " << roll << " actions here = " << actionshere << " seq = "; 
      for (int j = 0; j < RECALL; j++) cout << sequence[j] << " "; 
      cout << endl;*/

      // prepare to traverse all the actions
      int action = -1; 

      is.value = 0; 
      double stratEV = 0.0;
      double moveEVs[actionshere]; 
      for (int a = 0; a < actionshere; a++) moveEVs[a] = 0; 

      bool childp1 = !p1; 
      unsigned int childroll = (childp1 ? p1roll : p2roll); 
      unsigned long long childkey = 0;      

      unsigned int oldestbid = sequence[0];
        
      // left-shift to make space for new bid
      for (int j = 0; j < RECALL-1; j++) { sequence[j] = sequence[j+1]; }

      // traverse all valid actions (valid bids). One-step lookahead
      //
      for (int i = curbid+1; i <= maxBid; i++) 
      {
        action++;
        assert(action < actionshere); 

        if (i == BLUFFBID) 
        {
          // terminal node, get the payoff
          int callingPlayer = player; 
          int bidder = 3-player;
          double myutil = payoff(curbid, bidder, callingPlayer, p1roll, p2roll, player);
   
          moveEVs[action] = myutil; 
          stratEV += is.curMoveProbs[action] * moveEVs[action];
        }
        else
        {
          sequence[RECALL-1] = i; 

          //Infoset cis;           
          //childkey = fsiGetSSKey(childp1, sequence);
          childkey = getAbsSSKey(childp1, sequence);

          int childMaxBid = BLUFFBID;
          int child_actionshere = childMaxBid - i;

          /*cout << "child infoset: " << childp1 << " " << childroll << " child actions here = " 
           *     << child_actionshere << " seq = "; 
          for (int j = 0; j < RECALL; j++) cout << sequence[j] << " "; 
          cout << "(isk = " << childkey << ")" << endl;
          cout << endl; */

          /*bool cret = seqstore.get(childkey, cis, child_actionshere, 0, childroll);
          assert(cret); 

          assert(cis.value >= -1.0000000001 && cis.value <= 1.000000001); 

          moveEVs[action] = -cis.value;
          stratEV += is.curMoveProbs[action] * moveEVs[action];*/

          double cval = 0;

          bool cret = seqstore.fsiGetValue(childkey, cval, child_actionshere, 0, childroll);

          assert(cret);
          assert(cval >= -1.0000000001 && cval <= 1.000000001); 

          moveEVs[action] = -cval;
          stratEV += is.curMoveProbs[action] * moveEVs[action];
          
        }
      }

      assert(action == actionshere-1);

      // put the bid back normal (right-shift)
      for (int j = RECALL-1; j >= 1; j--) { sequence[j] = sequence[j-1]; }
      sequence[0] = oldestbid;

      is.value = stratEV; 

      CHKDBL(is.value); 

      assert(is.value >= -1.00000001 && is.value <= 1.000000001); 

      double oppreach = (player == 1 ? is.reach2 : is.reach1); 
      for (int a = 0; a < actionshere; a++)
      {
        is.cfr[a] += oppreach*(moveEVs[a] - stratEV); 
      }
      
      is.reach1 = is.reach2 = 0.0;

      seqstore.put(sskey, is, actionshere, 0, roll); 
    }
  
    n++;
    pass = subOne(sequence);     
  }
  while(pass); 

  return n;
}

#else

unsigned int fsiForwardPass(int p1roll, int p2roll, unsigned int * sequence) { return 0; }
unsigned int fsiBackwardPass(int p1roll, int p2roll, unsigned int * sequence) { return 0; }

#endif

void fsimtSampleChanceEvent(int player, int & roll)
{
  vector<bool> & inUse = (player == 1 ? p1coInUse : p2coInUse); 
  int nCO = numChanceOutcomes(player);

  double total = 0.0;

  for (int co = 1; co <= nCO; co++)
  {
    if (!inUse[co-1]) {
      //total += getChanceProb(player, co);
      total += 0.01;
    }
  }

  assert(total > 0.0 && total <= 1.00000001);  

  double rollnum = drand48() * total; 

  double sum = 0.0;

  for (int co = 1; co <= nCO; co++)
  {
    if (!inUse[co-1])
    {
      //double prob = getChanceProb(player, co);
      double prob = 0.01;

      if (rollnum >= 0.0 && rollnum < sum + prob) 
      {
        roll = co; 
        return;
      }

      sum += prob;
    }
  }
 
  assert(false);
}


void fsiIter(tData * data)
{
  // sampled version. does not use SS 
  //
  int p1roll = 0, p2roll = 0;
  //double prob1 = 0.0, prob2 = 0.0;

  pthread_mutex_lock(&comutex);

  fsimtSampleChanceEvent(1, p1roll); 
  fsimtSampleChanceEvent(2, p2roll);
  p1coInUse[p1roll-1] = true; 
  p2coInUse[p2roll-1] = true; 

  pthread_mutex_unlock(&comutex);

  assert(p1roll > 0); 
  assert(p2roll > 0); 

  unsigned int sequence[RECALL];

  unsigned int infosets1 = fsiForwardPass(p1roll, p2roll, sequence); 
  unsigned int infosets2 = fsiBackwardPass(p1roll, p2roll, sequence); 

  assert(infosets1 == infosets2);

  pthread_mutex_lock(&comutex);
  
  p1coInUse[p1roll-1] = false; 
  p2coInUse[p2roll-1] = false; 

  pthread_mutex_unlock(&comutex);
}

static double getElapsedTime()
{
  double total = 0;

  for (unsigned int i = 0; i < wthreads; i++)
    total += threadData[i].worker_time;

  return elapsed_time + total/wthreads;
}

void * worker(void * ptr)
{
  tData * data = static_cast<tData*>(ptr); 
  
  cout << "W" << data->id << endl; 
  
  while (data->alive)
  {
    pthread_mutex_lock(&data->pmutex);
    data->active = false; 
    pthread_cond_wait(&data->pcv, &data->pmutex); // wait for green light
    pthread_mutex_unlock(&data->pmutex);

    //cout << "w" << data->id << "s "; 
    cout << "-"; 
    cout.flush();

    StopWatch wsw; 

    for (unsigned int i = 0; i < batchcount; i++)
    {
      fsiIter(data);
    }

    data->worker_time += wsw.stop(); 
  
    cout << "=";
    cout.flush();
  }

  return NULL;
}

void parent()
{
  for (int co = 1; co <= numChanceOutcomes(1); co++) p1coInUse.push_back(false);
  for (int co = 1; co <= numChanceOutcomes(2); co++) p2coInUse.push_back(false);

  pthread_t threads[wthreads];
  threadData = new tData[wthreads]; 

  for (unsigned int i = 0; i < wthreads; i++) 
  {
    threadData[i].id = i; 
    threadData[i].iters = 0; 
    threadData[i].alive = true; 
    pthread_mutex_init(&threadData[i].pmutex, NULL);
    pthread_cond_init(&threadData[i].pcv, NULL);
    threadData[i].active = false;
    threadData[i].worker_time = 0;

    int ret = pthread_create( &threads[i], NULL, &worker, static_cast<void*>(threadData + i));

    if (ret != 0) 
    {
      cerr << "Could not create thread " << i << endl;
      exit(-1);
    }
  }

  stopwatch.reset(); 
  struct timespec interval; 
 
  bool allInactive = false; 
  bool done = false;
  while(!done)
  {        
    allInactive = true; 
  
    interval.tv_sec = 0; 
    interval.tv_nsec = 10*1000; // 10 ms 
    nanosleep(&interval, NULL);

    double et = getElapsedTime();

    // Do the rounds
    for (unsigned int i = 0; i < wthreads; i++) 
    {
      pthread_mutex_lock(&threadData[i].pmutex);

      if (threadData[i].active) allInactive = false; 

      // start one if we should
      //if (totalIters < nextReport && !threadData[i].active)
      if (et < nextReportTime && !threadData[i].active)
      {
        totalIters += batchcount; 
        threadData[i].active = true; 
        allInactive = false; 

        pthread_cond_signal(&threadData[i].pcv);
      }
        
      pthread_mutex_unlock(&threadData[i].pmutex); 
    }

    //if (allInactive && totalIters >= nextReport)
    if (allInactive && et >= nextReportTime)
    {
      // report!
      iter = totalIters; 

      elapsed_time = getElapsedTime();

      double itpersec = totalIters / elapsed_time;
      cout << endl;
      cout << "Time taken: " << stopwatch.stop() << " seconds. Report time! Total iters = " 
           << totalIters << ", Total elapsed = " 
           << elapsed_time << " (iters/sec = " << itpersec << ")" << endl;

      //total_leafevals += countLeafEvals();
      //total_regupdates += countRegUpdates();
      
      cout << "Computing bounds... "; cout.flush(); 
      double b1 = 0.0, b2 = 0.0;
      seqstore.computeBound(b1, b2); 
      //double bound = getBoundMultiplier("cfrcs")*(2.0*MAX(b1,b2));
      double bound = 2.0*MAX(b1, b2);
      cout << " b1 = " << b1 << ", b2 = " << b2 << ", bound = " << bound << endl;

      // enabled when not big jobs

      double conv = 0.0;
      if (runBR)
      {
        //if (sopt.alg == "cfrmcext") {        
        //  bluffbr_setOpAvPreBRFix(true); 
        //  bluffbr_setCurrentIter(totalIters); 
        //}
        conv = computeBestResponses(false, false);
      }

      //dumpState();
      //report(); 
      //cout << endl;
      report("fsimtreport.txt", elapsed_time, bound, 0, 0, conv);

      //if (totalIters % (5*reportInc) == 0)
      //dumpInfosets("iss");
      string tag = "seqstore-" + to_string(P1DICE) + "-" + to_string(P2DICE) + "-r" + to_string(RECALL); 
      dumpSeqStore(tag);
    
      // when maxIters > 0
      if (maxIters > 0 && totalIters >= maxIters) 
      {
        done = true;
        break;
      }

      lastReport = nextReport;
      nextReport += reportInc;

      lastReportTime = et; 
      nextReportTime += reportIncTime;

      reports++;

      for (unsigned int i = 0; i < wthreads; i++)
      {
        //threadData[i].gbls.leafevals = 0;
        //threadData[i].gbls.regupdates = 0;
        threadData[i].worker_time = 0;
      }

      cout << endl;
      stopwatch.reset();
    }
  }

  cout << "Parent done. Total time: " << elapsed_time << " seconds. Terminating." << endl; 

  for (unsigned int i = 0; i < wthreads; i++) 
    pthread_join( threads[i], NULL);

}


int main(int argc, char ** argv)
{
  init();
  
  if (argc < 2) {
    cerr << "Usage: ./fsicfr <recall value>" << endl;
    exit(-1); 
  }

  if (argc <= 2) 
  {
    string recstr = argv[1];
    RECALL = to_int(recstr); 
    assert(RECALL >= 1); 

    //fsiInitInfosets(); // for ISS version    
    fsiInitSeqStore();   // for seqstore (ss) version

    exit(-1);
  }
  else
  {
    if (argc != 3) { 
      cerr << "Usage: ./fsicfr <recall value> <abstract infoset file>" << endl;
      exit(-1); 
    }
    
    string recstr = argv[1];
    RECALL = to_int(recstr); 
    assert(RECALL >= 1); 

    string filename = argv[2];
    cout << "Reading the infosets from " << filename << "..." << endl;
    seqstore.readFromDisk(filename);  
  }


  if (argc < 2)
  {
    //initInfosets();    // ISS version
    fsiInitSeqStore();   // for seqstore (ss) version
    exit(-1);
  }
  else 
  { 
    string filename = argv[1];
    cout << "Reading the infosets from " << filename << "..." << endl;
    
    //iss.readFromDisk(filename);
    seqstore.readFromDisk(filename);

    if (argc >= 3)
      maxIters = to_ull(argv[2]);
  }  
  
  // get the iteration
  string filename = argv[2];
  vector<string> parts; 
  split(parts, filename, '.'); 
  if (parts.size() != 3 || parts[1] == "initial")
    iter = 0;
  else
    iter = to_ull(parts[1]); 
  cout << "Set iteration to " << iter << endl;
  totalIters = iter; 
  nextReport = totalIters + reportInc;

  parent();
}

