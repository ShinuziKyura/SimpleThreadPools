#include "stp.h"

#include <iostream>
#include <algorithm>
#include <random>

constexpr int thread_amount = 0;

std::random_device seed;
std::mt19937 generate(seed());
std::uniform_int_distribution<int> random_list(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());

std::chrono::duration<double, std::nano> generator(std::vector<int> & vec)
{
	std::chrono::time_point<std::chrono::high_resolution_clock> start_timer, stop_timer;
	start_timer = std::chrono::high_resolution_clock::now();
	for (auto & i : vec)
	{
		i = random_list(generate);
	}
	stop_timer = std::chrono::high_resolution_clock::now();
	return std::chrono::duration<double, std::nano>(stop_timer - start_timer);
}

std::chrono::duration<double, std::nano> quicksort(std::vector<int> & vec)
{
	std::chrono::time_point<std::chrono::high_resolution_clock> start_timer, stop_timer;
	start_timer = std::chrono::high_resolution_clock::now();
	std::sort(vec.begin(), vec.end(), [] (int const & i1, int const & i2) -> bool
	{
		return i1 < i2;
	});
	stop_timer = std::chrono::high_resolution_clock::now();
	return std::chrono::duration<double, std::nano>(stop_timer - start_timer);
}

int main()
{
	// Generating vectors

	std::cout << "Generating vector 1..." << std::endl;
	std::vector<int> vec_1(1000000);
	auto elapsed1 = generator(vec_1);
	std::cout << "Elapsed: " << elapsed1.count() << "ns" << std::endl;

	std::cout << "Generating vector 2..." << std::endl;
	std::vector<int> vec_2(1000000);
	auto elapsed2 = generator(vec_2);
	std::cout << "Elapsed: " << elapsed2.count() << "ns" << std::endl;

	std::cout << "Generating vector 3..." << std::endl;
	std::vector<int> vec_3(1000000);
	auto elapsed3 = generator(vec_3);
	std::cout << "Elapsed: " << elapsed3.count() << "ns" << std::endl;

	std::cout << "Generating vector 4..." << std::endl;
	std::vector<int> vec_4(1000000);
	auto elapsed4 = generator(vec_4);
	std::cout << "Elapsed: " << elapsed4.count() << "ns" << std::endl;

	std::cout << "Generating vector 5..." << std::endl;
	std::vector<int> vec_5(1000000);
	auto elapsed5 = generator(vec_5);
	std::cout << "Elapsed: " << elapsed5.count() << "ns" << std::endl;

	std::cout << "Generating vector 6..." << std::endl;
	std::vector<int> vec_6(1000000);
	auto elapsed6 = generator(vec_6);
	std::cout << "Elapsed: " << elapsed6.count() << "ns" << std::endl;

	std::cout << "Generating vector 7..." << std::endl;
	std::vector<int> vec_7(1000000);
	auto elapsed7 = generator(vec_7);
	std::cout << "Elapsed: " << elapsed7.count() << "ns" << std::endl;

	std::cout << "Generating vector 8..." << std::endl;
	std::vector<int> vec_8(1000000);
	auto elapsed8 = generator(vec_8);
	std::cout << "Elapsed: " << elapsed8.count() << "ns" << std::endl;

	// Sorting first four vectors (without concurrency)

	std::cout << "Sorting first four vectors... " << std::endl;
	auto start_timer = std::chrono::high_resolution_clock::now();

	std::cout << "Sorting vector 1..." << std::endl;
	elapsed1 = quicksort(vec_1);
	std::cout << "Elapsed: " << elapsed1.count() << "ns" << std::endl;

	std::cout << "Sorting vector 2..." << std::endl;
	elapsed2 = quicksort(vec_2);
	std::cout << "Elapsed: " << elapsed2.count() << "ns" << std::endl;

	std::cout << "Sorting vector 3..." << std::endl;
	elapsed3 = quicksort(vec_3);
	std::cout << "Elapsed: " << elapsed3.count() << "ns" << std::endl;

	std::cout << "Sorting vector 4..." << std::endl;
	elapsed4 = quicksort(vec_4);
	std::cout << "Elapsed: " << elapsed4.count() << "ns" << std::endl;

	auto stop_timer = std::chrono::high_resolution_clock::now();
	std::cout << "Elapsed on first four vectors: " << std::chrono::duration<double, std::nano>(stop_timer - start_timer).count() << std::endl;

	// Sorting second four vectors (with concurrency)

	stp::threadpool threadpool(thread_amount);
	stp::task<std::chrono::duration<double, std::nano>> task_1(quicksort, std::ref(vec_5));
	stp::task<std::chrono::duration<double, std::nano>> task_2(quicksort, std::ref(vec_6));
	stp::task<std::chrono::duration<double, std::nano>> task_3(quicksort, std::ref(vec_7));
	stp::task<std::chrono::duration<double, std::nano>> task_4(quicksort, std::ref(vec_8));
	
	threadpool.run();
	threadpool.new_sync_task(task_1);
	threadpool.new_sync_task(task_2);
	threadpool.new_sync_task(task_3);
	threadpool.new_sync_task(task_4);

	while (threadpool.synced() != 4) 
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	std::cout << "Sorting second four vectors... " << std::endl;
	start_timer = std::chrono::high_resolution_clock::now();

	threadpool.run_synced();

	std::cout << "Sorting vector 5..." << std::endl;
	while (!task_1.executed());
	elapsed5 = task_1.result();
	std::cout << "Elapsed: " << elapsed5.count() << "ns" << std::endl;

	std::cout << "Sorting vector 6..." << std::endl;
	while (!task_2.executed());
	elapsed6 = task_2.result();
	std::cout << "Elapsed: " << elapsed6.count() << "ns" << std::endl;

	std::cout << "Sorting vector 7..." << std::endl;
	while (!task_3.executed());
	elapsed7 = task_3.result();
	std::cout << "Elapsed: " << elapsed7.count() << "ns" << std::endl;

	std::cout << "Sorting vector 8..." << std::endl;
	while (!task_4.executed());
	elapsed8 = task_4.result();
	std::cout << "Elapsed: " << elapsed8.count() << "ns" << std::endl;

	stop_timer = std::chrono::high_resolution_clock::now();
	std::cout << "Elapsed on second four vectors: " << std::chrono::duration<double, std::nano>(stop_timer - start_timer).count() << std::endl;

	return 0;
}
