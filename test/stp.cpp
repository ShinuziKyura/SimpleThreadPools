#include <iostream>
#include <fstream>
#include <string>
#include <array>
#include <algorithm>
#include <random>

#define DYNAMIC_ALLOCATION 0
#define	OUTPUT_TO_FILE 0

#if DYNAMIC_ALLOCATION
#include "..\ptr\stp.hpp" // Minimum standard version: C++14
#else
#include "..\any\stp.hpp" // Minimum standard version: C++17
#endif

using				ARRAY_TYPE			= uint64_t;
constexpr size_t	ARRAY_SIZE			= 1000000;
constexpr size_t	ARRAY_AMOUNT		= 16;
constexpr size_t	THREAD_AMOUNT		= 8;

namespace util
{
	std::string state_to_string(stp::task_state state)
	{
		switch (state)
		{
			case stp::task_state::ready:
				return std::string("ready");
			case stp::task_state::running:
				return std::string("running");
			case stp::task_state::waiting:
				return std::string("waiting");
			case stp::task_state::suspended:
				return std::string("suspended");
			case stp::task_state::null:
				return std::string("null");
		}
		return std::string();
	}
	std::string state_to_string(stp::threadpool_state state)
	{
		switch (state)
		{
			case stp::threadpool_state::running:
				return std::string("running");
			case stp::threadpool_state::stopped:
				return std::string("stopped");
			case stp::threadpool_state::terminating:
				return std::string("terminating");
		}
		return std::string();
	}
}

template <class IntType = int>
class randint_generator
{
	static_assert(std::is_same<IntType, short>::value ||
				  std::is_same<IntType, int>::value ||
				  std::is_same<IntType, long>::value ||
				  std::is_same<IntType, long long>::value ||
				  std::is_same<IntType, unsigned short>::value ||
				  std::is_same<IntType, unsigned int>::value ||
				  std::is_same<IntType, unsigned long>::value ||
				  std::is_same<IntType, unsigned long long>::value,
				  "IntType must be one of \'short\', \'int\', \'long\', \'long long\', "
				  "\'unsigned short\', \'unsigned int\', \'unsigned long\', or \'unsigned long long\'.");
public:
	randint_generator<IntType>(IntType min = 0.0,
							   IntType max = std::numeric_limits<IntType>::max()) :
		_seeds(std::begin(_random_data), std::end(_random_data)),
		_engine(_seeds)
	{
		if (min > max)
		{
			throw std::logic_error("min must be no greater than max");
		}
		_numbers = std::uniform_int_distribution<IntType>{ min, max };
	}

	IntType operator()()
	{
		return _numbers(_engine);
	}
private:
	std::random_device _random_device;
	std::array<std::mt19937::result_type, std::mt19937::state_size> _random_data;
	std::nullptr_t _generate
	{
		(std::generate(std::begin(_random_data), std::end(_random_data), std::ref(_random_device)), nullptr)
	};
	std::seed_seq _seeds;
	std::mt19937 _engine;
	std::uniform_int_distribution<IntType> _numbers;
};

thread_local randint_generator<ARRAY_TYPE> rng;
thread_local std::chrono::steady_clock::time_point start_timer, stop_timer;

template <class ArrayType, size_t ArraySize>
long double generator(std::array<ArrayType, ArraySize> & array)
{
	start_timer = std::chrono::steady_clock::now();
	std::generate(std::begin(array), std::end(array), std::ref(rng));
	stop_timer = std::chrono::steady_clock::now();
	return std::chrono::duration<long double, std::nano>(stop_timer - start_timer).count();
}

template <class ArrayType, size_t ArraySize>
long double sorter(std::array<ArrayType, ArraySize> & array)
{
	start_timer = std::chrono::steady_clock::now();
	std::sort(std::begin(array), std::end(array));
	stop_timer = std::chrono::steady_clock::now();
	return std::chrono::duration<long double, std::nano>(stop_timer - start_timer).count();
}

long double test()
{
	stp::threadpool threadpool(THREAD_AMOUNT);

	std::vector<stp::task<long double>> tasks;
	tasks.reserve(ARRAY_AMOUNT);
	
	std::array<std::unique_ptr<std::array<ARRAY_TYPE, ARRAY_SIZE>>, ARRAY_AMOUNT> arrays;
	std::generate(std::begin(arrays), std::end(arrays), std::make_unique<std::array<ARRAY_TYPE, ARRAY_SIZE>>);

	long double total_time = 0;

	std::cout <<
		"\tThreadpool size: " << threadpool.size() << "\n"
		"\tThreadpool state: " << util::state_to_string(threadpool.state()) << "\n\n";

	//	Array generation

	{
		std::cout <<
			"\tArray generation begin...\n\n";

		for (auto & array : arrays)
		{
			tasks.emplace_back(generator<ARRAY_TYPE, ARRAY_SIZE>, *array);
		}

		start_timer = std::chrono::steady_clock::now();

		for (auto & task : tasks)
		{
			threadpool.push_task(task);
		}
		
		while (!std::all_of(std::begin(tasks), std::end(tasks), [] (stp::task<long double> & task)
		{
			return task.state() == stp::task_state::ready;
		}))
		{
			std::this_thread::yield();
		}

		stop_timer = std::chrono::steady_clock::now();

		std::cout <<
			"\t\tTime elapsed per array:\n";

		long double sum_result = 0.0;
		for (auto & task : tasks)
		{
			sum_result += task.get();
			std::cout <<
				"\t\t" << task.get() << " ns\n";
		}

		std::cout <<
			"\n\t\tAverage time elapsed per array:\n"
			"\t\t" << sum_result / ARRAY_AMOUNT << " ns/array\n\n"
			"\tTotal time elapsed:\n"
			"\t" << std::chrono::duration<long double>(stop_timer - start_timer).count() << "s\n\n"
			"\tArray generation end\n\n";
	}

	total_time += std::chrono::duration<long double>(stop_timer - start_timer).count();

	tasks.resize(0);
	threadpool.stop();

	//	Array sorting

	{
		std::cout <<
			"\tArray sorting begin...\n\n";

		for (auto & array : arrays)
		{
			tasks.emplace_back(generator<ARRAY_TYPE, ARRAY_SIZE>, *array);
			threadpool.push_task(tasks.back());
		}
		
		start_timer = std::chrono::steady_clock::now();

		threadpool.run();

		while (!std::all_of(std::begin(tasks), std::end(tasks), [] (stp::task<long double> & task)
		{
			return task.state() == stp::task_state::ready;
		}))
		{
			std::this_thread::yield();
		}

		stop_timer = std::chrono::steady_clock::now();

		std::cout <<
			"\t\tTime elapsed per array:\n";

		long double sum_result = 0.0;
		for (auto & task : tasks)
		{
			sum_result += task.get();
			std::cout <<
				"\t\t" << task.get() << " ns\n";
		}

		std::cout <<
			"\n\t\tAverage time elapsed per array:\n"
			"\t\t" << sum_result / ARRAY_AMOUNT << " ns/array\n\n"
			"\tTotal time elapsed:\n"
			"\t" << std::chrono::duration<long double>(stop_timer - start_timer).count() << "s\n\n"
			"\tArray sorting end\n\n";
	}

	total_time += std::chrono::duration<long double>(stop_timer - start_timer).count();
	return total_time;
}

int main()
{
	std::setvbuf(stdout, nullptr, _IOFBF, BUFSIZ);

	std::ios_base::sync_with_stdio(false);

#if (OUTPUT_TO_FILE)
	std::fstream fout("stp.tests", std::ios::out | std::ios::trunc);
	std::streambuf * cout_buffer = std::cout.rdbuf(fout.rdbuf());
#endif

	std::cout << std::scientific <<
		"Test begin...\n" << std::endl;

	long double total_time = test();

	std::cout <<
		"Total time elapsed:\n" <<
		total_time << " s\n\n"
		"Test end\n" << std::endl;

#if (OUTPUT_TO_FILE)
	fout.close();
	std::cout.rdbuf(cout_buffer);
#endif

	std::cout <<
		"======================\n\n"
		"Press enter to exit..." << std::endl;

	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

	return 0;
}
