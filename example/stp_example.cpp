#include "stp.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <random>

#define OUTPUT_TO_FILE_ 0
#define TEST_ITERATIONS_ 1

std::random_device seed;
std::mt19937 generate(seed());
std::uniform_int_distribution<int> random_number(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());

thread_local std::chrono::time_point<std::chrono::high_resolution_clock> start_timer, stop_timer;

long double generator(std::vector<int> & vec)
{
	start_timer = std::chrono::high_resolution_clock::now();
	for (auto & v : vec) v = random_number(generate);
	stop_timer = std::chrono::high_resolution_clock::now();
	return std::chrono::duration<long double, std::nano>(stop_timer - start_timer).count();
}

long double sorter(std::vector<int> & vec)
{
	start_timer = std::chrono::high_resolution_clock::now();
	std::sort(vec.begin(), vec.end());
	stop_timer = std::chrono::high_resolution_clock::now();
	return std::chrono::duration<long double, std::nano>(stop_timer - start_timer).count();
}

int main()
{
	std::setvbuf(stdout, nullptr, _IOFBF, BUFSIZ);

	std::cout << std::scientific << "Testing started...\n" << std::endl;

#if (OUTPUT_TO_FILE_ == 1)
	std::fstream fout("stp_tests.txt", std::ios::out | std::ios::trunc);
	std::streambuf * cout_buffer = std::cout.rdbuf(fout.rdbuf());
#endif

	auto start_test = std::chrono::high_resolution_clock::now();

	for (size_t n = 1; n <= TEST_ITERATIONS_; ++n)
	{
		std::cout << "n = " << n << "\n";

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

		std::cout << "\nGenerating 16 vectors...\n\n";

		generator(vec_00);

		std::cout << "Elapsed on vector 01: " << generator(vec_01) << "ns\n";
		std::cout << "Elapsed on vector 02: " << generator(vec_02) << "ns\n";
		std::cout << "Elapsed on vector 03: " << generator(vec_03) << "ns\n";
		std::cout << "Elapsed on vector 04: " << generator(vec_04) << "ns\n";
		std::cout << "Elapsed on vector 05: " << generator(vec_05) << "ns\n";
		std::cout << "Elapsed on vector 06: " << generator(vec_06) << "ns\n";
		std::cout << "Elapsed on vector 07: " << generator(vec_07) << "ns\n";
		std::cout << "Elapsed on vector 08: " << generator(vec_08) << "ns\n";
		std::cout << "Elapsed on vector 09: " << generator(vec_09) << "ns\n";
		std::cout << "Elapsed on vector 10: " << generator(vec_10) << "ns\n";
		std::cout << "Elapsed on vector 11: " << generator(vec_11) << "ns\n";
		std::cout << "Elapsed on vector 12: " << generator(vec_12) << "ns\n";
		std::cout << "Elapsed on vector 13: " << generator(vec_13) << "ns\n";
		std::cout << "Elapsed on vector 14: " << generator(vec_14) << "ns\n";
		std::cout << "Elapsed on vector 15: " << generator(vec_15) << "ns\n";
		std::cout << "Elapsed on vector 16: " << generator(vec_16) << "ns\n";

		std::cout << "\nSorting 16 vectors...\n";

		stp::threadpool threadpool(1, true, stp::threadpool_state::stopping);	/* Default: std::thread::hardware_concurrency()
																				 * Default: true
																				 * Default: stp::threadpool_state::running
																				 */
		stp::task<long double> task_00(sorter, vec_00);
		stp::task<long double> task_01(sorter, vec_01);
		stp::task<long double> task_02(sorter, vec_02);
		stp::task<long double> task_03(sorter, vec_03);
		stp::task<long double> task_04(sorter, vec_04);
		stp::task<long double> task_05(sorter, vec_05);
		stp::task<long double> task_06(sorter, vec_06);
		stp::task<long double> task_07(sorter, vec_07);
		stp::task<long double> task_08(sorter, vec_08);
		stp::task<long double> task_09(sorter, vec_09);
		stp::task<long double> task_10(sorter, vec_10);
		stp::task<long double> task_11(sorter, vec_11);
		stp::task<long double> task_12(sorter, vec_12);
		stp::task<long double> task_13(sorter, vec_13);
		stp::task<long double> task_14(sorter, vec_14);
		stp::task<long double> task_15(sorter, vec_15);
		stp::task<long double> task_16(sorter, vec_16);

		task_00();
		auto sleep_time = std::chrono::nanoseconds(static_cast<uint32_t>(task_00.result() * 2 / std::thread::hardware_concurrency()));

		// ===== First test =====

		std::cout << "\n\tThreadpool size: " << threadpool.size() << "\n";
		std::cout << "\tThreadpool state: " << (threadpool.state() == stp::threadpool_state::running ?
												"running\n" :
												(threadpool.state() == stp::threadpool_state::stopping ?
												"stopping\n" :
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

		// We can wait for stp::task::ready() to return "true", by which time stp::task::result() shall return the value of the task
		while (!task_01.ready() && !task_02.ready() && !task_03.ready() && !task_04.ready())
		{
			std::cout << "\t\tNumber of running threads: " << threadpool.running() << "\n";
			std::this_thread::sleep_for(sleep_time);
		}

		if (task_01.ready()) // It should always display this message
		{
			std::cout << "\t\tOK - Task 01 done before other tasks\n";
		}
		else
		{
			std::cout << "\t\tError - Some task done before task 01\n";
		}

		while (!task_01.ready() || !task_02.ready() || !task_03.ready() || !task_04.ready())
		{
			std::cout << "\t\tNumber of running threads: " << threadpool.running() << "\n";
			std::this_thread::sleep_for(sleep_time);
		}

		stop_timer = std::chrono::high_resolution_clock::now();

		std::cout << "\tElapsed on first test: " << std::chrono::duration<long double, std::nano>(stop_timer - start_timer).count() << "ns\n";

		threadpool.resize(4);

		// ===== Second test =====

		while (threadpool.size() != 4);

		std::cout << "\n\tThreadpool size: " << threadpool.size() << "\n";
		std::cout << "\tThreadpool state: " << (threadpool.state() == stp::threadpool_state::running ?
												"running\n" :
												(threadpool.state() == stp::threadpool_state::stopping ?
												"stopping\n" :
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

		while (!task_05.ready() || !task_06.ready() || !task_07.ready() || !task_08.ready())
		{
			std::cout << "\t\tNumber of synchronized running threads: " << threadpool.sync_running() << "\n";
			std::this_thread::sleep_for(sleep_time);
		}

		stop_timer = std::chrono::high_resolution_clock::now();

		std::cout << "\tElapsed on second test: " << std::chrono::duration<long double, std::nano>(stop_timer - start_timer).count() << "ns\n";

		threadpool.notify_new_tasks(false);

		threadpool.resize(2);

		// ===== Third test =====

		while (threadpool.size() != 2);

		std::cout << "\n\tThreadpool size: " << threadpool.size() << "\n";
		std::cout << "\tThreadpool state: " << (threadpool.state() == stp::threadpool_state::running ?
												"running\n" :
												(threadpool.state() == stp::threadpool_state::stopping ?
												"stopping\n" :
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

		while (!task_09.ready() && !task_10.ready() && !task_11.ready() && !task_12.ready())
		{
			std::cout << "\t\tNumber of running threads: " << threadpool.running() << "\n";
			std::this_thread::sleep_for(sleep_time);
		}

		if (task_10.ready() || task_12.ready()) // It should always display this message
		{
			std::cout << "\t\tOK - Tasks 10 or 12 done before other tasks\n";
		}
		else
		{
			std::cout << "\t\tError - Some tasks done before tasks 06 and 08\n";
		}

		while (threadpool.waiting() != 2);

		std::cout << "\t\tNumber of waiting threads: " << threadpool.waiting() << "\n";

		if (!task_09.ready())
		{
			threadpool.new_task(task_09, stp::task_priority::very_low);
		}
		if (!task_10.ready())
		{
			threadpool.new_task(task_10, stp::task_priority::high);
		}
		if (!task_11.ready())
		{
			threadpool.new_task(task_11, stp::task_priority::low);
		}
		if (!task_12.ready())
		{
			threadpool.new_task(task_12, stp::task_priority::very_high);
		}

		threadpool.notify_new_tasks();

		while (!task_09.ready() || !task_10.ready() || !task_11.ready() || !task_12.ready())
		{
			std::cout << "\t\tNumber of running threads: " << threadpool.running() << "\n";
			std::this_thread::sleep_for(sleep_time);
		}

		stop_timer = std::chrono::high_resolution_clock::now();

		std::cout << "\tElapsed on third test: " << std::chrono::duration<long double, std::nano>(stop_timer - start_timer).count() << "ns\n";

		threadpool.stop();

		threadpool.resize(8);

		// ===== Fourth test =====

		while (threadpool.size() != 8);

		std::cout << "\n\tThreadpool size: " << threadpool.size() << "\n";
		std::cout << "\tThreadpool state: " << (threadpool.state() == stp::threadpool_state::running ?
												"running\n" :
												(threadpool.state() == stp::threadpool_state::stopping ?
												"stopping\n" :
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
			while (!task_13.ready() && !task_14.ready() && !task_15.ready() && !task_16.ready());

			if (task_13.ready() && !task_13_check)
			{
				std::cout << "\t\tTask 13 done\n";
				task_13_check = true;
			}
			if (task_14.ready() && !task_14_check)
			{
				std::cout << "\t\tTask 14 done\n";
				task_14_check = true;
			}
			if (task_15.ready() && !task_15_check)
			{
				std::cout << "\t\tTask 15 done\n";
				task_15_check = true;
			}
			if (task_16.ready() && !task_16_check)
			{
				std::cout << "\t\tTask 16 done\n";
				task_16_check = true;
			}
		}

		stop_timer = std::chrono::high_resolution_clock::now();

		std::cout << "\tElapsed on fourth test: " << std::chrono::duration<long double, std::nano>(stop_timer - start_timer).count() << "ns\n\n";

		threadpool.terminate();

		while (threadpool.active() != 0);

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
		std::cout << "Elapsed on vector 16: " << task_16.result() << "ns\n" << std::endl;
	}

	auto stop_test = std::chrono::high_resolution_clock::now();

#if OUTPUT_TO_FILE_ == 1
	fout.close();
	std::cout.rdbuf(cout_buffer);
#endif

	std::cout << "Testing finished\nElapsed on test: " << std::chrono::duration<long double>(stop_test - start_test).count() << "s\n\nPress enter to exit\n";
	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

	return 0;
}
