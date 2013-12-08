
To compile: 

  cmake .
  make

To run, first generate the information set store:
  
  ./cfros 
  
Then:
 
  ./cfros scratch/iss.initial.dat

Bluff(X,Y) is explained in Ch. 3 of my thesis. This code has X = Y = 1 by default.

There is a lot more code than you need. So, I suggest you stick to only the relevant files:

  Outcome sampling (start here):    cfros.cpp
  Online Outcome Sampling (so far): cfroos.cpp 

To run simulations: 

  ./sim scratch/iss.initial.dat scratch/iss.initial.dat <player 1 type> <player 2 type> 

  The types are at the top of sim.cpp, but only these are working for the moment:
    0 = keyboard
    2 = OOS
    4 = random

