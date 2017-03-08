#include "stp.h"

//#include <fstream>
#include <iostream>
#include <algorithm>
#include <random>

std::random_device seed;
std::mt19937 generate(seed());
std::uniform_int_distribution<int> random_list(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());

std::chrono::duration<double, std::nano> generator(std::vector<int> * vec)
{
	auto start_timer = std::chrono::high_resolution_clock::now();
	for (auto & i : *vec)
	{
		i = random_list(generate);
	}
	auto stop_timer = std::chrono::high_resolution_clock::now();
	return std::chrono::duration<double, std::nano>(stop_timer - start_timer);
}

std::chrono::duration<double, std::nano> quicksort(std::vector<int> * vec)
{
	auto start_timer = std::chrono::high_resolution_clock::now();
	std::sort(vec->begin(), vec->end(), [] (int const & i1, int const & i2) -> bool
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
	std::vector<int> vec_1(1000000);
	auto elapsed1 = generator(&vec_1);
	std::cout << "Elapsed: " << elapsed1.count() << "ns" << std::endl;

	std::cout << "Generating vector 2..." << std::endl;
	std::vector<int> vec_2(1000000);
	auto elapsed2 = generator(&vec_2);
	std::cout << "Elapsed: " << elapsed2.count() << "ns" << std::endl;

	std::cout << "Generating vector 3..." << std::endl;
	std::vector<int> vec_3(1000000);
	auto elapsed3 = generator(&vec_3);
	std::cout << "Elapsed: " << elapsed3.count() << "ns" << std::endl;

	std::cout << "Generating vector 4..." << std::endl;
	std::vector<int> vec_4(1000000);
	auto elapsed4 = generator(&vec_4);
	std::cout << "Elapsed: " << elapsed4.count() << "ns" << std::endl;

	std::cout << "Generating vector 5..." << std::endl;
	std::vector<int> vec_5(1000000);
	auto elapsed5 = generator(&vec_5);
	std::cout << "Elapsed: " << elapsed5.count() << "ns" << std::endl;

	std::cout << "Generating vector 6..." << std::endl;
	std::vector<int> vec_6(1000000);
	auto elapsed6 = generator(&vec_6);
	std::cout << "Elapsed: " << elapsed6.count() << "ns" << std::endl;

	std::cout << "Generating vector 7..." << std::endl;
	std::vector<int> vec_7(1000000);
	auto elapsed7 = generator(&vec_7);
	std::cout << "Elapsed: " << elapsed7.count() << "ns" << std::endl;

	std::cout << "Generating vector 8..." << std::endl;
	std::vector<int> vec_8(1000000);
	auto elapsed8 = generator(&vec_8);
	std::cout << "Elapsed: " << elapsed8.count() << "ns" << std::endl;
/*
	std::fstream unsorted("vectors_unsorted.txt", std::ios::out | std::ios::trunc);
	std::fstream sorted("vectors_sorted.txt", std::ios::out | std::ios::trunc);

	unsorted << "Vector 1:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		unsorted << vec_1[i] << std::endl;
	unsorted << "Elapsed: " << elapsed1.count() << "ns" << std::endl << std::endl;
	unsorted << "Vector 2:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		unsorted << vec_2[i] << std::endl;
	unsorted << "Elapsed: " << elapsed2.count() << "ns" << std::endl << std::endl;
	unsorted << "Vector 3:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		unsorted << vec_3[i] << std::endl;
	unsorted << "Elapsed: " << elapsed3.count() << "ns" << std::endl << std::endl;
	unsorted << "Vector 4:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		unsorted << vec_4[i] << std::endl;
	unsorted << "Elapsed: " << elapsed4.count() << "ns" << std::endl << std::endl;
	unsorted << "Vector 5:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		unsorted << vec_5[i] << std::endl;
	unsorted << "Elapsed: " << elapsed5.count() << "ns" << std::endl << std::endl;
	unsorted << "Vector 6:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		unsorted << vec_6[i] << std::endl;
	unsorted << "Elapsed: " << elapsed6.count() << "ns" << std::endl << std::endl;
	unsorted << "Vector 7:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		unsorted << vec_7[i] << std::endl;
	unsorted << "Elapsed: " << elapsed7.count() << "ns" << std::endl << std::endl;
	unsorted << "Vector 8:" << std::endl;
	for (int i = 0; i < 1000000; ++i)
		unsorted << vec_8[i] << std::endl;
	unsorted << "Elapsed: " << elapsed8.count() << "ns" << std::endl << std::endl;
//*/
	// Sorting first four vectors (without concurrency)

	std::cout << "Sorting first four vectors... " << std::endl;
	auto start_timer = std::chrono::high_resolution_clock::now();

	std::cout << "Sorting vector 1..." << std::endl;
	elapsed1 = quicksort(&vec_1);
	std::cout << "Elapsed: " << elapsed1.count() << "ns" << std::endl;

	std::cout << "Sorting vector 2..." << std::endl;
	elapsed2 = quicksort(&vec_2);
	std::cout << "Elapsed: " << elapsed2.count() << "ns" << std::endl;

	std::cout << "Sorting vector 3..." << std::endl;
	elapsed3 = quicksort(&vec_3);
	std::cout << "Elapsed: " << elapsed3.count() << "ns" << std::endl;

	std::cout << "Sorting vector 4..." << std::endl;
	elapsed4 = quicksort(&vec_4);
	std::cout << "Elapsed: " << elapsed4.count() << "ns" << std::endl;

	auto stop_timer = std::chrono::high_resolution_clock::now();
	std::cout << "Elapsed on first four vectors: " << std::chrono::duration<double, std::nano>(stop_timer - start_timer).count() << std::endl;

	// Sorting second four vectors (with concurrency)

	stp::threadpool threadpool(0);
	stp::task <std::chrono::duration<double, std::nano>> task_1(quicksort, &vec_5);
	stp::task <std::chrono::duration<double, std::nano>> task_2(quicksort, &vec_6);
	stp::task <std::chrono::duration<double, std::nano>> task_3(quicksort, &vec_7);
	stp::task <std::chrono::duration<double, std::nano>> task_4(quicksort, &vec_8);
	
	threadpool.run();
	threadpool.new_sync_task(task_1);
	threadpool.new_sync_task(task_2);
	threadpool.new_sync_task(task_3);
	threadpool.new_sync_task(task_4);

	std::cout << "Sorting second four vectors... " << std::endl;
	start_timer = std::chrono::high_resolution_clock::now();

	threadpool.start_sync_task();

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
//*/
	while (threadpool.is_active());
	return 0;
}
