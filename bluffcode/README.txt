
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


