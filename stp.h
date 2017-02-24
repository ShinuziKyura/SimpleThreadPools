#ifndef SIMPLE_THREAD_POOLS_H_
#define SIMPLE_THREAD_POOLS_H_

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <functional>
#include <utility>
#include <queue>

namespace stp
{
	enum class threadpool_status
	{
		running,
		stopping,
		terminating
	};

	template <typename ReturnType>
	class task
	{
	public:
		ReturnType task_result()
		{
			if (task_result_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
				throw std::logic_error("Future not ready");
			return task_result_.get();
		}
		bool is_ready()
		{
			return task_result_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
		}

		task<ReturnType>() = delete;
		template <typename FuncType, typename ... ArgType>
		task<ReturnType>(FuncType func, ArgType ... args)
		{
			package_ = std::packaged_task<decltype(func(args ...))()>(std::bind(func, args ...));
			task_ = std::function<void()>([this]
			{
				package_();
			});
			task_result_ = package_.get_future();
		}
		task<ReturnType>(task<ReturnType> const &) = delete;
		task<ReturnType> & operator=(task<ReturnType> const &) = delete;
		task<ReturnType>(task<ReturnType> &&) = default;
		task<ReturnType> & operator=(task<ReturnType> &&) = default;
		~task<ReturnType>() = default;
	private:
		std::packaged_task<ReturnType()> package_;
		std::function<void()> task_;
		std::future<ReturnType> task_result_;

		friend class threadpool;
	};

	class threadpool
	{
	public:
		template <typename ReturnType>
		void new_task(task<ReturnType> & task)
		{
			if (threadpool_status_.load() == threadpool_status::terminating)
				return;

			std::unique_lock<std::mutex> lock_(threadpool_lock_);
			threadpool_queue_.push(&task.task_);
			lock_.unlock();
			threadpool_alert_.notify_one();
		}
		void clear_tasks()
		{
			std::unique_lock<std::mutex> lock_(threadpool_lock_);
			while (!threadpool_queue_.empty())
				threadpool_queue_.pop();
			lock_.unlock();
		}
		void run()
		{
			if (threadpool_status_.load() == threadpool_status::terminating)
				return;

			std::unique_lock<std::mutex> lock_(threadpool_lock_);
			threadpool_status_.store(threadpool_status::running);
			lock_.unlock();
			threadpool_alert_.notify_all();
		}
		void stop()
		{
			if (threadpool_status_.load() == threadpool_status::terminating)
				return;

			std::unique_lock<std::mutex> lock_(threadpool_lock_);
			threadpool_status_.store(threadpool_status::stopping);
			lock_.unlock();
			threadpool_alert_.notify_all();
		}
		bool is_running()
		{
			if (threadpool_status_.load() == threadpool_status::terminating)
				return false;

			return threadpool_size_ != threadpool_ready_.load();
		}

		threadpool() = delete;
		threadpool(size_t threadpool_size, threadpool_status threadpool_status = threadpool_status::stopping) : 
			threadpool_size_(threadpool_size == 0 ? threadpool_size = std::thread::hardware_concurrency() : threadpool_size)
		{
			threadpool_status_.store(threadpool_status);
			threadpool_ready_.store(threadpool_size_);
			threadpool_array_ = new std::thread[threadpool_size_];

			while (threadpool_size > 0)
				threadpool_array_[threadpool_size_ - threadpool_size--] = std::thread(&threadpool::threadpool_, this);
		}
		threadpool(threadpool const &) = delete;
		threadpool & operator=(threadpool const &) = delete;
		threadpool(threadpool &&) = delete;
		threadpool & operator=(threadpool &&) = delete;
		~threadpool()
		{
			std::unique_lock<std::mutex> lock_(threadpool_lock_);
			threadpool_status_.store(threadpool_status::terminating);
			lock_.unlock();
			threadpool_alert_.notify_all();

			for (size_t i = 0; i < threadpool_size_; ++i)
				threadpool_array_[i].join();
			delete[] threadpool_array_;
		}
	private:
		std::queue<std::function<void()> *> threadpool_queue_;
		std::thread * threadpool_array_;
		std::mutex threadpool_lock_;
		std::condition_variable threadpool_alert_;
		std::atomic<threadpool_status> threadpool_status_;
		std::atomic<size_t> threadpool_ready_;
		size_t const threadpool_size_;

		void threadpool_()
		{
			std::function<void()> * task_ = nullptr;
			std::unique_lock<std::mutex> lock_(threadpool_lock_);

			while (true)
			{
				if (threadpool_status_.load() == threadpool_status::running)
				{
					if (!threadpool_queue_.empty())
					{
						task_ = threadpool_queue_.front();
						threadpool_queue_.pop();
						threadpool_ready_.fetch_sub(1);
						lock_.unlock();
						(*task_)();
						lock_.lock();
						threadpool_ready_.fetch_add(1);
					}
					else
					{
						auto wait_status_ = std::cv_status::timeout;
						while (wait_status_ == std::cv_status::timeout && threadpool_status_.load() == threadpool_status::running)
						{
							wait_status_ = threadpool_alert_.wait_for(lock_, std::chrono::seconds(1));
						}
					}
				}
				else if (threadpool_status_.load() == threadpool_status::stopping)
				{
					auto wait_status_ = std::cv_status::timeout;
					while (wait_status_ == std::cv_status::timeout && threadpool_status_.load() == threadpool_status::stopping)
					{
						wait_status_ = threadpool_alert_.wait_for(lock_, std::chrono::seconds(1));
					}
				}
				else if (threadpool_status_.load() == threadpool_status::terminating)
				{
					lock_.unlock();
					break;
				}
			}
		}
	};
}

#endif//SIMPLE_THREAD_POOLS_H_
