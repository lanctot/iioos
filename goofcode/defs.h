
#define ABS(x) ((x) >= 0 ? (x) : (-(x)))
#define CHKDBL(x)    { int c = fpclassify(x); if (!(c == FP_NORMAL || c == FP_ZERO)) cout << "x = " << x << endl; assert(c == FP_NORMAL || c == FP_ZERO); }
#define CHKPROB(x)   CHKDBL((x)); assert((x) >= 0.0 && (x) <= 1.0)
#define CHKPROBNZ(x) CHKDBL((x)); assert((x) > 0.0 && (x) <= 1.0)
#define ABS(x)       ((x) >= 0 ? (x) : (-(x)))
#define MAX(x,y)     ((x) > (y) ? (x) : (y))
#define ASSERTEQZERO(x)    assert((ABS((x))) < 0.00000000000001)

// Player types for search
#define PLYR_KEYBOARD    0 
#define PLYR_MCTS        1
#define PLYR_OOS         2 
#define PLYR_STRAT       3
#define PLYR_RANDOM      4

