
#define FSICFR         0
#define FSIPCS         0

#define ABS(x) ((x) >= 0 ? (x) : (-(x)))
#define CHKDBL(x)    { int c = fpclassify(x); if (!(c == FP_NORMAL || c == FP_ZERO)) cout << "x = " << x << endl; assert(c == FP_NORMAL || c == FP_ZERO); }
#define CHKPROB(x)   CHKDBL((x)); assert((x) >= 0.0 && (x) <= 1.0)
#define CHKPROBNZ(x) CHKDBL((x)); assert((x) > 0.0 && (x) <= 1.0)
#define ABS(x)       ((x) >= 0 ? (x) : (-(x)))
#define MAX(x,y)     ((x) > (y) ? (x) : (y))
#define ASSERTEQZERO(x)    assert((ABS((x))) < 0.00000000000001)
//static const size_t SIZE_MAX = std::numeric_limits<std::size_t>::max();

// note: no support for diefaces other than 6 for P1DICE != 1 or P2DICE != 1

// Old (Liar's Dice values)
//#define VAL21  (0.6190495778863445)
//#define VAL12 (-0.5901367109169530)
//#define VAL22  (0.0053404213561389)
//
// For each estimate, the gap is the the absval of the diff between the upper and lower bounds
// So gap/2 represents the maximum error of the estimate of the value
// 
// VAL11 is exact, using SFLP
// VAL21 gap = 0.000219176101228458
// VAL12 gap = 0.000133214851192665
// VAL22 gap = 0.00274636063062968
// VAL31 gap = 0.000518864860588719
// VAL13 gap = 0.000924095315894879
#define VAL11 (-0.0271317829457364)
#define VAL21 (0.6189107786524395)
#define VAL12 (-0.5882679528387005)
#define VAL22 (0.01265214009195285)
#define VAL31 (0.8645693929896585)
#define VAL13 (-0.8497395132282245)
#define VAL32 (0.0)

// these need to be defined for PCS
#if P1DICE == 1
#define P1CO 6
#elif P1DICE == 2
#define P1CO 21
#elif P1DICE == 3
#define P1CO 56
#elif P1DICE == 4
#define P1CO 126
#elif P1DICE == 5
#define P1CO 252
#endif

#if P2DICE == 1
#define P2CO 6
#elif P2DICE == 2
#define P2CO 21
#elif P2DICE == 3
#define P2CO 56
#elif P2DICE == 4
#define P2CO 126
#elif P2DICE == 5
#define P2CO 252
#endif

#if P1CO > P2CO
#define MAXCO P1CO
#else
#define MAXCO P2CO
#endif

// for probing
#define ISKMAX 10
#define ACTMAX 10 
#if 0
#if (P1DICE + P2DICE) == 2 
#define ISKMAX 131072   // 2^17 (12 for actions, 3 for die, 1 for player + 1)
#define ACTMAX 13 
#elif (P1DICE + P2DICE) == 3
#define ISKMAX 33554432 // 2^25 (18 for actions, 5 for die, 1 for player + 1)
#define ACTMAX 19 
#elif (P1DICE + P2DICE) == 4
#define ISKMAX 10 // 2^25 (18 for actions, 5 for die, 1 for player + 1)
#define ACTMAX 10 
#else
#error "P1DICE + P2DICE not defined for probing"
#endif
#endif


