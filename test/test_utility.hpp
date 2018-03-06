#ifndef TEST_UTILITY_HPP
#define TEST_UTILITY_HPP

#include <algorithm>
#include <random>

// Simplified version of my stdx::randint_generator

template <class IntType = int>
class random_generator
{
public:
	random_generator<IntType>(IntType min = 0.0, IntType max = std::numeric_limits<IntType>::max()) :
		_seeds((std::generate(std::begin(_random_data), std::end(_random_data), std::ref(_random_device)),
				std::begin(_random_data)),
			   std::end(_random_data)),
		_engine(_seeds),
		_numbers(min, max)
	{
	}

	IntType operator()()
	{
		return _numbers(_engine);
	}
private:
	std::random_device _random_device;
	std::array<std::mt19937::result_type, std::mt19937::state_size> _random_data;
	std::seed_seq _seeds;
	std::mt19937 _engine;
	std::uniform_int_distribution<IntType> _numbers;
};

#endif
