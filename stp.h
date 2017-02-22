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
#include <deque>

namespace stp
{
	template <typename ReturnType>
	class task
	{
	public:
		ReturnType result()
		{
			if (task_result_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			{
				throw std::logic_error("Future not ready");
			}
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
			notify_one(notification_(1, message_t::new_task, this));
		}
		template <typename ReturnType>
		void new_sync_task(task<ReturnType> & task)
		{
			lock_ lock_(thread_lock_);
			task_queue_.push(std::make_pair(&task.task_, true));
			lock_.unlock();
			notify_one(notification_(1, message_t::new_sync_task, this));
		}
		void run() // Do checks to calls after finalize
		{
			thread_state_ == state_t::running;
			notify_all(notification_(thread_count_, message_t::run, this));
		}
		void run_sync()
		{
			notify_all(notification_(thread_count_, message_t::run_sync, this));
		}
		void stop()
		{
			thread_state_ == state_t::waiting;
			notify_all(notification_(thread_count_, message_t::stop, this));
		}
		void finalize()
		{
			notify_all(notification_(thread_count_, message_t::finalize, this));
		}
		bool is_waiting()
		{
			lock_ lock_(thread_lock_);
			bool is_waiting = thread_active_ == thread_count_;
			lock_.unlock();
			return is_waiting;
		}

		threadpool() = delete;
		threadpool(uint16_t thread_count) :
			thread_count_(thread_count == 0 ? std::thread::hardware_concurrency() : thread_count)
		{
			thread_array_ = new std::thread[thread_count_];
			for (uint16_t i = 0; i < thread_count_; ++i)
			{
				thread_array_[i] = std::thread(&threadpool::threadpool_, this);
			}
			thread_watcher_ = new std::thread(&threadpool::threadpool_watcher_, this);
		}
		threadpool(threadpool const &) = delete;
		threadpool & operator=(threadpool const &) = delete;
		threadpool(threadpool &&) = delete;
		threadpool & operator=(threadpool &&) = delete;
		~threadpool()
		{
			notify_all(notification_(thread_count_, message_t::terminate, this));

			for (uint16_t i = 0; i < thread_count_; ++i)
			{
				thread_array_[i].join();
			}
			thread_watcher_->join();
			delete[] thread_array_;
			delete thread_watcher_;
		}
	private:
		typedef std::pair<std::function<void()> *, bool> task_;
		typedef std::unique_lock <std::mutex> lock_;
		enum class state_t
		{
			running,
			waiting
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
			mutable uint16_t notified;
			uint16_t priority = 0;
			message_t msg;
			notification_(uint16_t notified, message_t msg, threadpool * this_) : id(++(this_->notification_id_)), notified(notified), msg(msg)
			{
				switch (msg)
				{
					case message_t::terminate:
						++priority;
					case message_t::finalize:
						++priority;
					case message_t::run:
					case message_t::stop:
						++priority;
						++priority;
					case message_t::new_task:
					case message_t::new_sync_task:
						++priority;
						break;
					case message_t::run_sync:
						priority = (this_->thread_state_ == state_t::running ? 0 : 2);
				}
				if (id == 0)
				{
					id = ++(this_->notification_id_);
				}
			}
		};
		struct notification_comparator_
		{
			notification_comparator_() = default;
			bool operator()(notification_ const & n1, notification_ const & n2) const
			{
				return n1.priority < n2.priority;
			}
		};

		std::queue<task_> task_queue_;
		std::priority_queue<notification_, std::deque<notification_>, notification_comparator_> notification_queue_;
		std::thread * thread_array_;
		std::thread * thread_watcher_;
		std::mutex thread_lock_;
		std::condition_variable thread_alert_;
		state_t thread_state_ = state_t::waiting;
		uint16_t const thread_count_;
		int16_t thread_active_ = 0;
		uint64_t notification_id_ = 0;

		void notify_one(notification_ notification_)
		{
			lock_ lock_(thread_lock_);
			notification_queue_.push(notification_);
			thread_alert_.notify_one();
			lock_.unlock();
		}
		void notify_all(notification_ notification_)
		{
			lock_ lock_(thread_lock_);
			if (notification_queue_.top().priority > 1 || notification_queue_.top().priority == 0)
			{
				notification_queue_.top().notified = thread_count_;
			}
			notification_queue_.push(notification_);
			thread_alert_.notify_all();
			lock_.unlock();
		}
		void threadpool_()
		{
			uint64_t last_id_ = 0;
			message_t last_msg_;
			state_t state_ = state_t::waiting;
			task_ task_(nullptr, false);
			lock_ lock_(thread_lock_);

			while (true)
			{
				do
				{
					thread_alert_.wait(lock_);
				}
				while (notification_queue_.empty() || notification_queue_.top().id == last_id_ || (notification_queue_.top().priority == 0 && task_.first));
				last_id_ = notification_queue_.top().id;
				last_msg_ = notification_queue_.top().msg;
				
				switch (last_msg_)
				{
					case message_t::run:
						state_ = state_t::running;
						if (--notification_queue_.top().notified == 0)
						{
							notification_queue_.pop();
						}
						if (task_.first && !task_.second)
						{
							++thread_active_;
							lock_.unlock();
							(*task_.first)();
							task_.first = nullptr;
							lock_.lock();
							--thread_active_;
						}
						continue;
					case message_t::run_sync:
						if (state_ == state_t::running) // If running and priority == 2 then ignore
						{
							if (--notification_queue_.top().notified == 0)
							{
								notification_queue_.pop();
							}
							if (task_.first && task_.second)
							{
								++thread_active_;
								lock_.unlock();
								(*task_.first)();
								task_.first = nullptr;
								lock_.lock();
								--thread_active_;
							}
						}
						else // If waiting and priority == 0 then ignore
						{
							notification_queue_.pop();
						}
						continue;
					case message_t::new_task:
						if (state_ == state_t::running)
						{
							if (--notification_queue_.top().notified == 0)
							{
								notification_queue_.pop();
							}
							if (!task_queue_.empty())
							{
								task_ = task_queue_.front();
								task_queue_.pop();

								++thread_active_;
								lock_.unlock();
								(*task_.first)();
								task_.first = nullptr;
								lock_.lock();
								--thread_active_;
							}
						}
						continue;
					case message_t::new_sync_task:
						if (state_ == state_t::running)
						{
							if (--notification_queue_.top().notified == 0)
							{
								notification_queue_.pop();
							}
							if (!task_queue_.empty())
							{
								task_ = task_queue_.front();
								task_queue_.pop();
							}
						}
						continue;
					case message_t::stop:
						state_ = state_t::waiting;
						if (--notification_queue_.top().notified == 0)
						{
							notification_queue_.pop();
						}
						continue;
					case message_t::finalize:
						if (--notification_queue_.top().notified == 0)
						{
							notification_queue_.pop();
						}
						if (task_.first)
						{
							++thread_active_;
							lock_.unlock();
							(*task_.first)();
							task_.first = nullptr;
							lock_.lock();
							--thread_active_;
						}
						while (!task_queue_.empty())
						{
							task_ = task_queue_.front();
							task_queue_.pop();

							++thread_active_;
							lock_.unlock();
							(*task_.first)();
							task_.first = nullptr;
							lock_.lock();
							--thread_active_;
						}
					case message_t::terminate:
						if (--notification_queue_.top().notified == 0)
						{
							notification_queue_.pop();
						}
						--thread_active_;
						break;
				}
				break;
			}
			lock_.unlock();
		}
		void threadpool_watcher_()
		{
			lock_ lock_(thread_lock_, std::defer_lock);
			while (lock_.lock(), -thread_active_ != thread_count_)
			{
				lock_.unlock();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				thread_alert_.notify_all();
			}
			lock_.unlock();
		}
	};
}

#endif//SIMPLE_THREAD_POOLS_H_
