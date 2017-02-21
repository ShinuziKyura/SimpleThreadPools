#ifndef SIMPLE_THREAD_POOLS_H_
#define SIMPLE_THREAD_POOLS_H_

#include <cstdint>
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
			lock_ lock_(thread_lock_);
			task_queue_.push(std::make_pair(&task.task_, false));
			lock_.unlock();
			notify_one(notification_(message_t::new_task, this));
		}
		template <typename ReturnType>
		void new_sync_task(task<ReturnType> & task)
		{
			lock_ lock_(thread_lock_);
			task_queue_.push(std::make_pair(&task.task_, true));
			lock_.unlock();
			notify_one(notification_(message_t::new_sync_task, this));
		}
		void run()
		{
			notify_all(notification_(message_t::run, this));
		}
		void run_sync()
		{
			notify_all(notification_(message_t::run_sync, this));
		}
		void stop()
		{
			notify_all(notification_(message_t::stop, this));
		}
		void finalize()
		{
			notify_all(notification_(message_t::finalize, this));
		}
		bool is_running()
		{
			// To do
			return false;
		}

		threadpool() = delete;
		threadpool(uint16_t thread_count) :
			thread_count_(thread_count == 0 ? std::thread::hardware_concurrency() : thread_count)
		{
			thread_array_ = new std::thread[thread_count_];
			for (size_t i = 0; i < thread_count_; ++i)
				thread_array_[i] = std::thread(&threadpool::threadpool_, this);
		}
		threadpool(threadpool const &) = delete;
		threadpool & operator=(threadpool const &) = delete;
		threadpool(threadpool &&) = delete;
		threadpool & operator=(threadpool &&) = delete;
		~threadpool()
		{
			notify_all(notification_(message_t::terminate, this));

			for (size_t i = 0; i < thread_count_; ++i)
				thread_array_[i].join();
			delete[] thread_array_;
		}
	private:
		typedef std::pair<std::function<void()> *, bool> task_;
		typedef std::unique_lock <std::mutex> lock_;
		enum class state_t
		{
			running,
			waiting,
			finalizing
		};
		enum class message_t
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
			uint64_t id;
			uint16_t notified;
			message_t msg;
			notification_(message_t msg, threadpool * this_) : id(++(this_->notification_id_)), notified(this_->thread_count_), msg(msg)
			{
				if (id == 0)
					id = ++(this_->notification_id_);
			}
		};

		std::queue<task_> task_queue_;
		std::queue<notification_> notification_queue_;
		std::thread * thread_array_;
		std::mutex thread_lock_;
		std::condition_variable thread_alert_;
		uint16_t const thread_count_;
		uint64_t notification_id_ = 0;

		void notify_one(notification_ notification_)
		{
			lock_ lock_(thread_lock_);
			notification_queue_.push(notification_);
			lock_.unlock();
			thread_alert_.notify_one();
		}
		void notify_all(notification_ notification_)
		{
			lock_ lock_(thread_lock_);
			notification_queue_.push(notification_);
			lock_.unlock();
			thread_alert_.notify_all();
		}
		void threadpool_()
		{
			uint64_t last_id_ = 0;
			message_t last_msg_;
			state_t state_ = state_t::waiting;
			task_ task_(nullptr, false);
			lock_ lock_(thread_lock_);

			do
			{
				thread_alert_.wait(lock_, [=]
				{
					return (!notification_queue_.empty() && notification_queue_.front().id != last_id_);
				});
				last_id_ = notification_queue_.front().id;
				last_msg_ = notification_queue_.front().msg;
				if (--notification_queue_.front().notified == 0)
				{
					notification_queue_.pop();
				}

				switch (last_msg_)
				{
					case message_t::run:
						running = true;
						if (task_.first)
						{
							if (!task_.second)
							{
								lock_.unlock();
								(*task_.first)();
								lock_.lock();
							}
						}
						else
						{
							if (!task_queue_.empty())
							{
								task_ = task_queue_.front();
								task_queue_.pop();
								if (!task_.second)
								{
									lock_.unlock();
									(*task_.first)();
									task_.first = nullptr;
									lock_.lock();
								}
							}
						}
						continue;
					case message_t::run_sync:
						if (running)
						{
							if (task_.first && task_.second)
							{
								lock_.unlock();
								(*task_.first)();
								task_.first = nullptr;
								lock_.lock();
							}
						}
						continue;
					case message_t::new_task:
						if (running)
						{
							if (!task_queue_.empty())
							{
								task_ = task_queue_.front();
								task_queue_.pop();
								lock_.unlock();
								(*task_.first)();
								task_.first = nullptr;
								lock_.lock();
							}
						}
						continue;
					case message_t::new_sync_task:
						if (running)
						{
							if (!task_queue_.empty())
							{
								task_ = task_queue_.front();
								task_queue_.pop();
							}
						}
						continue;
					case message_t::stop:
						running = false;
						continue;
					case message_t::finalize:
						continue;
					case message_t::terminate:
						continue;
				}
			}
			while (false);
		}
	};
}

#endif//SIMPLE_THREAD_POOLS_H_
