#ifndef SIMPLE_THREAD_POOLS_H_
#define SIMPLE_THREAD_POOLS_H_

#include <cstdint>
#include <chrono>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <utility>
#include <queue>
#include <deque>

namespace stp
{
	typedef void NullType;

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
			task_ = std::function<NullType()>([this]
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
		std::function<NullType()> task_;
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
				notification_queue_.push(notification(1, message_t::new_task)); // 0 allows for no conflict in notify_all functions // nvm
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
				notification_queue_.push(notification(1, message_t::new_sync_task));
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
				while (!notification_queue_.empty() && notification_queue_.top().notified != thread_count_)
				{
					lock.unlock();
					std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Or smth else
					lock.lock();
				}
				notification_queue_.push(notification(thread_count_, message_t::start_sync_task));
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
				while (!notification_queue_.empty() && notification_queue_.top().notified != thread_count_)
				{
					lock.unlock();
					std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Or smth else
					lock.lock();
				}
				notification_queue_.push(notification(thread_count_, message_t::run));
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
				while (!notification_queue_.empty() && notification_queue_.top().notified != thread_count_)
				{
					lock.unlock();
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					lock.lock();
				}
				notification_queue_.push(notification(thread_count_, message_t::stop));
				lock.unlock();
				thread_alert_.notify_all();
			}
		}
		void finalize()
		{
			if (thread_state_ != state_t::finalizing || thread_state_ != state_t::terminating)
			{
				thread_state_ = state_t::finalizing;
				std::unique_lock<std::mutex> lock(thread_lock_);
				while (!notification_queue_.empty() && notification_queue_.top().notified != thread_count_)
				{
					lock.unlock();
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					lock.lock();
				}
				notification_queue_.push(notification(1, message_t::finalize)); // Maybe 1, cause of how finalize works
				lock.unlock();
				thread_alert_.notify_one();
			}
		}
		bool is_active()
		{
			std::unique_lock<std::mutex> lock(thread_lock_);
			bool is_active = thread_active_ > (thread_state_ == state_t::terminating ? -8 : 0);
			lock.unlock();
			return is_active;
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
			thread_monitor_ = new std::thread(&threadpool::threadpool_monitor_, this);
		}
		threadpool(threadpool const &) = delete;
		threadpool & operator=(threadpool const &) = delete;
		threadpool(threadpool &&) = delete;
		threadpool & operator=(threadpool &&) = delete;
		~threadpool()
		{
			thread_state_ = state_t::terminating;
			std::unique_lock<std::mutex> lock(thread_lock_);
			notification_queue_.push(notification(thread_count_, message_t::terminate));
			lock.unlock();
			thread_alert_.notify_all();

			for (uint16_t i = 0; i < thread_count_; ++i)
			{
				thread_array_[i].join();
			}
			thread_monitor_->join();
			delete[] thread_array_;
			delete thread_monitor_;
		}
	private:
		enum class state_t
		{
			running,
			waiting,
			finalizing,
			terminating
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
		private:
			static uint64_t base_id;
		public:
			uint64_t id;
			uint16_t priority = 0;
			message_t message;
			mutable uint16_t notified;
			notification(uint16_t notified, message_t message) : id(++base_id), message(message), notified(notified)
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
					case message_t::start_sync_task: // Special priority
						break;
				}
				if (id == 0)
				{
					id = ++base_id;
				}
			}
		};
		struct notification_comparator
		{
			notification_comparator() = default;
			bool operator()(notification const & n1, notification const & n2) const
			{
				if (n1.priority == 0 || n2.priority == 0)
				{
					return n1.id > n2.id;
				}
				else
				{
					if (n1.priority < n2.priority)
						return true;
					else if (n1.priority == n2.priority)
						return n1.id > n2.id;
					else
						return false;
				}
			}
		};

		std::queue<std::pair<std::function<void()> *, bool>> task_queue_;
		std::priority_queue<notification, std::deque<notification>, notification_comparator> notification_queue_;
		std::thread * thread_array_;
		std::thread * thread_monitor_;
		std::mutex thread_lock_;
		std::condition_variable thread_alert_;

		state_t thread_state_ = state_t::waiting;
		int16_t thread_active_ = 0;
		uint16_t const thread_count_;

		void threadpool_()
		{
			uint64_t last_id = 0;
			state_t state = state_t::waiting;
			std::pair<std::function<void()> *, bool> task(nullptr, false);
			std::unique_lock<std::mutex> lock(thread_lock_);

			while (true)
			{
				if (state != state_t::finalizing)
				{
					thread_alert_.wait(lock, [this, &last_id]
					{
						if (!notification_queue_.empty())
							return notification_queue_.top().id != last_id;
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
						case message_t::run: // Only relevant while state_t == waiting
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
							state = state_t::finalizing;
							if (task.first)
							{
								task.second = true;
							}
							if (--notification_queue_.top().notified == 0)
							{
								notification_queue_.pop();
							}
							break;
						case message_t::terminate:
							state = state_t::terminating;
							if (--notification_queue_.top().notified == 0)
							{
								notification_queue_.pop();
							}
							break;
					}
				}
				else
				{
					if (notification_queue_.empty() || notification_queue_.top().message != message_t::terminate)
					{
						if (!task_queue_.empty())
						{
							task = task_queue_.front();
							task_queue_.pop();
							task.second = true;
						}
						else
						{
							state = state_t::terminating;
						}
					}
					else
					{
						state = state_t::terminating;
					}
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

				if (state == state_t::terminating)
				{
					--thread_active_;
					break;
				}
			}
			lock.unlock();
		}
		void threadpool_monitor_()
		{
			std::unique_lock<std::mutex> lock(thread_lock_, std::defer_lock);
			while (lock.lock(), -thread_active_ != thread_count_)
			{
				lock.unlock();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				thread_alert_.notify_all();
			}
			lock.unlock();
		}
	};

	uint64_t threadpool::notification::base_id = 0;
}

#endif//SIMPLE_THREAD_POOLS_H_
