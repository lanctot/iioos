
#ifndef __FVECTOR_H__
#define __FVECTOR_H__

#include <vector>
#include <string>
#include <sstream>
#include <cassert>
#include <iostream>

// a mutable version of FVector
// hoping it's faster 

template <class T>
class FVector
{
  std::vector<T> elements;
  int size; 

public:  
  FVector() 
    : size(0) 
  { 
  }

  FVector(int _size, T val = 0) { 
    size = _size; 
    elements.resize(0);
    elements.resize(size, val); 
  }

  T& operator[](int n) { return elements[n]; }
  T get_const(int n) const { return elements[n]; }

  //const T& operator[](int n) const { return elements[n]; }
  
  int getSize() const { return size; }

  bool allEqualTo(T elem) {
    for (int i = 0; i < size; i++) 
      if (elements[i] != elem)
        return false; 
    return true;
  }

  FVector<T> & operator+=(FVector<T> & right) 
  {
    assert(size == right.size); 

    for (int i = 0; i < size; i++) 
      elements[i] += right[i]; 

    return (*this);
  }
 
  // non-standard element by element multiply
  FVector<T> & operator*=(FVector<T> & right) 
  {
    assert(size == right.size); 

    for (int i = 0; i < size; i++) 
      elements[i] *= right[i]; 

    return (*this);
  }
  
  // non-standard element by element multiply
  FVector<T> & operator*=(std::vector<T> & right) 
  {
    assert(size == right.size()); 

    for (int i = 0; i < size; i++) 
      elements[i] *= right[i]; 

    return (*this);
  }

  FVector<T> & operator*=(T factor) 
  {
    for (int i = 0; i < size; i++) 
      elements[i] *= factor;  

    return (*this);
  }

  std::string to_string() { 
    std::string str = "["; 

    for (int i = 0; i < size; i++) { 
      std::ostringstream oss; 
      oss << elements[i]; 
      str = str + oss.str(); 
      if (i < (size-1))
        str = str + " "; 
    }

    str = str + "]";

    return str; 
  }

  void assertprob() { 
    for (int i = 0; i < size; i++) {
      assert(elements[i] >= 0 && elements[i] <= 1); 
    }
  }

  FVector<T> & operator= (const FVector<T> & other) 
  {
    size = other.size; 
    elements = other.elements; 
    return (*this);
  }
  
  FVector<T> & operator= (const std::vector<T> & other) 
  {
    size = other.size(); 
    elements = other; 
    return (*this);
  }

  void reset(T val) {
    for (int i = 0; i < size; i++) 
      elements[i] = val;
  }

};

std::ostream &operator<<(std::ostream &o,  const FVector<double> &v);

#endif


