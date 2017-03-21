#include "stp.h"

#include <iostream>
#include <algorithm>
#include <random>

constexpr int thread_single = 1;
constexpr int thread_amount = 0; // 0 == std::thread::hardware_concurrency()

std::random_device seed;
std::mt19937 generate(seed());
std::uniform_int_distribution<int> random_list(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
thread_local std::chrono::time_point<std::chrono::high_resolution_clock> start_timer, stop_timer;

double generator(std::vector<int> & vec)
{
	start_timer = std::chrono::high_resolution_clock::now();
	for (auto & i : vec)
	{
		i = random_list(generate);
	}
	stop_timer = std::chrono::high_resolution_clock::now();
	return std::chrono::duration<double, std::nano>(stop_timer - start_timer).count();
}

double quicksort(std::vector<int> & vec)
{
	start_timer = std::chrono::high_resolution_clock::now();
	std::sort(vec.begin(), vec.end(), [] (int const & i1, int const & i2) -> bool
	{
		return i1 < i2;
	});
	stop_timer = std::chrono::high_resolution_clock::now();
	return std::chrono::duration<double, std::nano>(stop_timer - start_timer).count();
}

int main()
{
	std::chrono::time_point<std::chrono::high_resolution_clock> start_timer, stop_timer;

	// Generating vectors

	std::cout << "Generating vector 1..." << std::endl;
	std::vector<int> vec_1(1000000);
	std::cout << "Elapsed: " << generator(vec_1) << "ns" << std::endl;

	std::cout << "Generating vector 2..." << std::endl;
	std::vector<int> vec_2(1000000);
	std::cout << "Elapsed: " << generator(vec_2) << "ns" << std::endl;

	std::cout << "Generating vector 3..." << std::endl;
	std::vector<int> vec_3(1000000);
	std::cout << "Elapsed: " << generator(vec_3) << "ns" << std::endl;

	std::cout << "Generating vector 4..." << std::endl;
	std::vector<int> vec_4(1000000);
	std::cout << "Elapsed: " << generator(vec_4) << "ns" << std::endl;

	std::cout << "Generating vector 5..." << std::endl;
	std::vector<int> vec_5(1000000);
	std::cout << "Elapsed: " << generator(vec_5) << "ns" << std::endl;

	std::cout << "Generating vector 6..." << std::endl;
	std::vector<int> vec_6(1000000);
	std::cout << "Elapsed: " << generator(vec_6) << "ns" << std::endl;

	std::cout << "Generating vector 7..." << std::endl;
	std::vector<int> vec_7(1000000);
	std::cout << "Elapsed: " << generator(vec_7) << "ns" << std::endl;

	std::cout << "Generating vector 8..." << std::endl;
	std::vector<int> vec_8(1000000);
	std::cout << "Elapsed: " << generator(vec_8) << "ns" << std::endl;

	// Sorting first four vectors (without concurrency)

	{
		stp::threadpool threadpool_1(thread_single);
		stp::task<double> task_1(quicksort, std::ref(vec_1));
		stp::task<double> task_2(quicksort, std::ref(vec_2));
		stp::task<double> task_3(quicksort, std::ref(vec_3));
		stp::task<double> task_4(quicksort, std::ref(vec_4));

		threadpool_1.new_task(task_1);
		threadpool_1.new_task(task_2);
		threadpool_1.new_task(task_3);
		threadpool_1.new_task(task_4);

		std::cout << "Sorting first four vectors... " << std::endl;
		start_timer = std::chrono::high_resolution_clock::now();

		threadpool_1.run();

		std::cout << "Sorting vector 1..." << std::endl;
		while (!task_1.ready());
		std::cout << "Elapsed: " << task_1.result() << "ns" << std::endl;

		std::cout << "Sorting vector 2..." << std::endl;
		while (!task_2.ready());
		std::cout << "Elapsed: " << task_2.result() << "ns" << std::endl;

		std::cout << "Sorting vector 3..." << std::endl;
		while (!task_3.ready());
		std::cout << "Elapsed: " << task_3.result() << "ns" << std::endl;

		std::cout << "Sorting vector 4..." << std::endl;
		while (!task_4.ready());
		std::cout << "Elapsed: " << task_4.result() << "ns" << std::endl;

		stop_timer = std::chrono::high_resolution_clock::now();
		std::cout << "Elapsed on first four vectors: " << std::chrono::duration<double, std::nano>(stop_timer - start_timer).count() << std::endl;
	}

	// Sorting second four vectors (with concurrency)

	{
		stp::threadpool threadpool_2(thread_amount, stp::thread_state::running);
		stp::task<double> task_5(quicksort, std::ref(vec_5));
		stp::task<double> task_6(quicksort, std::ref(vec_6));
		stp::task<double> task_7(quicksort, std::ref(vec_7));
		stp::task<double> task_8(quicksort, std::ref(vec_8));

		threadpool_2.new_sync_task(task_5);
		threadpool_2.new_sync_task(task_6);
		threadpool_2.new_sync_task(task_7);
		threadpool_2.new_sync_task(task_8);

		while (threadpool_2.synced() != 4)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		std::cout << "Sorting second four vectors... " << std::endl;
		start_timer = std::chrono::high_resolution_clock::now();

		threadpool_2.run_synced();

		std::cout << "Sorting vector 5..." << std::endl;
		while (!task_5.ready());
		std::cout << "Elapsed: " << task_5.result() << "ns" << std::endl;

		std::cout << "Sorting vector 6..." << std::endl;
		while (!task_6.ready());
		std::cout << "Elapsed: " << task_6.result() << "ns" << std::endl;

		std::cout << "Sorting vector 7..." << std::endl;
		while (!task_7.ready());
		std::cout << "Elapsed: " << task_7.result() << "ns" << std::endl;

		std::cout << "Sorting vector 8..." << std::endl;
		while (!task_8.ready());
		std::cout << "Elapsed: " << task_8.result() << "ns" << std::endl;

		stop_timer = std::chrono::high_resolution_clock::now();
		std::cout << "Elapsed on second four vectors: " << std::chrono::duration<double, std::nano>(stop_timer - start_timer).count() << std::endl;
	}

	return 0;
}
