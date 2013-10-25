#ifndef __STATISTICS_HPP__
#define __STATISTICS_HPP__

#include <cassert>

/*
 * Stores stats without keep track of the actual numbers.
 *
 * Implements Knuth's online algorithm for variance, first one
 * found under "Online Algorithm" of
 * http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance
 *
 * Uses the incremental algorithm for updating the mean found at
 * http://webdocs.cs.ualberta.ca/~sutton/book/2/node6.html
 *
 */

class StatCounter {
  
	public:
	  
		StatCounter();

		// reininitialise
		void reset();
		
		// add a sample
		void push(double num);

		// sample variance
		double variance() const;

    // confidence interval
    double ci95() const;

		// sample mean
		double mean() const;

		// number of samples
		size_t size() const;

	private:

		double m_sum;
		double m_m2;
		double m_mean;
		size_t m_n;
};


inline StatCounter::StatCounter() {
	
	reset();
}


/* reininitialise */
inline void StatCounter::reset() {
  
	m_n = 0; 
	m_sum = m_m2 = m_mean = 0.0; 
}


/* add a sample */
inline void StatCounter::push(double num) {

  m_sum += num; 
  m_n++;

  double delta = num - m_mean;
  m_mean += delta / m_n; 
  m_m2 += delta*(num - m_mean);
}


/* sample variance */
inline double StatCounter::variance() const {

	return m_m2 / m_n; 
}

/* sample variance */
inline double StatCounter::ci95() const {

  double stddev = sqrt(variance()); 
	return (1.96*stddev / sqrt(m_n)); 
}


/* sample mean */
inline double StatCounter::mean() const {

	return m_mean;
}


/* number of samples */
inline size_t StatCounter::size() const {
	
	return m_n;
}


// Online computation of the sample covariance between two random variables
//   See "Covariance" at:
//   http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance

class SampleCovPair {

	public:

		SampleCovPair();

		// reset the statistics
		void reset();

		// add a new data point
		void push(double x, double y);

		// unbiased estimate of the covariance
		double covariance() const;

		// number of data points estimate is based on
		size_t size() const;

	private:

		StatCounter m_x;
		StatCounter m_y;
		StatCounter m_xy;
};


inline SampleCovPair::SampleCovPair() {

	reset();
}


/* add a new data point */
inline void SampleCovPair::push(double x, double y) {

	m_x.push(x);
	m_y.push(y);
	m_xy.push(x*y);
}


/* reset the covariance statistics */
inline void SampleCovPair::reset() {

	m_x.reset();
	m_y.reset();
	m_xy.reset();
}


/* unbiased estimate of the covariance */
inline double SampleCovPair::covariance() const {

	return m_xy.mean() - m_x.mean() * m_y.mean();
}


/* how many data points is our covariance estimate based on? */
inline size_t SampleCovPair::size() const {

	return m_x.size();
}

#endif  // __STATISTICS_HPP__

