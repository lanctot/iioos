
#include <cassert>
#include <iostream>
#include <cstdlib>

#include "bluff.h"

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


double cfrcs(GameState & gs, int player, int depth, unsigned long long bidseq, 
             double reach1, double reach2, int phase, int updatePlayer)
{
  // at terminal node?
  if (terminal(gs))
  {
    return payoff(gs, updatePlayer);
  }

  // chance nodes
  if (gs.p1roll == 0) 
  {
    double EV = 0.0; 

    int outcome = 0;
    double prob = 0.0;
    sampleChanceEvent(1, outcome, prob); 

    CHKPROBNZ(prob);
    assert(outcome > 0); 

    GameState ngs = gs; 
    ngs.p1roll = outcome; 

    EV += getChanceProb(1,outcome)*cfrcs(ngs, player, depth+1, bidseq, reach1, reach2, phase, updatePlayer); 

    return EV;
  }
  else if (gs.p2roll == 0)
  {
    double EV = 0.0; 
    
    int outcome = 0;
    double prob = 0.0;
    sampleChanceEvent(2, outcome, prob); 

    CHKPROBNZ(prob);
    assert(outcome > 0); 

    GameState ngs = gs; 
    ngs.p2roll = outcome;
    
    EV += getChanceProb(2,outcome)*cfrcs(ngs, player, depth+1, bidseq, reach1, reach2, phase, updatePlayer); 

    return EV;
  }

  // check for cuts   // must include the updatePlayer
  if (phase == 1 && (   (player == 1 && updatePlayer == 1 && reach2 <= 0.0)
                     || (player == 2 && updatePlayer == 2 && reach1 <= 0.0)))
  {
    phase = 2; 
  }

  if (phase == 2 && (   (player == 1 && reach1 <= 0.0)
                     || (player == 2 && reach2 <= 0.0)))
  {
    return 0.0;
  }

  // declare the variables
  Infoset is;
  unsigned long long infosetkey = 0;
  double stratEV = 0.0;
  int action = -1;

  int maxBid = (gs.curbid == 0 ? BLUFFBID-1 : BLUFFBID);
  int actionshere = maxBid - gs.curbid; 
  // count the number of actions and initialize moveEVs
  // special case. can't just call bluff on the first move!
  assert(actionshere > 0);
  double moveEVs[actionshere]; 
  for (int i = 0; i < actionshere; i++) 
    moveEVs[i] = 0.0;

  // get the info set
  infosetkey = bidseq;  
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
  }

  // iterate over the actions
  for (int i = gs.curbid+1; i <= maxBid; i++) 
  {
    // there is a valid action here
    action++;
    assert(action < actionshere);

    unsigned long long newbidseq = bidseq;
    double moveProb = is.curMoveProbs[action]; 
    double newreach1 = (player == 1 ? moveProb*reach1 : reach1); 
    double newreach2 = (player == 2 ? moveProb*reach2 : reach2); 

    GameState ngs = gs; 
    ngs.prevbid = gs.curbid;
    ngs.curbid = i; 
    ngs.callingPlayer = player;
    newbidseq |= (1ULL << (BLUFFBID-i)); 
    
    double payoff = cfrcs(ngs, 3-player, depth+1, newbidseq, newreach1, newreach2, phase, updatePlayer); 
   
    moveEVs[action] = payoff; 
    stratEV += moveProb*payoff; 
  }

  // post-traversals: update the infoset
  double myreach = (player == 1 ? reach1 : reach2); 
  double oppreach = (player == 1 ? reach2 : reach1); 

  if (phase == 1 && player == updatePlayer) // regrets
  {
    for (int a = 0; a < actionshere; a++)
    {
      is.cfr[a] += oppreach*(moveEVs[a] - stratEV); 
    }
  }

  if (phase >= 1 && player == updatePlayer) // av. strat
  {
    for (int a = 0; a < actionshere; a++)
    {
      is.totalMoveProbs[a] += myreach*is.curMoveProbs[a]; 
    }
  }

  // save the infoset back to the store if needed
  if (player == updatePlayer) {
    totalUpdates++;
    iss.put(infosetkey, is, actionshere, 0); 
  }

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
      
      GameState gs1; 
      bidseq = 0; 
      cfrcs(gs1, 1, 0, bidseq, 1.0, 1.0, 1, 1);

      bidseq = 0;
      
      GameState gs2; 
      cfrcs(gs2, 1, 0, bidseq, 1.0, 1.0, 1, 2);
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
      
      cout << "Computing bounds... "; cout.flush(); 
      double b1 = 0.0, b2 = 0.0;
      iss.computeBound(b1, b2); 
      double bound = getBoundMultiplier("cfrcs")*(2.0*MAX(b1,b2));
      cout << " b1 = " << b1 << ", b2 = " << b2 << ", bound = " << bound << endl;

      // enabled when not big jobs

      if (runBR)
      {
        //if (sopt.alg == "cfrmcext") {        
        //  bluffbr_setOpAvPreBRFix(true); 
        //  bluffbr_setCurrentIter(totalIters); 
        //}
        computeBestResponses(false, false);
      }

      //dumpState();
      //report(); 
      //cout << endl;
      report("cfrcsmtreport.txt", elapsed_time, bound, 0, 0, 0);

      //if (totalIters % (5*reportInc) == 0)
      if (reports % 5 == 0)
        dumpInfosets("iss");
    
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

  if (argc < 2)
  {
    initInfosets();
    exit(-1);
  }
  else 
  { 
    string filename = argv[1];
    cout << "Reading the infosets from " << filename << "..." << endl;
    iss.readFromDisk(filename);

    if (argc >= 3)
      maxIters = to_ull(argv[2]);
  }  
  
  // get the iteration
  string filename = argv[1];
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

