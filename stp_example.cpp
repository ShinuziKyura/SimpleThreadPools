#include "stp.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>

std::mt19937 generate(std::random_device());
std::uniform_int_distribution<int> random_list(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());

std::chrono::duration<double, std::nano> generator(std::vector<int> * vec)
{
	auto start_timer = std::chrono::high_resolution_clock::now();
	for (auto & i : *vec)
		i = random_list(generate);
	auto stop_timer = std::chrono::high_resolution_clock::now();
	return std::chrono::duration<double, std::nano>(stop_timer - start_timer);
}

std::chrono::duration<double, std::nano> quicksort(std::vector<int> * vec)
{
	auto start_timer = std::chrono::high_resolution_clock::now();
	std::sort((*vec).begin(), (*vec).end(), [] (int const & i1, int const & i2) -> bool
	{
		return i1 < i2;
	});
	auto stop_timer = std::chrono::high_resolution_clock::now();
	return std::chrono::duration<double, std::nano>(stop_timer - start_timer);
}

int main()
{
	// Generating

	std::cout << "Generating vector 1..." << std::endl;
	std::vector<int> vec1(1000000);
	auto elapsed1 = generator(&vec1);
	std::cout << "Elapsed: " << elapsed1.count() << "ns" << std::endl;

	std::cout << "Generating vector 2..." << std::endl;
	std::vector<int> vec2(1000000);
	auto elapsed2 = generator(&vec2);
	std::cout << "Elapsed: " << elapsed2.count() << "ns" << std::endl;

	std::cout << "Generating vector 3..." << std::endl;
	std::vector<int> vec3(1000000);
	auto elapsed3 = generator(&vec3);
	std::cout << "Elapsed: " << elapsed3.count() << "ns" << std::endl;

	std::cout << "Generating vector 4..." << std::endl;
	std::vector<int> vec4(1000000);
	auto elapsed4 = generator(&vec4);
	std::cout << "Elapsed: " << elapsed4.count() << "ns" << std::endl;

	std::cout << "Generating vector 5..." << std::endl;
	std::vector<int> vec5(1000000);
	auto elapsed5 = generator(&vec5);
	std::cout << "Elapsed: " << elapsed5.count() << "ns" << std::endl;

	std::cout << "Generating vector 6..." << std::endl;
	std::vector<int> vec6(1000000);
	auto elapsed6 = generator(&vec6);
	std::cout << "Elapsed: " << elapsed6.count() << "ns" << std::endl;

	std::cout << "Generating vector 7..." << std::endl;
	std::vector<int> vec7(1000000);
	auto elapsed7 = generator(&vec7);
	std::cout << "Elapsed: " << elapsed7.count() << "ns" << std::endl;

	std::cout << "Generating vector 8..." << std::endl;
	std::vector<int> vec8(1000000);
	auto elapsed8 = generator(&vec8);
	std::cout << "Elapsed: " << elapsed8.count() << "ns" << std::endl;

	// Sorting first 4
	std::cout << "Sorting first four vectors... " << std::endl;
	auto start_timer = std::chrono::high_resolution_clock::now();

	std::cout << "Sorting vector 1..." << std::endl;
	elapsed1 = quicksort(&vec1);
	std::cout << "Elapsed: " << elapsed1.count() << "ns" << std::endl;

	std::cout << "Sorting vector 2..." << std::endl;
	elapsed2 = quicksort(&vec2);
	std::cout << "Elapsed: " << elapsed2.count() << "ns" << std::endl;

	std::cout << "Sorting vector 3..." << std::endl;
	elapsed3 = quicksort(&vec3);
	std::cout << "Elapsed: " << elapsed3.count() << "ns" << std::endl;

	std::cout << "Sorting vector 4..." << std::endl;
	elapsed4 = quicksort(&vec4);
	std::cout << "Elapsed: " << elapsed4.count() << "ns" << std::endl;

	auto stop_timer = std::chrono::high_resolution_clock::now();
	std::cout << "Elapsed on first four vectors: " << std::chrono::duration<double, std::nano>(stop_timer - start_timer).count() << std::endl;

	// Sorting second 4...

	stp::threadpool threadpool(0);
	stp::task <std::chrono::duration<double, std::nano>> task1(quicksort, &vec5);
	stp::task <std::chrono::duration<double, std::nano>> task2(quicksort, &vec6);
	stp::task <std::chrono::duration<double, std::nano>> task3(quicksort, &vec7);
	stp::task <std::chrono::duration<double, std::nano>> task4(quicksort, &vec8);
	threadpool.new_task(task1);
	threadpool.new_task(task2);
	threadpool.new_task(task3);
	threadpool.new_task(task4);
	
	std::cout << "Sorting second four vectors... " << std::endl;
	start_timer = std::chrono::high_resolution_clock::now();

	threadpool.run();

	std::cout << "Sorting vector 5..." << std::endl;
	while (!task1.is_ready());
	elapsed5 = task1.task_result();
	std::cout << "Elapsed: " << elapsed5.count() << "ns" << std::endl;

	std::cout << "Sorting vector 6..." << std::endl;
	while (!task2.is_ready());
	elapsed6 = task2.task_result();
	std::cout << "Elapsed: " << elapsed6.count() << "ns" << std::endl;

	std::cout << "Sorting vector 7..." << std::endl;
	while (!task3.is_ready());
	elapsed7 = task3.task_result();
	std::cout << "Elapsed: " << elapsed7.count() << "ns" << std::endl;

	std::cout << "Sorting vector 8..." << std::endl;
	while (!task4.is_ready());
	elapsed8 = task4.task_result();
	std::cout << "Elapsed: " << elapsed8.count() << "ns" << std::endl;

	stop_timer = std::chrono::high_resolution_clock::now();
	std::cout << "Elapsed on second four vectors: " << std::chrono::duration<double, std::nano>(stop_timer - start_timer).count() << std::endl;

	threadpool.stop();
	while (threadpool.is_running());
	return 0;
}