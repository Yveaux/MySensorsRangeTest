#pragma once

template<typename T>
class histogram
{
public:
	histogram(const size_t numBuckets, const T lowerBound, const T upperBound);
	~histogram();

	void clear();
	void store(const T value);
	size_t size() const;
	T lowerbound() const;
	T lowerbound(const size_t bucket) const;
	T upperbound() const;
	T upperbound(const size_t bucket) const;
	size_t count(const size_t bucket) const;
	double relcount(const size_t bucket) const;
	void decay( const size_t amount );

private:
	size_t whichbucket(const T value) const;
	size_t 	m_numBuckets;
	T* 	   	m_bounds;
	size_t*	m_counts;
	size_t  m_countsTotal;
};

template<typename T>
histogram<T>::histogram(const size_t numBuckets, const T lowerBound, const T upperBound)
 : m_numBuckets(numBuckets)
{
	m_counts = new size_t[numBuckets];
	m_bounds = new T[numBuckets + 1];
	T bound = lowerBound;
	const T step  = (upperBound - lowerBound) / T(numBuckets);
	for (size_t i = 0; i <= numBuckets; ++i)
	{
		m_bounds[i] = bound;
		bound += step;
	}
	clear();
}

template<typename T>
histogram<T>::~histogram()
{
	delete [] m_bounds;
	delete [] m_counts;
}

template<typename T>
void histogram<T>::clear()
{
	for (size_t i = 0; i < m_numBuckets; ++i)
	{
		m_counts[i] = 0;
	}
	m_countsTotal = 0;
}

template<typename T>
void histogram<T>::store(const T value)
{
	if ((value >= lowerbound()) && (value < upperbound()))
	{
//		Serial.print(value); Serial.print(F("->")); Serial.print(whichbucket(value));
		m_counts[whichbucket(value)]++;
		m_countsTotal++;
	}
//	else
//	{
//		Serial.print(value); Serial.print(F("->X"));
//	}
}

template<typename T>
size_t histogram<T>::size() const
{
	return m_numBuckets;
}

template<typename T>
T histogram<T>::lowerbound() const
{
	return m_bounds[0];
}

template<typename T>
T histogram<T>::lowerbound(const size_t bucket) const
{
	if (bucket < m_numBuckets)
	{
		return m_bounds[bucket];
	}
	return m_bounds[m_numBuckets];
}

template<typename T>
T histogram<T>::upperbound() const
{
	return m_bounds[m_numBuckets];
}

template<typename T>
T histogram<T>::upperbound(const size_t bucket) const
{
	if (bucket < m_numBuckets) 
	{
		return m_bounds[bucket+1];
	}
	return m_bounds[m_numBuckets+1];
}

template<typename T>
size_t histogram<T>::count(const size_t bucket) const
{
	if (bucket >= m_numBuckets)
	{
		return 0;
	}	
	return m_counts[bucket];
}

template<typename T>
double histogram<T>::relcount(const size_t bucket) const
{
	if ((bucket >= m_numBuckets) || (0 == m_countsTotal))
	{
		return 0.0;
	}	
	return double(m_counts[bucket]) / double(m_countsTotal);
}

template<typename T>
void histogram<T>::decay( const size_t amount )
{
	if (!amount)
	{
		return;
	}

	for (size_t i = 0; i < m_numBuckets; ++i)
	{
		if (m_counts[i] > amount)
		{
			m_countsTotal -= amount;
			m_counts[i]   -= amount;
		}
		else
		{
			m_countsTotal -= m_counts[i];
			m_counts[i] = 0;
		}
	}
}

template<typename T>
size_t histogram<T>::whichbucket(const T value) const
{
	size_t i = 0;
	while (i < m_numBuckets)
	{
		++i;
		if (value < m_bounds[i])
			return i-1;
	}
	return 0;
}

