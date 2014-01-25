
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <sys/time.h>

#include "goof.h"

using namespace std;

string to_string(int i)
{
  ostringstream oss; 
  oss << i; 
  return oss.str();
}

std::string to_string(unsigned long long i)
{
  ostringstream oss; 
  oss << i; 
  return oss.str();
}

std::string to_string(double i)
{
  ostringstream oss; 
  oss << i; 
  return oss.str();
}

unsigned long long to_ull(string str)
{
  stringstream stmT;
  unsigned long long iR;

  stmT << str;
  stmT >> iR;

  return iR;
}

int to_int(string str)
{
  stringstream stmT;
  int iR;

  stmT << str;
  stmT >> iR;

  return iR;
}


double to_double(string str)
{
  stringstream stmT;
  double iR;

  stmT << str;
  stmT >> iR;

  return iR;
}


void getSortedKeys(map<int,bool> & m, list<int> & kl)
{
  map<int,bool>::iterator iter;
  for (iter = m.begin(); iter != m.end(); iter++)
  {
    kl.push_back(iter->first);
  }

  kl.sort();
}

bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

void split(vector<string> & tokens, const string line, char delimiter)
{
  // if there is none, then return just the string itself
  //  (already works like this)
  //if (line.find(delimiter) == string::npos)
  //{
  //  tokens.push_back(line);
  //  return;
  //}

  string::size_type index = 0;

  while (index < line.length())
  {
    string::size_type new_index = line.find(delimiter, index);

    if (new_index == string::npos)
    {
      tokens.push_back(line.substr(index));
      break;
    }
    else
    {
      tokens.push_back(line.substr(index, new_index - index));
      index = new_index+1;
    }
  }

  // special case with token as the last character
  if (index == line.length())
    tokens.push_back("");
}

unsigned long long pow2(int i)
{
  unsigned long long answer = 1;
  return (answer << i); 
}


void bubsort(int * array, int size)
{
  bool swapped_flag;

  do
  {
    swapped_flag = false;
    int i;

    for (i = 0; i < (size-1); i++)
    {
      if (array[i] > array[i+1])  // sort increasing
      {
        int tmp = array[i];
        array[i] = array[i+1];
        array[i+1] = tmp;

        swapped_flag = true;
      }
    }
  }
  while (swapped_flag);
}

string infosetkey_to_string(unsigned long long infosetkey)
{
  /*
  int player = (infosetkey & 1) + 1;
  infosetkey >>= 1; 

  string str = "P" + to_string(player);

  int roll = infosetkey & (pow2(iscWidth) - 1); // for iscWidth = 3, 2**3 - 1 = 8-1 = 7
  infosetkey >>= iscWidth;

  str += (" " + to_string(roll)); 
  
  for (int i = 1; i < BLUFFBID; i++) { 
    int bit = (infosetkey >> (BLUFFBID-i)) & 1; 
    if (bit == 1)
    {
      int dice, face;
      convertbid(dice, face, i);
      str += (" " + to_string(dice) + "-" + to_string(face)); 
    }
  }*/

  return "";
}

string getCurDateTime()
{
  char str[200] = { 0 }; 
 
  time_t tval = time(NULL);
  struct tm * tmptr = localtime(&tval); 
  strftime(str, 200, "%Y-%m-%d %H:%M:%S", tmptr); 

  string cppstr = str;
  return cppstr;
}

