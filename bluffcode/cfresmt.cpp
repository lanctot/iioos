
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

  // external sampling globals
  vector<int> savedChanceEvents;
  vector<double> savedChanceProbs;
  unsigned int chanceIndex; 
  bool loadingChance;

} tData; 


static tData * threadData;

// num each thread does before communicating with the parent
unsigned int batchcount = 10000;   

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

extern SequenceStore seqstore;
extern unsigned int bidWidth;
extern int RECALL;

static unsigned long long bitMask = 0;

void sampleOrLoadChanceEvent(GameState & gs, int player, double & sampleProb, tData * data)
{
  int & roll = (player == 1 ? gs.p1roll : gs.p2roll); 
  
  if (data->loadingChance) 
  {
    assert(data->chanceIndex < data->savedChanceEvents.size());
    roll = data->savedChanceEvents[data->chanceIndex];
    sampleProb = data->savedChanceProbs[data->chanceIndex];
    data->chanceIndex++;    
    CHKPROBNZ(sampleProb);
  }
  else 
  {
    sampleChanceEvent(player, roll, sampleProb);
    CHKPROBNZ(sampleProb);
    
    //cout << "sampleing chance roll: " << roll << ", prob " << sampleProb << endl;

    data->savedChanceEvents.push_back(roll); 
    data->savedChanceProbs.push_back(sampleProb);
  }
}

double cfres(GameState & gs, int player, int depth, unsigned long long bidseq, 
             double reach1, double reach2, double chanceReach, int updatePlayer, tData * data) 
{
  CHKPROB(reach1); 
  CHKPROB(reach2); 
  CHKPROBNZ(chanceReach); 

  // check: at terminal node?
  if (terminal(gs))
  {
    return payoff(gs, updatePlayer); 
  }

  nodesTouched++;

  // chance nodes
  if (gs.p1roll == 0) 
  {
    GameState ngs = gs; 
    double sampleProb = 0.0;

    sampleOrLoadChanceEvent(ngs, 1, sampleProb, data);
    
    return cfres(ngs, player, depth+1, bidseq, reach1, reach2, sampleProb*chanceReach, updatePlayer, data); 
  }
  else if (gs.p2roll == 0)
  {
    GameState ngs = gs; 
    double sampleProb = 0.0;
    
    sampleOrLoadChanceEvent(ngs, 2, sampleProb, data);

    return cfres(ngs, player, depth+1, bidseq, reach1, reach2, sampleProb*chanceReach, updatePlayer, data); 
  }
  
  // declare the variables
  Infoset is;
  unsigned long long sskey = 0;
  int action = -1;
  
  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 
  assert(actionshere > 0);
  
  double moveEVs[actionshere]; 
  for (int i = 0; i < actionshere; i++) 
    moveEVs[i] = 0.0;

  // get the info set from the info set store (iss) 
  /*infosetkey = bidseq;  
  infosetkey <<= iscWidth; 
  if (player == 1)
  {
    infosetkey |= gs.p1roll; 
    infosetkey <<= 1; 
    bool ret = iss.get(infosetkey, is, actionshere, 0); 
    assert(ret);
  }
  else if (player == 2)
  {
    infosetkey |= gs.p2roll; 
    infosetkey <<= 1; 
    infosetkey |= 1; 
    bool ret = iss.get(infosetkey, is, actionshere, 0); 
    assert(ret);
  }*/

  //cout << "gs.curbid = " << gs.curbid << endl;

  /*sskey = bidseq;
  sskey <<= 1;
  if (player == 2) sskey |= 1;
  bool ret = seqstore.get(sskey, is, actionshere, 0, (player == 1 ? gs.p1roll : gs.p2roll));
  assert(ret);*/

  getAbsSSKey(gs, sskey, player, bidseq);
  bool ret = seqstore.get(sskey, is, actionshere, 0, (player == 1 ? gs.p1roll : gs.p2roll));
  assert(ret);

  double stratEV = 0.0;

  // traverse or sample actions.
  if (player != updatePlayer)           
  {
    // sample opponent nodes
    double sampleprob = -1; 
    int takeAction = sampleAction(player, is, actionshere, sampleprob, 0.0, false); 
    CHKPROBNZ(sampleprob); 

    // take the action. find the i for this action
    int i;
    for (i = gs.curbid+1; i <= maxBid; i++) 
    {
      action++; 

      if (action == takeAction)
        break; 
    }
 
    assert(i >= gs.curbid+1 && i <= maxBid); 

    unsigned long long newbidseq = bidseq;
    double moveProb = is.curMoveProbs[action]; 

    CHKPROBNZ(moveProb); 
 
    double newreach1 = (player == 1 ? moveProb*reach1 : reach1); 
    double newreach2 = (player == 2 ? moveProb*reach2 : reach2); 

    GameState ngs = gs; 
    ngs.prevbid = gs.curbid;
    ngs.curbid = i; 
    ngs.callingPlayer = player;
    newbidseq |= (1 << (BLUFFBID-i)); 

    /*newbidseq <<= bidWidth; 
    newbidseq |= i; 
    newbidseq &= bitMask; // imperfect recall*/

    // recursive call
    stratEV = cfres(ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, chanceReach, updatePlayer, data);
  }
  else 
  {
    // travers over my nodes
    for (int i = gs.curbid+1; i <= maxBid; i++) 
    {
      // there is a valid action here
      action++;
      assert(action < actionshere);

      unsigned long long newbidseq = bidseq;
      double moveProb = is.curMoveProbs[action]; 

      //CHKPROBNZ(moveProb); 

      double newreach1 = (player == 1 ? moveProb*reach1 : reach1); 
      double newreach2 = (player == 2 ? moveProb*reach2 : reach2); 

      GameState ngs = gs; 
      ngs.prevbid = gs.curbid;
      ngs.curbid = i; 
      ngs.callingPlayer = player;
      newbidseq |= (1ULL << (BLUFFBID-i)); 
    
      /*newbidseq <<= bidWidth; 
      newbidseq |= i;
      newbidseq &= bitMask; // imperfect recall*/
    
      double payoff = cfres(ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, chanceReach, updatePlayer, data);
    
      moveEVs[action] = payoff; 
      stratEV += moveProb*payoff; 
    }
  }

  // on my nodes, update the regrets

  if (player == updatePlayer) 
  {
    for (int a = 0; a < actionshere; a++)
      is.cfr[a] += (moveEVs[a] - stratEV); 
  }

  // on opponent node, update the average strategy

  if (player != updatePlayer) 
  {
    // myreach cancels here
    //double myreach = (player == 1 ? reach1 : reach2); 
    //double sampleProb = myreach*chanceReach;
    //for (int a = 0; a < actionshere; a++)
    //  is.totalMoveProbs[a] += (1.0 / sampleProb) * myreach * is.curMoveProbs[a]; 
    
    for (int a = 0; a < actionshere; a++)
      is.totalMoveProbs[a] += (1.0 / chanceReach) * is.curMoveProbs[a]; 
  }
  
  // we're always  updating, so save back to the store
  //ttlUpdates++;
  
  //iss.put(infosetkey, is, actionshere, 0); 
  seqstore.put(sskey, is, actionshere, 0, (player == 1 ? gs.p1roll : gs.p2roll)); 
 
  return stratEV;
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
      unsigned long long bidseq = 0; 

      data->savedChanceEvents.clear();
      data->savedChanceProbs.clear();
      data->chanceIndex = 0;
      data->loadingChance = false;
      
      GameState gs1; bidseq = 0;
      cfres(gs1, 1, 0, bidseq, 1.0, 1.0, 1.0, 1, data); 

      data->chanceIndex = 0;
      data->loadingChance = true; 
      
      GameState gs2; bidseq = 0;
      cfres(gs2, 1, 0, bidseq, 1.0, 1.0, 1.0, 2, data); 
    }

    data->worker_time += wsw.stop(); 
  
    cout << "=";
    cout.flush();
  }

  return NULL;
}

void parent()
{
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
      
      unsigned int sequence[RECALL];
      initSequenceForward(sequence);
      double avStratEV = fsiAvgStratEV(true, sequence, 0, 0);
      cout << "expected value of av strat = " << avStratEV << endl;
      
      cout << "Computing bounds... "; cout.flush(); 
      double b1 = 0.0, b2 = 0.0;
      seqstore.computeBound(b1, b2); 
      //double bound = getBoundMultiplier("cfrcs")*(2.0*MAX(b1,b2));
      double bound = 2.0*MAX(b1,b2);
      cout << " b1 = " << b1 << ", b2 = " << b2 << ", bound = " << bound << endl;

      // enabled when not big jobs

      double conv = 0, aconv = 0;

      if (runBR)
      {
        //if (sopt.alg == "cfrmcext") {        
        //  bluffbr_setOpAvPreBRFix(true); 
        //  bluffbr_setCurrentIter(totalIters); 
        //}
        conv = computeBestResponses(false, false);
        aconv = absComputeBestResponses(false, false);
      }

      //dumpState();
      //report(); 
      //cout << endl;
      report("cfresmtreport.txt", elapsed_time, bound, 0, 0, conv);

      //if (totalIters % (5*reportInc) == 0)
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
    cerr << "Usage: ./cfresmt <recall value>" << endl;
    exit(-1); 
  }

  if (argc <= 2) 
  {
    string recstr = argv[1];
    RECALL = to_int(recstr); 
    assert(RECALL >= 1);

    fsiTestAddSub();
    cout << numChanceOutcomes(1) << " " << numChanceOutcomes(2) << endl;
    exit(-1);

    //fsiInitInfosets(); // for ISS version    
    fsiInitSeqStore();   // for seqstore (ss) version

    exit(-1);
  }
  else
  {
    if (argc != 3) { 
      cerr << "Usage: ./cfresmt <recall value> <abstract infoset file>" << endl;
      exit(-1); 
    }
    
    string recstr = argv[1];
    RECALL = to_int(recstr); 
    assert(RECALL >= 1);     

    string filename = argv[2];

    if (filename == "dynamic") {
      allocSeqStore();
      seqstore.setDynamic();
    }
    else {
      cout << "Reading the infosets from " << filename << "..." << endl;
      seqstore.readFromDisk(filename);  
    }
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

  // comput the bitmask for imperfect recall
  for (int i = 0; i < RECALL; i++) {    
    for (unsigned int j = 0; j < bidWidth; j++) {
      bitMask <<= 1;
      bitMask |= 1ULL;
    }
  }

  cout << "bitMask = " << bitMask << endl;

  parent();

}

