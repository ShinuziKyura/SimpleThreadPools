#ifndef SIMPLE_THREAD_POOLS_H_
#define SIMPLE_THREAD_POOLS_H_

#include <future>
#include <shared_mutex>
#include <queue>

namespace stp
{
	enum class thread_state
	{
		running,
		waiting,
		finalizing,
		terminating
	};

	template <typename ReturnType>
	class task
	{
	public:	
		ReturnType result()
		{
			if (result_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			{
				throw std::logic_error("Future not ready");
			}
			return result_.get();
		}
		bool executed()
		{
			return result_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
		}

		task<ReturnType>() = delete;
		template <typename FuncType, typename ... ArgType>
		task<ReturnType>(FuncType func, ArgType ... args) : package_(std::bind(func, args ...))
		{
			task_ = std::function<void()>([this]
			{
				package_();
			});
			result_ = package_.get_future();
		}
		task<ReturnType>(task<ReturnType> const &) = delete;
		task<ReturnType> & operator=(task<ReturnType> const &) = delete;
		task<ReturnType>(task<ReturnType> &&) = default;
		task<ReturnType> & operator=(task<ReturnType> &&) = default;
		~task<ReturnType>() = default;

		void operator()(nullptr_t)
		{
			task_();
		}
		ReturnType operator()()
		{
			task_();
			return result_.get();
		}
	private:
		std::packaged_task<ReturnType()> package_;
		std::function<void()> task_;
		std::future<ReturnType> result_;

		friend class threadpool;
	};

	class threadpool
	{
	public:
		template <typename ReturnType>
		void new_task(task<ReturnType> & task)
		{
			if (thread_state_ != stp::thread_state::finalizing &&
				thread_state_ != stp::thread_state::terminating)
			{
				thread_lock_.lock();
				task_queue_.push(std::make_pair(&task.task_, false));
				thread_lock_.unlock();

				if (thread_state_ == stp::thread_state::running)
				{
					thread_alert_.notify_one();
				}
			}
		}
		template <typename ReturnType>
		void new_sync_task(task<ReturnType> & task)
		{
			if (thread_state_ != stp::thread_state::finalizing &&
				thread_state_ != stp::thread_state::terminating)
			{
				thread_lock_.lock();
				task_queue_.push(std::make_pair(&task.task_, true));
				thread_lock_.unlock();

				if (thread_state_ == stp::thread_state::running)
				{
					thread_alert_.notify_one();
				}
			}
		}
		void run_synced()
		{
			if (thread_state_ != stp::thread_state::finalizing &&
				thread_state_ != stp::thread_state::terminating)
			{
				thread_sync_executed_ = thread_sync_ready_._My_val;

				if (thread_state_ == stp::thread_state::running)
				{
					thread_sync_alert_.notify_all();
				}
			}
		}
		void run()
		{
			if (thread_state_ != stp::thread_state::finalizing &&
				thread_state_ != stp::thread_state::terminating)
			{
				thread_state_changed_ = thread_number_._My_val;
				thread_state_ = stp::thread_state::running;
				thread_alert_.notify_all();
				thread_sync_alert_.notify_all();
			}
		}
		void stop()
		{
			if (thread_state_ != stp::thread_state::finalizing &&
				thread_state_ != stp::thread_state::terminating)
			{
				thread_state_changed_ = thread_number_._My_val;
				thread_state_ = stp::thread_state::waiting;
				thread_alert_.notify_all();
				thread_sync_alert_.notify_all();
			}
		}
		void finalize()
		{
			if (thread_state_ != stp::thread_state::finalizing &&
				thread_state_ != stp::thread_state::terminating)
			{
				thread_state_changed_ = thread_number_._My_val;
				thread_state_ = stp::thread_state::finalizing;
				thread_alert_.notify_all();
				thread_sync_alert_.notify_all();
			}
		}
		int running()
		{
			return thread_run_ + thread_sync_run_;
		}
		int ready()
		{
			return thread_ready_;
		}
		int synced()
		{
			return thread_sync_ready_;
		}
		bool available()
		{
			return thread_ready_ != 0;
		}
		int size()
		{
			if (thread_state_ == stp::thread_state::finalizing ||
				thread_state_ == stp::thread_state::terminating)
			{
				return 0;
			}
			return thread_number_;
		}

		threadpool() = delete;
		threadpool(int16_t thread_number, stp::thread_state thread_state = stp::thread_state::waiting) :
			thread_number_(thread_number > 0 ? thread_number : std::thread::hardware_concurrency())
		{
			thread_array_ = new std::thread[thread_number_];
			for (int16_t i = 0; i < thread_number_; ++i)
			{
				thread_array_[i] = std::thread(&stp::threadpool::threadpool__, this);
			}
		}
		threadpool(threadpool const &) = delete;
		threadpool & operator=(threadpool const &) = delete;
		threadpool(threadpool &&) = delete;
		threadpool & operator=(threadpool &&) = delete;
		~threadpool()
		{
			thread_state_changed_ = thread_number_._My_val;
			thread_state_ = stp::thread_state::terminating;
			thread_alert_.notify_all();
			thread_sync_alert_.notify_all();

			for (int16_t i = 0; i < thread_number_; ++i)
			{
				thread_array_[i].join();
			}
			delete[] thread_array_;
		}
	private:
		std::queue<std::pair<std::function<void()> *, bool>> task_queue_;
		std::thread * thread_array_;
		std::mutex thread_lock_;
		std::shared_mutex thread_sync_lock_;
		std::condition_variable thread_alert_;
		std::condition_variable_any thread_sync_alert_;
		std::atomic<thread_state> thread_state_;
		std::atomic<int16_t> thread_state_changed_ = 0;
		std::atomic<int16_t> thread_sync_executed_ = 0;
		std::atomic<int16_t> thread_ready_;
		std::atomic<int16_t> thread_run_;
		std::atomic<int16_t> thread_sync_ready_;
		std::atomic<int16_t> thread_sync_run_;
		std::atomic<int16_t> const thread_number_;

		void threadpool__()
		{
			std::pair<std::function<void()> *, bool> task(nullptr, false);
			std::unique_lock<std::mutex> lock(thread_lock_);
			std::shared_lock<std::shared_mutex> shared_lock(thread_sync_lock_);
			++thread_ready_;

			while (thread_state_ != stp::thread_state::terminating)
			{
				do thread_alert_.wait_for(lock, std::chrono::milliseconds(1));
				while (!thread_state_changed_ && task_queue_.empty());

				if (thread_state_changed_)
				{
					--thread_state_changed_;
				}

				while (thread_state_ != stp::thread_state::terminating)
				{
					switch (thread_state_)
					{
						case stp::thread_state::running:
							if (!task.first && !task_queue_.empty())
							{
								task = task_queue_.front();
								task_queue_.pop();
							}

							if (task.first && !task.second)
							{
								threadrun__(task.first, lock);
							}
							else if (task.first && task.second)
							{
								threadsync__(task.first, lock, shared_lock);
							}
							else
							{
								break;
							}

							continue;
						case stp::thread_state::waiting:
							if (!task.first && !task_queue_.empty())
							{
								task = task_queue_.front();
								task_queue_.pop();
							}

							if (task.first && task.second)
							{
								threadsync__(task.first, lock, shared_lock);
							}
							else
							{
								break;
							}

							continue;
						case stp::thread_state::finalizing:
							if (!task.first && !task_queue_.empty())
							{
								task = task_queue_.front();
								task_queue_.pop();
							}

							if (task.first)
							{
								threadrun__(task.first, lock);
							}
							else
							{
								break;
							}

							continue;
						case stp::thread_state::terminating:
							break;
					}
					break;
				}
			}

			--thread_ready_;
			shared_lock.unlock();
			lock.unlock();
		}
		void threadrun__(std::function<void()> * & task, std::unique_lock<std::mutex> & lock)
		{
			--thread_ready_;
			++thread_run_;
			lock.unlock();

			(*task)();
			task = nullptr;

			lock.lock();
			--thread_run_;
			++thread_ready_;
		}
		void threadsync__(std::function<void()> * & task, std::unique_lock<std::mutex> & lock, std::shared_lock<std::shared_mutex> & shared_lock)
		{
			--thread_ready_;
			++thread_sync_ready_;
			lock.unlock();

			while (thread_state_ != stp::thread_state::terminating)
			{
				do thread_sync_alert_.wait_for(shared_lock, std::chrono::milliseconds(1));
				while (!thread_state_changed_ && !thread_sync_executed_);

				if (thread_state_changed_)
				{
					--thread_state_changed_;
				}

				switch (thread_state_)
				{
					case stp::thread_state::running:
						if (thread_sync_executed_)
						{
							--thread_sync_executed_;

							--thread_sync_ready_;
							++thread_sync_run_;

							(*task)();
							task = nullptr;

							--thread_sync_run_;

							break;
						}

						continue;
					case stp::thread_state::waiting:
						continue;
					case stp::thread_state::finalizing:
						--thread_sync_ready_;

						break;
					case stp::thread_state::terminating:
						break;
				}
				break;
			}

			lock.lock();
			++thread_ready_;
		}
	};
}

#endif//SIMPLE_THREAD_POOLS_H_
