#include "stp.hpp"

#include <iostream>
#include <algorithm>
#include <random>

constexpr size_t single_thread = 1;
constexpr size_t multiple_threads = 4; // 0 == std::thread::hardware_concurrency()

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

double sorter(std::vector<int> & vec)
{
	start_timer = std::chrono::high_resolution_clock::now();
	std::sort(vec.begin(), vec.end());
	stop_timer = std::chrono::high_resolution_clock::now();
	return std::chrono::duration<double, std::nano>(stop_timer - start_timer).count();
}

int main()
{
	std::cout << std::scientific;
	std::cout.precision(5);	

	// Generating vectors	

	std::vector<int> vec_1(1000000);
	std::vector<int> vec_2(1000000);
	std::vector<int> vec_3(1000000);
	std::vector<int> vec_4(1000000);
	std::vector<int> vec_5(1000000);
	std::vector<int> vec_6(1000000);
	std::vector<int> vec_7(1000000);
	std::vector<int> vec_8(1000000);

	stp::task<double> task_1(generator, std::ref(vec_1));
	stp::task<double> task_2(generator, std::ref(vec_2));
	stp::task<double> task_3(generator, std::ref(vec_3));
	stp::task<double> task_4(generator, std::ref(vec_4));
	stp::task<double> task_5(generator, std::ref(vec_5));
	stp::task<double> task_6(generator, std::ref(vec_6));
	stp::task<double> task_7(generator, std::ref(vec_7));
	stp::task<double> task_8(generator, std::ref(vec_8));

	std::cout << "Generating eight vectors..." << std::endl;

	task_1();
	task_2();
	task_3();
	task_4();
	task_5();
	task_6();
	task_7();
	task_8();

	std::cout << "Elapsed on vector 1: " << task_1.result() << "ns" << std::endl;
	std::cout << "Elapsed on vector 2: " << task_2.result() << "ns" << std::endl;
	std::cout << "Elapsed on vector 3: " << task_3.result() << "ns" << std::endl;
	std::cout << "Elapsed on vector 4: " << task_4.result() << "ns" << std::endl;
	std::cout << "Elapsed on vector 5: " << task_5.result() << "ns" << std::endl;
	std::cout << "Elapsed on vector 6: " << task_6.result() << "ns" << std::endl;
	std::cout << "Elapsed on vector 7: " << task_7.result() << "ns" << std::endl;
	std::cout << "Elapsed on vector 8: " << task_8.result() << "ns" << std::endl;

	// Sorting first four vectors (without concurrency)

	{
		stp::threadpool threadpool_1(single_thread);

		stp::task<double> task_1(sorter, std::ref(vec_1));
		stp::task<double> task_2(sorter, std::ref(vec_2));
		stp::task<double> task_3(sorter, std::ref(vec_3));
		stp::task<double> task_4(sorter, std::ref(vec_4));

		threadpool_1.new_task(task_1);
		threadpool_1.new_task(task_2);
		threadpool_1.new_task(task_3);
		threadpool_1.new_task(task_4, stp::task_priority::normal);

		std::cout << "Sorting first four vectors...\n[Without concurrency]\n";

		start_timer = std::chrono::high_resolution_clock::now();

		threadpool_1.run();

		while (!task_1.ready());
		while (!task_2.ready());
		while (!task_3.ready());
		while (!task_4.ready());

		stop_timer = std::chrono::high_resolution_clock::now();

		std::cout << "Elapsed on vector 1: " << task_1.result() << "ns" << std::endl;
		std::cout << "Elapsed on vector 2: " << task_2.result() << "ns" << std::endl;
		std::cout << "Elapsed on vector 3: " << task_3.result() << "ns" << std::endl;
		std::cout << "Elapsed on vector 4: " << task_4.result() << "ns" << std::endl;

		std::cout << "Elapsed on first four vectors: " << std::chrono::duration<double, std::nano>(stop_timer - start_timer).count() << "ns" << std::endl;
	}

	// Sorting second four vectors (with concurrency)

	{
		stp::threadpool threadpool_2(multiple_threads, stp::threadpool_state::running);

		stp::task<double> task_5(sorter, std::ref(vec_5));
		stp::task<double> task_6(sorter, std::ref(vec_6));
		stp::task<double> task_7(sorter, std::ref(vec_7));
		stp::task<double> task_8(sorter, std::ref(vec_8));

		threadpool_2.new_sync_task(task_5);
		threadpool_2.new_sync_task(task_6);
		threadpool_2.new_sync_task(task_7);
		threadpool_2.new_sync_task(task_8, stp::task_priority::normal);

		while (threadpool_2.sync_ready() != 4)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		std::cout << "Sorting second four vectors...\n[With concurrency]\n";

		start_timer = std::chrono::high_resolution_clock::now();

		threadpool_2.sync_run();

		while (!task_8.ready() || !task_7.ready() || !task_6.ready() || !task_5.ready());

		stop_timer = std::chrono::high_resolution_clock::now();

		std::cout << "Elapsed on vector 5: " << task_5.result() << "ns" << std::endl;
		std::cout << "Elapsed on vector 6: " << task_6.result() << "ns" << std::endl;
		std::cout << "Elapsed on vector 7: " << task_7.result() << "ns" << std::endl;
		std::cout << "Elapsed on vector 8: " << task_8.result() << "ns" << std::endl;

		std::cout << "Elapsed on second four vectors: " << std::chrono::duration<double, std::nano>(stop_timer - start_timer).count() << "ns" << std::endl;
	}

	return 0;
}
