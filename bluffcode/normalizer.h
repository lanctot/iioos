#ifndef __NORMALIZER_H__
#define __NORMALIZER_H__

#include <map>
#include <cassert>
#include <vector>
#include <iostream>

#include "bluff.h"

class NormalizerVector
{
  bool normalized;
  double total; 
  std::vector<double> values; 

public:

  NormalizerVector() 
  {
    normalized = false;
    total = 0.0; 
  }

  void push_back(double val) 
  {
    assert(val >= 0.0);
    total += val; 
    values.push_back(val); 
  }

  void normalize()
  {
    assert(total > 0.0); 
    
    //if (total <= 0) {
    //  for (unsigned int i = 0; i < values.size(); i++)
    //    values[i] = (1.0 / values.size()); 
    //  normalized = true;
    //  return;
    //}
    
    assert(!normalized); 

    for (unsigned int i = 0; i < values.size(); i++)
      values[i] = (values[i] / total); 

    normalized = true; 
  }

  unsigned size() const { return values.size(); }
  
  double & operator[](int n) { return values[n]; }

  std::string to_string() {
    std::string str = "";

    for (unsigned int i = 0; i < values.size(); i++) 
      str += (::to_string(values[i]) + " ");

    return str; 
  }

};

// integer keys
class NormalizerMap
{
  bool normalized;
  double total; 
  std::map<int,double> values; 

public:

  NormalizerMap() 
  {
    normalized = false;
    total = 0.0; 
  }

  void add(int key, double value) 
  {
    assert(value >= 0.0);
    total += value;
    values[key] = value;
  }

  void normalize()
  {
    assert(total > 0.0); 
    
    //if (total <= 0) {
    //  std::map<int,double>::iterator iter; 
    //  for (iter = values.begin(); iter != values.end(); iter++)
    //    iter->second = (1.0 / values.size());
    //  normalized = true;
    //  return;
    //}

    assert(!normalized); 

    std::map<int,double>::iterator iter; 
    for (iter = values.begin(); iter != values.end(); iter++)
      iter->second = (iter->second / total); 

    normalized = true; 
  }
  
  double & operator[](int n) { return values[n]; }
  
  unsigned size() const { return values.size(); }

  std::string to_string() {
    std::string str = "";

    std::map<int,double>::iterator iter; 

    for (iter = values.begin(); iter != values.end(); iter++) 
      str += (::to_string(iter->first) + " -> " + ::to_string(iter->second) + " ");

    return str; 
  }
};

#endif

