#ifndef TEST_UTILITY_HPP
#define TEST_UTILITY_HPP

#include <algorithm>
#include <random>

template <class IntType = int>
class /*[[deprecated]]*/ random_generator
{
	static_assert(std::is_integral_v<IntType>, "std::random_generator<T>: T must be a integral type");
public:
	random_generator<IntType>() :
		_seed(std::begin(_random_data),
			  std::transform(std::begin(_random_data), std::end(_random_data),
							 std::begin(_random_data), [this] (auto const &) { return _random_device(); })),
		_engine(_seed),
		_distribution(0, std::numeric_limits<IntType>::max())
	{
	}

	IntType operator()()
	{
		return _distribution(_engine);
	}
private:
	std::random_device _random_device;
	std::array<std::mt19937_64::result_type, std::mt19937_64::state_size> _random_data;
	std::seed_seq _seed;
	std::mt19937_64 _engine;
	std::uniform_int_distribution<IntType> _distribution;
};

#endif
