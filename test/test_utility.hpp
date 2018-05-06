#ifndef TEST_UTILITY_HPP
#define TEST_UTILITY_HPP

#include <algorithm>
#include <random>

// TODO Overhaul this class

template <class IntType = int>
class random_generator
{
	static_assert(std::is_integral_v<IntType>, "std::random_generator<T>: T must be a integral type");
public:
	random_generator<IntType>() :
		_seeds(std::transform(std::begin(_data), std::end(_data), std::begin(_data), [] (auto &) { return std::chrono::duration_cast<
							  std::chrono::template duration<std::mt19937_64::result_type, std::nano>
		>(std::chrono::steady_clock::now().time_since_epoch()).count(); }), std::end(_data)),
		_engine(_seeds),
		_numbers(0, std::numeric_limits<IntType>::max())
	{
	}

	IntType operator()()
	{
		return _numbers(_engine);
	}
private:
	std::array<std::mt19937_64::result_type, std::mt19937_64::state_size> _data;
	std::seed_seq _seeds;
	std::mt19937_64 _engine;
	std::uniform_int_distribution<IntType> _numbers;
};

#endif
