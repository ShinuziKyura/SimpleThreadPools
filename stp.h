#ifndef SIMPLE_THREAD_POOLS_H_
#define SIMPLE_THREAD_POOLS_H_

#include <future>
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
			auto package = std::make_shared<std::packaged_task<decltype(func(args ...))()>>(std::bind(func, args ...));
			task_ = std::function<void()>([package]
			{
				(*package)();
			});
			task_result_ = package->get_future();
		}
		task<ReturnType>(task<ReturnType> const &) = delete;
		task<ReturnType> & operator=(task<ReturnType> const &) = delete;
		task<ReturnType>(task<ReturnType> &&) = default;
		task<ReturnType> & operator=(task<ReturnType> &&) = default;
		~task<ReturnType>() = default;
	private:
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
			if (thread_state_ == state_t::running || thread_state_ == state_t::waiting)
			{
				std::unique_lock<std::mutex> lock(thread_lock_);
				task_queue_.push(std::make_pair(&task.task_, true));
				notification_queue_.push(notification(1, message_t::new_task, this));
				lock.unlock();
			}
			if (thread_state_ == state_t::running)
			{
				thread_alert_.notify_one();
			}
		}
		template <typename ReturnType>
		void new_sync_task(task<ReturnType> & task)
		{
			if (thread_state_ == state_t::running || thread_state_ == state_t::waiting)
			{
				std::unique_lock<std::mutex> lock(thread_lock_);
				task_queue_.push(std::make_pair(&task.task_, false));
				notification_queue_.push(notification(1, message_t::new_sync_task, this));
				lock.unlock();
			}
			if (thread_state_ == state_t::running)
			{
				thread_alert_.notify_one();
			}
		}
		void start_sync_task()
		{
			if (thread_state_ == state_t::running)
			{
				std::unique_lock<std::mutex> lock(thread_lock_);
				while (!notification_queue_.empty() && notification_queue_.top().notified != thread_number_)
				{
					lock.unlock();
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					lock.lock();
				}
				notification_queue_.push(notification(thread_number_, message_t::start_sync_task, this));
				lock.unlock();
				thread_alert_.notify_all();
			}
		}
		void run()
		{
			if (thread_state_ == state_t::waiting)
			{
				thread_state_ = state_t::running;
				std::unique_lock<std::mutex> lock(thread_lock_);
				while (!notification_queue_.empty() && notification_queue_.top().notified != thread_number_)
				{
					lock.unlock();
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					lock.lock();
				}
				notification_queue_.push(notification(thread_number_, message_t::run, this));
				lock.unlock();
				thread_alert_.notify_all();
			}
		}
		void stop()
		{
			if (thread_state_ == state_t::running)
			{
				thread_state_ = state_t::waiting;
				std::unique_lock<std::mutex> lock(thread_lock_);
				while (!notification_queue_.empty() && notification_queue_.top().notified != thread_number_)
				{
					lock.unlock();
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					lock.lock();
				}
				notification_queue_.push(notification(thread_number_, message_t::stop, this));
				lock.unlock();
				thread_alert_.notify_all();
			}
		}
		void finalize()
		{
			if (thread_state_ != state_t::finalizing)
			{
				thread_state_ = state_t::finalizing;
				std::unique_lock<std::mutex> lock(thread_lock_);
				while (!notification_queue_.empty() && notification_queue_.top().notified != thread_number_)
				{
					lock.unlock();
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					lock.lock();
				}
				notification_queue_.push(notification(thread_number_, message_t::finalize, this));
				lock.unlock();
				thread_alert_.notify_one();
			}
		}
		bool is_active()
		{
			std::unique_lock<std::mutex> lock(thread_lock_);
			bool is_active = thread_active_ > (thread_state_ == state_t::finalizing ? -8 : 0);
			lock.unlock();
			return is_active;
		}
		int size()
		{
			return static_cast<int const>(thread_number_);
		}

		threadpool() = delete;
		threadpool(uint8_t thread_number) :
			thread_number_(thread_number == 0 ? std::thread::hardware_concurrency() : thread_number)
		{
			thread_array_ = new std::thread[thread_number_];
			for (uint8_t i = 0; i < thread_number_; ++i)
			{
				thread_array_[i] = std::thread(&threadpool::threadpool_, this);
			}
			thread_monitor_ = new std::thread(&threadpool::threadpool_monitor_, this);
		}
		threadpool(threadpool const &) = delete;
		threadpool & operator=(threadpool const &) = delete;
		threadpool(threadpool &&) = delete;
		threadpool & operator=(threadpool &&) = delete;
		~threadpool()
		{
			thread_state_ = state_t::finalizing;
			std::unique_lock<std::mutex> lock(thread_lock_);
			notification_queue_.push(notification(0, message_t::terminate, this));
			lock.unlock();
			thread_alert_.notify_all();
			for (uint8_t i = 0; i < thread_number_; ++i)
			{
				thread_array_[i].join();
			}
			delete[] thread_array_;
			thread_monitor_->join();
			delete thread_monitor_;
		}
	private:
		enum class state_t
		{
			running,
			waiting,
			finalizing
		};
		enum class message_t
		{
			run,
			stop,
			new_task,
			new_sync_task,
			start_sync_task,
			finalize,
			terminate
		};

		struct notification
		{
			uint64_t id;
			uint8_t priority = 0;
			message_t message;
			mutable uint16_t notified;
			notification(uint8_t notified, message_t message, threadpool * threadpool) : id(++threadpool->notification_id_), message(message), notified(notified)
			{
				switch (message)
				{
					case message_t::terminate:
						++priority;
					case message_t::finalize:
						++priority;
					case message_t::run:
					case message_t::stop:
						++priority;
					case message_t::new_task:
					case message_t::new_sync_task:
						++priority;
					case message_t::start_sync_task:
						break;
				}
				if (id == 0)
				{
					id = ++threadpool->notification_id_;
				}
			}
		};
		struct notification_comparator
		{
			notification_comparator() = default;
			bool operator()(notification const & n1, notification const & n2) const
			{
				if (n1.priority == 0 || n2.priority == 0 || n1.priority == n2.priority)
				{
					return n1.id > n2.id;
				}
				else
				{
					return n1.priority < n2.priority;
				}
			}
		};

		std::queue<std::pair<std::function<void()> *, bool>> task_queue_;
		std::priority_queue<notification, std::deque<notification>, notification_comparator> notification_queue_;
		std::thread * thread_array_;
		std::thread * thread_monitor_;
		std::mutex thread_lock_;
		std::condition_variable thread_alert_;
		state_t thread_state_ = state_t::waiting; // Should the threadpool sync with this var?
		int8_t thread_active_ = 0;
		uint8_t const thread_number_;
		uint64_t notification_id_ = 0;

		void threadpool_()
		{
			uint64_t last_id = 0;
			state_t state = state_t::waiting;
			std::pair<std::function<void()> *, bool> task(nullptr, false);
			std::unique_lock<std::mutex> lock(thread_lock_);

			while (true)
			{
				thread_alert_.wait(lock, [this, &last_id]
				{
					if (!notification_queue_.empty())
					{
						return notification_queue_.top().id != last_id;
					}
					return false;
				});
				last_id = notification_queue_.top().id;

				switch (notification_queue_.top().message)
				{
					case message_t::new_task:
					case message_t::new_sync_task:
						if (!task.first)
						{
							task = task_queue_.front();
							task_queue_.pop();
							if (--notification_queue_.top().notified == 0)
							{
								notification_queue_.pop();
							}
						}
						break;
					case message_t::start_sync_task:
						if (state == state_t::running)
						{
							if (task.first)
							{
								task.second = true;
							}
							if (--notification_queue_.top().notified == 0)
							{
								notification_queue_.pop();
							}
						}
						break;
					case message_t::run:
						if (state == state_t::waiting)
						{
							state = state_t::running;
							if (--notification_queue_.top().notified == 0)
							{
								notification_queue_.pop();
							}
						}
						break;
					case message_t::stop:
						if (state == state_t::running)
						{
							state = state_t::waiting;
							if (--notification_queue_.top().notified == 0)
							{
								notification_queue_.pop();
							}
						}
						break;
					case message_t::finalize:
						if (--notification_queue_.top().notified == 0)
						{
							notification_queue_.pop();
						}
						if (task.first)
						{
							++thread_active_;
							lock.unlock();
							(*task.first)();
							lock.lock();
							--thread_active_;
						}
						while ((notification_queue_.empty() || notification_queue_.top().message != message_t::terminate) && !task_queue_.empty())
						{
							task = task_queue_.front();
							task_queue_.pop();

							++thread_active_;
							lock.unlock();
							(*task.first)();
							lock.lock();
							--thread_active_;
						}
					case message_t::terminate:
						--thread_active_;
						lock.unlock();
						return;
				}

				if (task.first && task.second)
				{
					++thread_active_;
					lock.unlock();
					(*task.first)();
					task = {nullptr, false};
					lock.lock();
					--thread_active_;
				}
			}
		}
		void threadpool_monitor_() // Make it more robust
		{
			std::unique_lock<std::mutex> lock(thread_lock_, std::defer_lock);
			while (lock.lock(), -thread_active_ != thread_number_)
			{
				lock.unlock();
				thread_alert_.notify_all();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			lock.unlock();
		}
	};
}

#endif//SIMPLE_THREAD_POOLS_H_
