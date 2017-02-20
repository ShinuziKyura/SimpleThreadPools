#include "stp.h"
#include <iostream>
#include <chrono>

int function(int & n)
{
	return ++n;
}

int main()
{
	int foo = 0;
	int bar = 0;

	stp::threadpool my_pool(0);

	stp::task<int> my_task_1(function, foo);
	stp::task<int> my_task_2(function, std::ref(bar));

	my_pool.new_task(my_task_1);
	my_pool.new_task(my_task_2);

	std::cout << "foo: " << foo << std::endl;
	std::cout << "bar: " << bar << std::endl;

	my_pool.run();
	
	while (!my_task_1.is_ready());
	std::cout << "function: " << my_task_1.task_result() << "\nfoo: " << foo << std::endl;

	while (!my_task_2.is_ready());
	std::cout << "function: " << my_task_2.task_result() << "\nbar: " << bar;

	my_pool.stop();

	while (my_pool.is_running());
	return 0;
}