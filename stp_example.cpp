#include "stp.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <random>

#define OUTPUT_TO_FILE_ 0
#define TEST_ITERATIONS_ 100

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
	std::setvbuf(stdout, nullptr, _IOFBF, BUFSIZ);

#if (OUTPUT_TO_FILE_ == 1)
	std::fstream fout("stp_tests.txt", std::ios::out | std::ios::trunc);
	std::streambuf * cout_buffer = std::cout.rdbuf(fout.rdbuf());
#endif
	
	std::cout << std::scientific;

	// Generating vectors	

	std::vector<int> vec_00(1000000);
	std::vector<int> vec_01(1000000);
	std::vector<int> vec_02(1000000);
	std::vector<int> vec_03(1000000);
	std::vector<int> vec_04(1000000);
	std::vector<int> vec_05(1000000);
	std::vector<int> vec_06(1000000);
	std::vector<int> vec_07(1000000);
	std::vector<int> vec_08(1000000);
	std::vector<int> vec_09(1000000);
	std::vector<int> vec_10(1000000);
	std::vector<int> vec_11(1000000);
	std::vector<int> vec_12(1000000);
	std::vector<int> vec_13(1000000);
	std::vector<int> vec_14(1000000);
	std::vector<int> vec_15(1000000);
	std::vector<int> vec_16(1000000);

	std::cout << "Generating 16 vectors...\n\n";

	stp::task<double> task_00(generator, std::ref(vec_00));
	stp::task<double> task_01(generator, std::ref(vec_01));
	stp::task<double> task_02(generator, std::ref(vec_02));
	stp::task<double> task_03(generator, std::ref(vec_03));
	stp::task<double> task_04(generator, std::ref(vec_04));
	stp::task<double> task_05(generator, std::ref(vec_05));
	stp::task<double> task_06(generator, std::ref(vec_06));
	stp::task<double> task_07(generator, std::ref(vec_07));
	stp::task<double> task_08(generator, std::ref(vec_08));
	stp::task<double> task_09(generator, std::ref(vec_09));
	stp::task<double> task_10(generator, std::ref(vec_10));
	stp::task<double> task_11(generator, std::ref(vec_11));
	stp::task<double> task_12(generator, std::ref(vec_12));
	stp::task<double> task_13(generator, std::ref(vec_13));
	stp::task<double> task_14(generator, std::ref(vec_14));
	stp::task<double> task_15(generator, std::ref(vec_15));
	stp::task<double> task_16(generator, std::ref(vec_16));

	task_00();
	task_01();
	task_02();
	task_03();
	task_04();
	task_05();
	task_06();
	task_07();
	task_08();
	task_09();
	task_10();
	task_11();
	task_12();
	task_13();
	task_14();
	task_15();
	task_16();

	std::cout << "Elapsed on vector 01: " << task_01.result() << "ns\n";
	std::cout << "Elapsed on vector 02: " << task_02.result() << "ns\n";
	std::cout << "Elapsed on vector 03: " << task_03.result() << "ns\n";
	std::cout << "Elapsed on vector 04: " << task_04.result() << "ns\n";
	std::cout << "Elapsed on vector 05: " << task_05.result() << "ns\n";
	std::cout << "Elapsed on vector 06: " << task_06.result() << "ns\n";
	std::cout << "Elapsed on vector 07: " << task_07.result() << "ns\n";
	std::cout << "Elapsed on vector 08: " << task_08.result() << "ns\n";
	std::cout << "Elapsed on vector 09: " << task_09.result() << "ns\n";
	std::cout << "Elapsed on vector 10: " << task_10.result() << "ns\n";
	std::cout << "Elapsed on vector 11: " << task_11.result() << "ns\n";
	std::cout << "Elapsed on vector 12: " << task_12.result() << "ns\n";
	std::cout << "Elapsed on vector 13: " << task_13.result() << "ns\n";
	std::cout << "Elapsed on vector 14: " << task_14.result() << "ns\n";
	std::cout << "Elapsed on vector 15: " << task_15.result() << "ns\n";
	std::cout << "Elapsed on vector 16: " << task_16.result() << "ns\n";

	for (size_t n = 1; n <= TEST_ITERATIONS_; ++n)
	{
		std::cout << "\nn = " << n << "\n\n";
		std::cout << "Sorting 16 vectors...\n";

		stp::threadpool threadpool(1);	// Default: std::thread::hardware_concurrency()
										// Default: stp::threadpool_state::waiting
										// Default: true

		stp::task<double> task_00(sorter, std::ref(vec_00));
		stp::task<double> task_01(sorter, std::ref(vec_01));
		stp::task<double> task_02(sorter, std::ref(vec_02));
		stp::task<double> task_03(sorter, std::ref(vec_03));
		stp::task<double> task_04(sorter, std::ref(vec_04));
		stp::task<double> task_05(sorter, std::ref(vec_05));
		stp::task<double> task_06(sorter, std::ref(vec_06));
		stp::task<double> task_07(sorter, std::ref(vec_07));
		stp::task<double> task_08(sorter, std::ref(vec_08));
		stp::task<double> task_09(sorter, std::ref(vec_09));
		stp::task<double> task_10(sorter, std::ref(vec_10));
		stp::task<double> task_11(sorter, std::ref(vec_11));
		stp::task<double> task_12(sorter, std::ref(vec_12));
		stp::task<double> task_13(sorter, std::ref(vec_13));
		stp::task<double> task_14(sorter, std::ref(vec_14));
		stp::task<double> task_15(sorter, std::ref(vec_15));
		stp::task<double> task_16(sorter, std::ref(vec_16));

		task_00();
		auto sleep_time = std::chrono::nanoseconds(static_cast<uint32_t>(task_00.result() * 2 / std::thread::hardware_concurrency()));

		// ===== First test =====

		std::cout << "\n\tThreadpool size: " << threadpool.size() << "\n";
		std::cout << "\tThreadpool state: " << (threadpool.state() == stp::threadpool_state::running ? 
											  "running\n" : 
											  (threadpool.state() == stp::threadpool_state::waiting ? 
											  "waiting\n" : 
											  "terminating\n"));
		std::cout << "\tNotify threads: " << (threadpool.notify() ? "true\n" : "false\n");

		std::cout << "\tFirst test - Single thread\n";

		threadpool.new_task(task_01); // Default: stp::task_priority::normal
		threadpool.new_task(task_02, stp::task_priority::normal);
		threadpool.new_task(task_03);
		threadpool.new_task(task_04);

		while (threadpool.waiting() != 1);

		std::cout << "\t\tNumber of waiting threads: " << threadpool.waiting() << "\n";

		threadpool.run();

		start_timer = std::chrono::high_resolution_clock::now();

		// We can wait for stp::task::result_ready() to return "true", by which time stp::task::result() shall return the value of the task
		while (!task_01.result_ready() && !task_02.result_ready() && !task_03.result_ready() && !task_04.result_ready())
		{
			std::cout << "\t\tNumber of running threads: " << threadpool.running() << "\n";
			std::this_thread::sleep_for(sleep_time);
		}

		if (task_01.result_ready()) // It should always display this message
		{
			std::cout << "\t\tOK - Task 01 done before other tasks\n";
		}
		else
		{
			std::cout << "\t\tError - Some task done before task 01\n";
		}

		while (!task_01.result_ready() || !task_02.result_ready() || !task_03.result_ready() || !task_04.result_ready())
		{
			std::cout << "\t\tNumber of running threads: " << threadpool.running() << "\n";
			std::this_thread::sleep_for(sleep_time);
		}

		stop_timer = std::chrono::high_resolution_clock::now();

		std::cout << "\tElapsed on first test: " << std::chrono::duration<double, std::nano>(stop_timer - start_timer).count() << "ns\n";

		threadpool.new_threads(3);

		// ===== Second test =====

		while (threadpool.size() != 4);

		std::cout << "\n\tThreadpool size: " << threadpool.size() << "\n";
		std::cout << "\tThreadpool state: " << (threadpool.state() == stp::threadpool_state::running ?
											  "running\n" :
											  (threadpool.state() == stp::threadpool_state::waiting ?
											  "waiting\n" :
											  "terminating\n"));
		std::cout << "\tNotify threads: " << (threadpool.notify() ? "true\n" : "false\n");

		std::cout << "\tSecond test - Multiple synchronized threads\n";

		threadpool.new_sync_task(task_05);
		threadpool.new_sync_task(task_06);
		threadpool.new_sync_task(task_07);
		threadpool.new_sync_task(task_08);

		while (threadpool.sync_waiting() != 4);

		std::cout << "\t\tNumber of synchronized waiting threads: " << threadpool.sync_waiting() << "\n";

		threadpool.run_sync_tasks();

		start_timer = std::chrono::high_resolution_clock::now();

		while (!task_05.result_ready() || !task_06.result_ready() || !task_07.result_ready() || !task_08.result_ready())
		{
			std::cout << "\t\tNumber of synchronized running threads: " << threadpool.sync_running() << "\n";
			std::this_thread::sleep_for(sleep_time);
		}

		stop_timer = std::chrono::high_resolution_clock::now();

		std::cout << "\tElapsed on second test: " << std::chrono::duration<double, std::nano>(stop_timer - start_timer).count() << "ns\n";

		threadpool.notify_new_tasks(false);

		threadpool.delete_threads(2);

		// ===== Third test =====

		while (threadpool.size() != 2);

		std::cout << "\n\tThreadpool size: " << threadpool.size() << "\n";
		std::cout << "\tThreadpool state: " << (threadpool.state() == stp::threadpool_state::running ?
											  "running\n" :
											  (threadpool.state() == stp::threadpool_state::waiting ?
											  "waiting\n" :
											  "terminating\n"));
		std::cout << "\tNotify threads: " << (threadpool.notify() ? "true\n" : "false\n");

		std::cout << "\tThird test - Overworked threads\n";

		threadpool.new_task(task_09, stp::task_priority::very_low);
		threadpool.new_task(task_10, stp::task_priority::high);
		threadpool.new_task(task_11, stp::task_priority::low);
		threadpool.new_task(task_12, stp::task_priority::very_high);

		while (threadpool.waiting() != 2);

		std::cout << "\t\tNumber of waiting threads: " << threadpool.waiting() << "\n";

		threadpool.notify_new_tasks();

		start_timer = std::chrono::high_resolution_clock::now();

		while (threadpool.running() != 2);

		threadpool.delete_tasks();

		while (!task_09.result_ready() && !task_10.result_ready() && !task_11.result_ready() && !task_12.result_ready())
		{
			std::cout << "\t\tNumber of running threads: " << threadpool.running() << "\n";
			std::this_thread::sleep_for(sleep_time);
		}

		if (task_10.result_ready() || task_12.result_ready()) // It should always display this message
		{
			std::cout << "\t\tOK - Tasks 10 or 12 done before other tasks\n";
		}
		else
		{
			std::cout << "\t\tError - Some tasks done before tasks 06 and 08\n";
		}

		while (threadpool.waiting() != 2);

		std::cout << "\t\tNumber of waiting threads: " << threadpool.waiting() << "\n";

		if (!task_09.result_ready())
		{
			threadpool.new_task(task_09, stp::task_priority::very_low);
		}
		if (!task_10.result_ready())
		{
			threadpool.new_task(task_10, stp::task_priority::high);
		}
		if (!task_11.result_ready())
		{
			threadpool.new_task(task_11, stp::task_priority::low);
		}
		if (!task_12.result_ready())
		{
			threadpool.new_task(task_12, stp::task_priority::very_high);
		}

		threadpool.notify_new_tasks();

		while (!task_09.result_ready() || !task_10.result_ready() || !task_11.result_ready() || !task_12.result_ready())
		{
			std::cout << "\t\tNumber of running threads: " << threadpool.running() << "\n";
			std::this_thread::sleep_for(sleep_time);
		}

		stop_timer = std::chrono::high_resolution_clock::now();

		std::cout << "\tElapsed on third test: " << std::chrono::duration<double, std::nano>(stop_timer - start_timer).count() << "ns\n";

		threadpool.wait();

		threadpool.new_threads(6);

		// ===== Fourth test =====

		while (threadpool.size() != 8);

		std::cout << "\n\tThreadpool size: " << threadpool.size() << "\n";
		std::cout << "\tThreadpool state: " << (threadpool.state() == stp::threadpool_state::running ?
											  "running\n" :
											  (threadpool.state() == stp::threadpool_state::waiting ?
											  "waiting\n" :
											  "terminating\n"));
		std::cout << "\tNotify threads: " << (threadpool.notify() ? "true\n" : "false\n");

		std::cout << "\tFourth test - Underworked threads\n";

		bool task_13_check = false, task_14_check = false, task_15_check = false, task_16_check = false;

		threadpool.new_task(task_13);
		threadpool.new_task(task_14, stp::task_priority::minimum);
		threadpool.new_task(task_15, stp::task_priority::normal);
		threadpool.new_task(task_16, stp::task_priority::maximum);

		threadpool.notify_new_tasks();

		threadpool.run();

		start_timer = std::chrono::high_resolution_clock::now();

		// Task priority is meaningless if there are more threads available than tasks
		for (size_t n = 0; n < 4; ++n)
		{
			while (!task_13.result_ready() && !task_14.result_ready() && !task_15.result_ready() && !task_16.result_ready());

			if (task_13.result_ready() && !task_13_check)
			{
				std::cout << "\t\tTask 13 done\n";
				task_13_check = true;
			}
			if (task_14.result_ready() && !task_14_check)
			{
				std::cout << "\t\tTask 14 done\n";
				task_14_check = true;
			}
			if (task_15.result_ready() && !task_15_check)
			{
				std::cout << "\t\tTask 15 done\n";
				task_15_check = true;
			}
			if (task_16.result_ready() && !task_16_check)
			{
				std::cout << "\t\tTask 16 done\n";
				task_16_check = true;
			}
		}

		stop_timer = std::chrono::high_resolution_clock::now();

		std::cout << "\tElapsed on fourth test: " << std::chrono::duration<double, std::nano>(stop_timer - start_timer).count() << "ns\n\n";

		threadpool.terminate();

		while (threadpool.size() != 0);

		std::cout << "Elapsed on vector 01: " << task_01.result() << "ns\n";
		std::cout << "Elapsed on vector 02: " << task_02.result() << "ns\n";
		std::cout << "Elapsed on vector 03: " << task_03.result() << "ns\n";
		std::cout << "Elapsed on vector 04: " << task_04.result() << "ns\n";
		std::cout << "Elapsed on vector 05: " << task_05.result() << "ns\n";
		std::cout << "Elapsed on vector 06: " << task_06.result() << "ns\n";
		std::cout << "Elapsed on vector 07: " << task_07.result() << "ns\n";
		std::cout << "Elapsed on vector 08: " << task_08.result() << "ns\n";
		std::cout << "Elapsed on vector 09: " << task_09.result() << "ns\n";
		std::cout << "Elapsed on vector 10: " << task_10.result() << "ns\n";
		std::cout << "Elapsed on vector 11: " << task_11.result() << "ns\n";
		std::cout << "Elapsed on vector 12: " << task_12.result() << "ns\n";
		std::cout << "Elapsed on vector 13: " << task_13.result() << "ns\n";
		std::cout << "Elapsed on vector 14: " << task_14.result() << "ns\n";
		std::cout << "Elapsed on vector 15: " << task_15.result() << "ns\n";
		std::cout << "Elapsed on vector 16: " << task_16.result() << "ns\n";
	}

	std::cout << "\nTesting finished\n";

#if OUTPUT_TO_FILE_ == 1
	fout.close();
	std::cout.rdbuf(cout_buffer);
#endif

	std::cout << "\nPress enter to exit\n";
	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

	return 0;
}
