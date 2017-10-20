#include "stp.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <array>
#include <algorithm>
#include <random>

constexpr bool		OUTPUT_TO_FILE		= false;
using				ARRAY_TYPE			= unsigned long long;
const size_t		ARRAY_SIZE			= 1000000;
const size_t		ARRAY_AMOUNT		= 16;
const size_t		THREAD_AMOUNT		= std::thread::hardware_concurrency();

template <class IntType>
class random_number_generator
{
public:
	IntType gen()
	{
		return numbers_(engine_);
	}

	random_number_generator<IntType>() :
		random_data_(),
		random_device_(),
		random_generator_((std::generate(std::begin(random_data_), std::end(random_data_), std::ref(random_device_)),
						   nullptr)),
		seeds_(std::begin(random_data_), std::end(random_data_)),
		engine_(seeds_),
		numbers_(std::numeric_limits<IntType>::min(), std::numeric_limits<IntType>::max())
	{
	}
private:
	std::array<std::mt19937::result_type, std::mt19937::state_size> random_data_;
	std::random_device random_device_;
	std::nullptr_t random_generator_;
	std::seed_seq seeds_;
	std::mt19937 engine_;
	std::uniform_int_distribution<IntType> numbers_;

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
};

thread_local	random_number_generator<ARRAY_TYPE> rng;
				std::chrono::steady_clock::time_point start_test, stop_test;
thread_local	std::chrono::steady_clock::time_point start_timer, stop_timer;

template <class ArrayType, size_t ArraySize>
long double generator(std::array<ArrayType, ArraySize> & arr)
{
	start_timer = std::chrono::steady_clock::now();
	std::generate(std::begin(arr), std::end(arr), std::bind(&random_number_generator<ArrayType>::gen, &rng));
	stop_timer = std::chrono::steady_clock::now();
	return std::chrono::duration<long double, std::nano>(stop_timer - start_timer).count();
}

template <class ArrayType, size_t ArraySize>
long double sorter(std::array<ArrayType, ArraySize> & arr)
{
	start_timer = std::chrono::steady_clock::now();
	std::sort(std::begin(arr), std::end(arr));
	stop_timer = std::chrono::steady_clock::now();
	return std::chrono::duration<long double, std::nano>(stop_timer - start_timer).count();
}

std::string threadpool_state_to_string(stp::threadpool_state state)
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

void test()
{
	stp::threadpool threadpool(THREAD_AMOUNT, false);

	std::vector<stp::task<long double>> tasks;
	tasks.reserve(ARRAY_AMOUNT);

	std::array<std::unique_ptr<std::array<ARRAY_TYPE, ARRAY_SIZE>>, ARRAY_AMOUNT> arrays;
	std::generate(std::begin(arrays), std::end(arrays), std::make_unique<std::array<ARRAY_TYPE, ARRAY_SIZE>>);

	std::cout <<
		"\tThreadpool size: " << threadpool.size() << "\n"
		"\tThreadpool notifications: " << (threadpool.notify() ? "active" : "inactive") << "\n"
		"\tThreadpool state: " << threadpool_state_to_string(threadpool.state()) << "\n\n";

	{	// Array generation
		std::cout <<
			"\tArray generation begin...\n\n";

		for (auto & array : arrays)
		{
			tasks.emplace_back(generator<ARRAY_TYPE, ARRAY_SIZE>, *array);
			threadpool.new_task(tasks.back());
		}

		start_test = std::chrono::steady_clock::now();

		threadpool.notify_threads();

		while (!std::all_of(std::begin(tasks), std::end(tasks),
							[] (stp::task<long double> & task) { return task.ready(); }))
		{
			std::this_thread::yield();
		}

		stop_test = std::chrono::steady_clock::now();

		std::cout <<
			"\t\tTime elapsed per array:\n";

		long double sum_result = 0.0;
		for (auto & task : tasks)
		{
			sum_result += task.result();
			std::cout << "\t\t" << task.result() << " ns\n";
		}

		std::cout <<
			"\n\t\tAverage time elapsed per array:\n\t\t" <<
			sum_result / ARRAY_AMOUNT << " ns/array\n\n"
			"\tArray generation end\n\n";
	}

	tasks.resize(0);

/*	{	// Array sorting
		std::cout <<
			"\tArray sorting begin...\n\n"
			"\t\tFirst four arrays\n";

		for (auto & array : arrays)
		{
			tasks.emplace_back(sorter<ARRAY_TYPE, ARRAY_SIZE>, *array);
			threadpool.new_task(tasks.back());
		}
	} //*/
}

int main()
{
	std::setvbuf(stdout, nullptr, _IOFBF, BUFSIZ);

	std::cout << std::scientific << 
		"Test begin...\n" << std::endl;

#if (OUTPUT_TO_FILE)
	std::fstream fout("stp.tests", std::ios::out | std::ios::trunc);
	std::streambuf * cout_buffer = std::cout.rdbuf(fout.rdbuf());
#endif

	test(); // The testbed is not yet complete

#if (OUTPUT_TO_FILE) // This may need adjusting
	std::cout << std::flush;
	fout.close();
	std::cout.rdbuf(cout_buffer);
#endif

	std::cout << 
		"Test end\n\n"
		"\tTotal time elapsed:\n\t" << 
		std::chrono::duration<long double>(stop_test - start_test).count() << " s\n\n"
		"Press enter to exit..." << std::endl;
	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

	return 0;
}
