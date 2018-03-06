#include <iostream>
#include <fstream>
#include <array>

#include "test_utility.hpp"

// Compilation variables

#define DYNAMIC_ALLOCATION 1
#define	FILE_OUTPUT 0

#if DYNAMIC_ALLOCATION
#include "../dyn/stp.hpp" // Standard revision required: C++14
#else
#include "../hyb/stp.hpp" // Standard revision required: C++17
#endif

// Test variables

using				ARRAY_TYPE			= uint_least64_t;
constexpr size_t	ARRAY_SIZE			= 1000000;
constexpr size_t	ARRAY_AMOUNT		= 16;
constexpr size_t	THREAD_AMOUNT		= 8;

// Test utilities

thread_local random_generator<ARRAY_TYPE> rng;
thread_local std::chrono::steady_clock::time_point start_timer, stop_timer;

// Test methods

long double generator(std::array<ARRAY_TYPE, ARRAY_SIZE> & array)
{
	start_timer = std::chrono::steady_clock::now();
	std::generate(std::begin(array), std::end(array), std::ref(rng));
	stop_timer = std::chrono::steady_clock::now();
	return std::chrono::duration<long double, std::nano>(stop_timer - start_timer).count();
}

long double sorter(std::array<ARRAY_TYPE, ARRAY_SIZE> & array)
{
	start_timer = std::chrono::steady_clock::now();
	std::sort(std::begin(array), std::end(array));
	stop_timer = std::chrono::steady_clock::now();
	return std::chrono::duration<long double, std::nano>(stop_timer - start_timer).count();
}

// Test suite

long double single_thread_test()
{
	std::array<stp::task<long double>, ARRAY_AMOUNT> tasks;
	std::array<std::unique_ptr<std::array<ARRAY_TYPE, ARRAY_SIZE>>, ARRAY_AMOUNT> arrays;

	long double total_time = 0;

	std::chrono::steady_clock::time_point start_single_timer, stop_single_timer;

	std::generate(std::begin(arrays), std::end(arrays), std::make_unique<std::array<ARRAY_TYPE, ARRAY_SIZE>>);
	std::transform(std::begin(arrays), std::end(arrays), std::begin(tasks), [] (auto & array) { return stp::make_task(generator, *array); });

	//	Array generation

	{
		std::cout <<
			"\tArray generation begin...\n\n";

		start_single_timer = std::chrono::steady_clock::now();

		for (auto & task : tasks)
		{
			task();
		}

		stop_single_timer = std::chrono::steady_clock::now();

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
			"\t" << sum_result << "s\n\n"
			"\tArray generation end\n\n";
	}

	total_time += std::chrono::duration<long double>(stop_single_timer - start_single_timer).count();

	std::transform(std::begin(arrays), std::end(arrays), std::begin(tasks), [] (auto & array) { return stp::make_task(sorter, *array); });

	//	Array sorting

	{
		std::cout <<
			"\tArray sorting begin...\n\n";

		start_single_timer = std::chrono::steady_clock::now();

		for (auto & task : tasks)
		{
			task();
		}

		stop_single_timer = std::chrono::steady_clock::now();

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
			"\t" << sum_result << "s\n\n"
			"\tArray sorting end\n\n";
	}

	return total_time + std::chrono::duration<long double>(stop_single_timer - start_single_timer).count();
}

long double multi_thread_test()
{
	std::array<stp::task<long double>, ARRAY_AMOUNT> tasks;
	std::array<std::unique_ptr<std::array<ARRAY_TYPE, ARRAY_SIZE>>, ARRAY_AMOUNT> arrays;

	long double total_time = 0;

	std::generate(std::begin(arrays), std::end(arrays), std::make_unique<std::array<ARRAY_TYPE, ARRAY_SIZE>>);
	std::transform(std::begin(arrays), std::end(arrays), std::begin(tasks), [] (auto & array) { return stp::make_task(generator, *array); });

	//	Array generation

	{
		std::cout <<
			"\tArray generation begin...\n\n";

		start_timer = std::chrono::steady_clock::now();

		for (auto & task : tasks)
		{
			std::thread(&stp::task<long double>::operator(), &task).detach();
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
			"\t" << sum_result / THREAD_AMOUNT << "s\n\n"
			"\tArray generation end\n\n";
	}

	total_time += std::chrono::duration<long double>(stop_timer - start_timer).count();

	std::transform(std::begin(arrays), std::end(arrays), std::begin(tasks), [] (auto & array) { return stp::make_task(sorter, *array); });

	//	Array sorting

	{
		std::cout <<
			"\tArray sorting begin...\n\n";

		start_timer = std::chrono::steady_clock::now();

		for (auto & task : tasks)
		{
			std::thread(&stp::task<long double>::operator(), &task).detach();
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
			"\t" << sum_result / THREAD_AMOUNT << "s\n\n"
			"\tArray sorting end\n\n";
	}

	return total_time + std::chrono::duration<long double>(stop_timer - start_timer).count();
}

long double thread_pool_test()
{
	stp::threadpool threadpool(THREAD_AMOUNT);

	std::array<stp::task<long double>, ARRAY_AMOUNT> tasks;
	std::array<std::unique_ptr<std::array<ARRAY_TYPE, ARRAY_SIZE>>, ARRAY_AMOUNT> arrays;

	long double total_time = 0;

	std::generate(std::begin(arrays), std::end(arrays), std::make_unique<std::array<ARRAY_TYPE, ARRAY_SIZE>>);
	std::transform(std::begin(arrays), std::end(arrays), std::begin(tasks), [] (auto & array) { return stp::make_task(generator, *array); });

	//	Array generation

	{
		std::cout <<
			"\tArray generation begin...\n\n";

		start_timer = std::chrono::steady_clock::now();

		for (auto & task : tasks)
		{
			threadpool.push(task);
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
			"\t" << sum_result / THREAD_AMOUNT << "s\n\n"
			"\tArray generation end\n\n";
	}

	total_time += std::chrono::duration<long double>(stop_timer - start_timer).count();

	std::transform(std::begin(arrays), std::end(arrays), std::begin(tasks), [] (auto & array) { return stp::make_task(sorter, *array); });

	//	Array sorting

	{
		std::cout <<
			"\tArray sorting begin...\n\n";

		start_timer = std::chrono::steady_clock::now();

		for (auto & task : tasks)
		{
			threadpool.push(task);
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
			"\t" << sum_result / THREAD_AMOUNT << "s\n\n"
			"\tArray sorting end\n\n";
	}

	return total_time + std::chrono::duration<long double>(stop_timer - start_timer).count();
}

// Main

int main()
{
	std::setvbuf(stdout, nullptr, _IOFBF, BUFSIZ);

	std::ios_base::sync_with_stdio(false);

	long double total_time = 0;

#if (FILE_OUTPUT)
	std::fstream fout("./test/stp.tests", std::ios::out | std::ios::trunc);
	std::streambuf * cout_buffer = std::cout.rdbuf(fout.rdbuf());
#endif

	std::cout << std::scientific <<
		"Single thread test begin...\n" << std::endl;

	total_time = single_thread_test();

	std::cout <<
		"Total time locked:\n" <<
		total_time << " ns\n\n"
		"Single thread test end\n\n"
		"======================\n" << std::endl;

	std::cout <<
		"Multi thread test begin...\n" << std::endl;

	total_time = multi_thread_test();

	std::cout <<
		"Total time locked:\n" <<
		total_time << " ns\n\n"
		"Multi thread test end\n\n"
		"======================\n" << std::endl;

	std::cout <<
		"Thread pool test begin...\n" << std::endl;

	total_time = thread_pool_test();

	std::cout <<
		"Total time locked:\n" <<
		total_time << " ns\n\n"
		"Thread pool test end\n\n"
		"======================\n" << std::endl;

#if (FILE_OUTPUT)
	fout.close();
	std::cout.rdbuf(cout_buffer);
#endif

	std::cout <<
		"Press enter to exit..." << std::endl;

	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

	return 0;
}
