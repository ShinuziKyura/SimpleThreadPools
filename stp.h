#ifndef SIMPLE_THREAD_POOLS_H_
#define SIMPLE_THREAD_POOLS_H_

#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <utility>
#include <queue>

namespace stp
{
	template <typename ReturnType>
	class task
	{
	public:
		ReturnType result()
		{
			if (task_result_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
				throw std::logic_error("Future not ready");
			return task_result_.get();
		}
		bool is_done()
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
			std::lock_guard<std::mutex> lock_(thread_lock_);
			task_queue_.push(&task.task_);
			notify_one(notification_(message::new_task, this));
		}
		template <typename ReturnType>
		void new_sync_task(task<ReturnType> & task)
		{
			std::lock_guard<std::mutex> lock_(thread_lock_);
			task_queue_.push(&task.task_);
			notify_one(notification_(message::new_sync_task, this));
		}
		void run()
		{
			std::lock_guard<std::mutex> lock_(thread_lock_);
			notify_all(notification_(message::run, this));
		}
		void run_sync()
		{
			std::lock_guard<std::mutex> lock_(thread_lock_);
			notify_all(notification_(message::run_sync, this));
		}
		void stop()
		{
			std::lock_guard<std::mutex> lock_(thread_lock_);
			notify_all(notification_(message::stop, this));
		}
		void finalize()
		{
			std::lock_guard<std::mutex> lock_(thread_lock_);
			notify_all(notification_(message::finalize, this));
		}
		bool is_running()
		{
			// To do
			return false;
		}

		threadpool() = delete;
		threadpool(size_t threadpool_size) : 
			threadpool_size_(threadpool_size == 0 ? threadpool_size = std::thread::hardware_concurrency() : threadpool_size)
		{
			thread_array_ = new std::thread[threadpool_size_];
			for (size_t i = 0; i < threadpool_size_; ++i)
				thread_array_[i] = std::thread(&threadpool::threadpool_, this);
		}
		threadpool(threadpool const &) = delete;
		threadpool & operator=(threadpool const &) = delete;
		threadpool(threadpool &&) = delete;
		threadpool & operator=(threadpool &&) = delete;
		~threadpool()
		{
			std::lock_guard<std::mutex> lock_(thread_lock_);
			notify_all(notification_(message::terminate, this));

			for (size_t i = 0; i < threadpool_size_; ++i)
				thread_array_[i].join();
			delete[] thread_array_;

			while (!notification_queue_.empty())
				delete notification_queue_.front();
		}
	private:
		typedef std::function<void()> task_;
		enum class message
		{
			run,
			run_sync,
			stop,
			new_task,
			new_sync_task,
			finalize,
			terminate
		};
		struct notification_
		{
			int id;
			int notified;
			message msg;
			notification_(message msg, threadpool * this_) : id(++(this_->notification_id_)), notified(this_->threadpool_size_), msg(msg)
			{
			}
		};

		std::queue<task_ *> task_queue_;
		std::queue<notification_ *> notification_queue_;
		std::thread * thread_array_;
		std::mutex thread_lock_;
		std::condition_variable thread_alert_;
		size_t const threadpool_size_;
		size_t notification_id_ = 0;

		void notify_one(notification_ notification_)
		{
			// To do
		}
		void notify_all(notification_ notification_)
		{
			// To do
		}
		void threadpool_()
		{
			// To do
			while (true);
		}
	};
}

#endif//SIMPLE_THREAD_POOLS_H_
