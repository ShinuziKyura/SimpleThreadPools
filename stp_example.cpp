#include "stp.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>

std::random_device seed;
std::mt19937 generate(seed());
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
	// Generating vectors

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
/*
	std::fstream unsorted("vectors_unsorted.txt", std::ios::out | std::ios::trunc);
	std::fstream sorted("vectors_sorted.txt", std::ios::out | std::ios::trunc);

	unsorted << "Vector 1:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		unsorted << vec1[i] << std::endl;
	unsorted << "Elapsed: " << elapsed1.count() << "ns" << std::endl << std::endl;
	unsorted << "Vector 2:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		unsorted << vec2[i] << std::endl;
	unsorted << "Elapsed: " << elapsed2.count() << "ns" << std::endl << std::endl;
	unsorted << "Vector 3:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		unsorted << vec3[i] << std::endl;
	unsorted << "Elapsed: " << elapsed3.count() << "ns" << std::endl << std::endl;
	unsorted << "Vector 4:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		unsorted << vec4[i] << std::endl;
	unsorted << "Elapsed: " << elapsed4.count() << "ns" << std::endl << std::endl;
	unsorted << "Vector 5:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		unsorted << vec5[i] << std::endl;
	unsorted << "Elapsed: " << elapsed5.count() << "ns" << std::endl << std::endl;
	unsorted << "Vector 6:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		unsorted << vec6[i] << std::endl;
	unsorted << "Elapsed: " << elapsed6.count() << "ns" << std::endl << std::endl;
	unsorted << "Vector 7:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		unsorted << vec7[i] << std::endl;
	unsorted << "Elapsed: " << elapsed7.count() << "ns" << std::endl << std::endl;
	unsorted << "Vector 8:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		unsorted << vec8[i] << std::endl;
	unsorted << "Elapsed: " << elapsed8.count() << "ns" << std::endl << std::endl;
*/
	// Sorting first four vectors (without concurrency)

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

	// Sorting second four vectors (with concurrency)

	stp::threadpool threadpool(1);
	stp::task <std::chrono::duration<double, std::nano>> task_1(quicksort, &vec5);
	stp::task <std::chrono::duration<double, std::nano>> task_2(quicksort, &vec6);
	stp::task <std::chrono::duration<double, std::nano>> task_3(quicksort, &vec7);
	stp::task <std::chrono::duration<double, std::nano>> task_4(quicksort, &vec8);

//	threadpool.run(); // Current tests
	threadpool.new_sync_task(task_1);
	threadpool.new_sync_task(task_2);
	threadpool.new_sync_task(task_3);
	threadpool.new_sync_task(task_4);
	
	threadpool.stop();
	threadpool.run_sync();
	threadpool.run();

	std::cout << "Sorting second four vectors... " << std::endl;
	start_timer = std::chrono::high_resolution_clock::now();

	threadpool.run_sync();

	std::cout << "Sorting vector 5..." << std::endl;
	while (!task_1.is_done());
	elapsed5 = task_1.result();
	std::cout << "Elapsed: " << elapsed5.count() << "ns" << std::endl;

	std::cout << "Sorting vector 6..." << std::endl;
	while (!task_2.is_done());
	elapsed6 = task_2.result();
	std::cout << "Elapsed: " << elapsed6.count() << "ns" << std::endl;

	std::cout << "Sorting vector 7..." << std::endl;
	while (!task_3.is_done());
	elapsed7 = task_3.result();
	std::cout << "Elapsed: " << elapsed7.count() << "ns" << std::endl;

	std::cout << "Sorting vector 8..." << std::endl;
	while (!task_4.is_done());
	elapsed8 = task_4.result();
	std::cout << "Elapsed: " << elapsed8.count() << "ns" << std::endl;

	stop_timer = std::chrono::high_resolution_clock::now();
	std::cout << "Elapsed on second four vectors: " << std::chrono::duration<double, std::nano>(stop_timer - start_timer).count() << std::endl;
/*
	sorted << "Vector 1:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		sorted << vec1[i] << std::endl;
	sorted << "Elapsed: " << elapsed1.count() << "ns" << std::endl << std::endl;
	sorted << "Vector 2:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		sorted << vec2[i] << std::endl;
	sorted << "Elapsed: " << elapsed2.count() << "ns" << std::endl << std::endl;
	sorted << "Vector 3:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		sorted << vec3[i] << std::endl;
	sorted << "Elapsed: " << elapsed3.count() << "ns" << std::endl << std::endl;
	sorted << "Vector 4:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		sorted << vec4[i] << std::endl;
	sorted << "Elapsed: " << elapsed4.count() << "ns" << std::endl << std::endl;
	sorted << "Vector 5:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		sorted << vec5[i] << std::endl;
	sorted << "Elapsed: " << elapsed5.count() << "ns" << std::endl << std::endl;
	sorted << "Vector 6:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		sorted << vec6[i] << std::endl;
	sorted << "Elapsed: " << elapsed6.count() << "ns" << std::endl << std::endl;
	sorted << "Vector 7:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		sorted << vec7[i] << std::endl;
	sorted << "Elapsed: " << elapsed7.count() << "ns" << std::endl << std::endl;
	sorted << "Vector 8:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		sorted << vec8[i] << std::endl;
	sorted << "Elapsed: " << elapsed8.count() << "ns" << std::endl << std::endl;

	unsorted.close();
	sorted.close();
*/
	threadpool.finalize();
	while (!threadpool.is_waiting());
	return 0;
}