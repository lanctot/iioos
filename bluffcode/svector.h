
#ifndef __SVECSIZEOR_H__
#define __SVECSIZEOR_H__

#include <string>
#include <sstream>
#include <cassert>
#include <iostream>

/* 
 * Provides all the functionality of SVector but uses static 
 * SIZEs to avoid dynamic memory allocations.
 *
 * Also: assumes doubles since we don't use it for anything else.
 */


template <unsigned int SIZE>
class SVector
{
  double elements[SIZE];

public:  
  SVector() 
  { 
    for (unsigned int i = 0; i < SIZE; i++)
      elements[i] = 0.0;
  }

  SVector(double ival) 
  {
    for (unsigned int i = 0; i < SIZE; i++)
      elements[i] = ival;
  }

  double& operator[](int n) { return elements[n]; }
  double get_const(int n) const { return elements[n]; }

  unsigned int getSize() const { return SIZE; }

  bool allEqualTo(double elem) {
    for (unsigned int i = 0; i < SIZE; i++) 
      if (elements[i] != elem)
        return false; 
    return true;
  }

  SVector<SIZE> & operator+=(SVector<SIZE> & right) 
  {
    assert(SIZE == right.getSize()); 

    for (unsigned int i = 0; i < SIZE; i++) 
      elements[i] += right[i]; 

    return (*this);
  }
 
  // non-standard element by element multiply
  SVector<SIZE> & operator*=(SVector<SIZE> & right) 
  {
    assert(SIZE == right.getSize()); 

    for (unsigned int i = 0; i < SIZE; i++) 
      elements[i] *= right[i]; 

    return (*this);
  }
  
  SVector<SIZE> & operator*=(double factor) 
  {
    for (unsigned int i = 0; i < SIZE; i++) 
      elements[i] *= factor;  

    return (*this);
  }

  std::string to_string() { 
    std::string str = "["; 

    for (unsigned int i = 0; i < SIZE; i++) { 
      std::ostringstream oss; 
      oss << elements[i]; 
      str = str + oss.str(); 
      if (i < (SIZE-1))
        str = str + " "; 
    }

    str = str + "]";

    return str; 
  }

  void assertprob() { 
    for (unsigned int i = 0; i < SIZE; i++) {
      assert(elements[i] >= 0.0 && elements[i] <= 1.0); 
    }
  }

  SVector<SIZE> & operator= (const SVector<SIZE> & other) 
  {
    assert(SIZE == other.getSize());
    for (unsigned int i = 0; i < SIZE; i++) 
      elements[i] = other.elements[i]; 
    return (*this);
  }
  
  void reset(double val) {
    for (unsigned int i = 0; i < SIZE; i++) 
      elements[i] = val;
  }

};

//std::ostream &operator<<(std::ostream &o,  const SVector<SIZE> &v);

#endif
