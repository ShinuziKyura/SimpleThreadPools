#include "stp.hpp"

#include <iostream>
#include <algorithm>
#include <random>

constexpr size_t minimum_threads = 1;
constexpr size_t maximum_threads = 8;

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
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	std::cout << std::scientific;
	std::cout.precision(5);

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

	{
		std::cout << "Generating 16 vectors...\n";

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
	}

	{
		std::cout << "\n" << "Sorting 16 vectors...\n";

		stp::threadpool threadpool(minimum_threads, maximum_threads); // Default: stp::threadpool_state::waiting
																	  // std::thread::hardware_concurrency()
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
		auto sleep_time = std::chrono::nanoseconds(static_cast<int64_t>(task_00.result() / 5.0));

		std::cout << "\tSize of threadpool: " << threadpool.size() << "\n";
		std::cout << "\tFirst test - Single thread\n";

		std::cout << "\t\tNumber of sleeping threads: " << threadpool.sleeping() << "\n";
		std::cout << "\t\tNumber of waiting threads: " << threadpool.waiting() << "\n";

		threadpool.new_task(task_01); // Default: stp::task_priority::normal
		threadpool.new_task(task_02, stp::task_priority::normal);
		threadpool.new_task(task_03);
		threadpool.new_task(task_04);
		
		while (threadpool.waiting() != 1);

		threadpool.run();

		start_timer = std::chrono::high_resolution_clock::now();

		// We can wait for task::ready() to return "true", by which time task::result() shall return the value of the task
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

		threadpool.stop();

		threadpool.resize(4);

		std::cout << "\tSecond test - Multiple synchronized threads\n";		

		threadpool.new_sync_task(task_05);
		threadpool.new_sync_task(task_06);
		threadpool.new_sync_task(task_07);
		threadpool.new_sync_task(task_08);

		while (threadpool.sync_waiting() != 4);

		std::cout << "\t\tNumber of sleeping threads: " << threadpool.sleeping() << "\n";
		std::cout << "\t\tNumber of synchronized waiting threads: " << threadpool.sync_waiting() << "\n";

		threadpool.run();

		threadpool.run_sync_tasks();

		start_timer = std::chrono::high_resolution_clock::now();

		while (!task_05.result_ready() || !task_06.result_ready() || !task_07.result_ready() || !task_08.result_ready())
		{
			std::cout << "\t\tNumber of synchronized running threads: " << threadpool.sync_running() << "\n";
			std::this_thread::sleep_for(sleep_time);
		}

		stop_timer = std::chrono::high_resolution_clock::now();

		std::cout << "\tElapsed on second test: " << std::chrono::duration<double, std::nano>(stop_timer - start_timer).count() << "ns\n";

		threadpool.stop();

		threadpool.set_notify_tasks(false);

		threadpool.resize(2);

		std::cout << "\tThird test - Overworked threads\n";

		std::cout << "\t\tNumber of sleeping threads: " << threadpool.sleeping() << "\n";
		std::cout << "\t\tNumber of waiting threads: " << threadpool.waiting() << "\n";

		threadpool.new_task(task_09, stp::task_priority::very_low);
		threadpool.new_task(task_10, stp::task_priority::high);
		threadpool.new_task(task_11, stp::task_priority::low);
		threadpool.new_task(task_12, stp::task_priority::very_high);
		
		while (threadpool.waiting() != 2);

		threadpool.notify_tasks();

		threadpool.run();

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
			std::cout << "\t\tError - Some tasks done before tasks 10 and 12\n";
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

		threadpool.notify_tasks();

		while (!task_09.result_ready() || !task_10.result_ready() || !task_11.result_ready() || !task_12.result_ready())
		{
			std::cout << "\t\tNumber of running threads: " << threadpool.running() << "\n";
			std::this_thread::sleep_for(sleep_time);
		}

		stop_timer = std::chrono::high_resolution_clock::now();

		std::cout << "\tElapsed on third test: " << std::chrono::duration<double, std::nano>(stop_timer - start_timer).count() << "ns\n";

		threadpool.stop();

		threadpool.set_notify_tasks(true);

		threadpool.resize(8);

		std::cout << "\tFourth test - Underworked threads\n";

		threadpool.new_task(task_13);
		threadpool.new_task(task_14, stp::task_priority::minimum);
		threadpool.new_sync_task(task_15, stp::task_priority::normal);
		threadpool.new_sync_task(task_16, stp::task_priority::maximum);

		while (threadpool.waiting() != 6 || threadpool.sync_waiting() != 2);

		std::cout << "\t\tNumber of sleeping threads: " << threadpool.sleeping() << "\n";
		std::cout << "\t\tNumber of waiting threads: " << threadpool.waiting() << "\n";
		std::cout << "\t\tNumber of synchronized waiting threads: " << threadpool.sync_waiting() << "\n";

		threadpool.finalize();

		start_timer = std::chrono::high_resolution_clock::now();

		while (!task_13.result_ready() || !task_14.result_ready() || !task_15.result_ready() || !task_16.result_ready())
		{
			std::cout << "\t\tNumber of waiting threads: " << threadpool.waiting() << "\n";			
			std::cout << "\t\tNumber of running threads: " << threadpool.running() << "\n";
			std::cout << "\t\tNumber of synchronized waiting threads: " << threadpool.sync_waiting() << "\n";
			std::cout << "\t\tNumber of synchronized running threads: " << threadpool.sync_running() << "\n";
			std::this_thread::sleep_for(sleep_time);
		}

		stop_timer = std::chrono::high_resolution_clock::now();

		std::cout << "\tElapsed on fourth test: " << std::chrono::duration<double, std::nano>(stop_timer - start_timer).count() << "ns\n";

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

	std::cout << std::flush;

	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

	return 0;
}
