#ifndef SIMPLE_THREAD_POOLS_H_
#define SIMPLE_THREAD_POOLS_H_

#include <future>
#include <queue>

namespace stp
{
	class thread_state_t
	{
		enum state : int
		{
			running = 0,
			waiting = 1,
			finalizing = 2,
			terminating = 3
		};

		friend class threadpool;
	};

	enum class thread_state : int
	{
		running = 0,
		waiting = 1
	};

	template <typename ReturnType>
	class task
	{
	public:
		ReturnType result()
		{
			if (result_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			{
				throw std::runtime_error("Future not ready");
			}
			return result_.get();
		}
		bool ready()
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

		void operator()()
		{
			task_();
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
			if (thread_state_ != thread_state_t::finalizing)
			{
				thread_lock_.lock();
				task_queue_.push(std::make_pair(&task.task_, false));
				thread_lock_.unlock();

				if (thread_state_ == thread_state_t::running)
				{
					thread_alert_.notify_one();
				}
			}
		}
		template <typename ReturnType>
		void new_sync_task(task<ReturnType> & task)
		{
			if (thread_state_ != thread_state_t::finalizing)
			{
				thread_lock_.lock();
				task_queue_.push(std::make_pair(&task.task_, true));
				thread_lock_.unlock();

				if (thread_state_ == thread_state_t::running)
				{
					thread_alert_.notify_one();
				}
			}
		}
		void run_synced()
		{
			if (thread_state_ != thread_state_t::finalizing)
			{
				if (thread_sync_executed_)
				{
					throw std::runtime_error("Threadpool synchronizing");
				}

				std::unique_lock<std::mutex> lock(thread_lock_, std::try_to_lock);
				if (!lock.owns_lock())
				{
					throw std::runtime_error("Threadpool busy");
				}

				thread_sync_run_ += thread_sync_ready_;
				thread_sync_count_ = thread_sync_ready_;
				thread_sync_ready_ = 0;

				lock.unlock();

				thread_sync_executed_ = true;

				if (thread_state_ == thread_state_t::running)
				{
					thread_sync_alert_.notify_all();
				}
			}
		}
		void run()
		{
			if (thread_state_ != thread_state_t::finalizing)
			{
				thread_state_ = thread_state_t::running;
				thread_state_changed_ = thread_number_;

				thread_alert_.notify_all();
				thread_sync_alert_.notify_all();
			}
		}
		void stop()
		{
			if (thread_state_ != thread_state_t::finalizing)
			{
				thread_state_ = thread_state_t::waiting;
				thread_state_changed_ = thread_number_;
				
				thread_alert_.notify_all();
				thread_sync_alert_.notify_all();
			}
		}
		void finalize()
		{
			if (thread_state_ != thread_state_t::finalizing)
			{
				thread_state_ = thread_state_t::finalizing;
				thread_state_changed_ = thread_number_;
				
				thread_alert_.notify_all();
				thread_sync_alert_.notify_all();
			}
		}
		int running() // Benign data races may occur
		{
			std::unique_lock<std::mutex> lock(thread_lock_, std::try_to_lock);
			return (thread_run_ + thread_sync_run_);
		}
		int ready() // Benign data races may occur
		{
			std::unique_lock<std::mutex> lock(thread_lock_, std::try_to_lock);
			return thread_ready_;
		}
		int synced() // Benign data races may occur
		{
			std::unique_lock<std::mutex> lock(thread_lock_, std::try_to_lock);
			return thread_sync_ready_;
		}
		bool available() // Benign data races may occur
		{
			std::unique_lock<std::mutex> lock(thread_lock_, std::try_to_lock);
			return (thread_ready_ != 0);
		}
		int size()
		{
			return (thread_state_ != thread_state_t::finalizing ? thread_number_ : 0);
		}

		threadpool() = delete;
		threadpool(int thread_number, thread_state thread_status = thread_state::waiting) :
			thread_state_(static_cast<thread_state_t::state>((static_cast<int>(thread_status) & 0xFFFFFFFE) == 0 ?
															 thread_status :
															 thread_state::waiting)),
			thread_state_changed_(0),
			thread_sync_executed_(false),
			thread_sync_count_(0),
			thread_number_(thread_number > 0 ? thread_number : std::thread::hardware_concurrency())
		{
			thread_array_ = new std::thread[thread_number_];
			for (int i = 0; i < thread_number_; ++i)
			{
				thread_array_[i] = std::thread(&threadpool::threadpool__, this);
			}
		}
		threadpool(threadpool const &) = delete;
		threadpool & operator=(threadpool const &) = delete;
		threadpool(threadpool &&) = delete;
		threadpool & operator=(threadpool &&) = delete;
		~threadpool()
		{
			thread_state_ = thread_state_t::terminating;
			thread_state_changed_ = thread_number_;
			
			thread_alert_.notify_all();
			thread_sync_alert_.notify_all();

			for (int i = 0; i < thread_number_; ++i)
			{
				thread_array_[i].join();
			}
			delete[] thread_array_;
		}
	private:
		std::queue<std::pair<std::function<void()> *, bool>> task_queue_;
		std::thread * thread_array_;
		std::mutex thread_lock_;
		std::condition_variable thread_alert_;
		std::condition_variable thread_sync_alert_;
		std::atomic<thread_state_t::state> thread_state_;
		std::atomic<int> thread_state_changed_;
		std::atomic<bool> thread_sync_executed_;
		std::atomic<int> thread_sync_count_;
		int thread_ready_ = 0;
		int thread_run_ = 0;
		int thread_sync_ready_ = 0;
		int thread_sync_run_ = 0;
		int const thread_number_;

		void threadpool__()
		{
			std::pair<std::function<void()> *, bool> task(nullptr, false);
			std::mutex sync_mutex;
			std::unique_lock<std::mutex> lock(thread_lock_);
			std::unique_lock<std::mutex> sync_lock(sync_mutex);

			++thread_ready_;

			while (thread_state_ != thread_state_t::terminating)
			{
				do thread_alert_.wait_for(lock, std::chrono::milliseconds(1));
				while (!thread_state_changed_ && task_queue_.empty());

				if (thread_state_changed_)
				{
					--thread_state_changed_;
				}

				while (thread_state_ != thread_state_t::terminating)
				{
					switch (thread_state_)
					{
						case thread_state_t::running:
							if (!task_queue_.empty())
							{
								if (!task.first)
								{
									task = task_queue_.front();
									task_queue_.pop();
								}
							}
							else
							{
								break;
							}

							if (task.first && !task.second)
							{
								threadrun__(task.first, lock);
							}
							else if (task.first && task.second)
							{
								threadsync__(task.first, lock, sync_lock);
							}
							else
							{
								break;
							}

							continue;
						case thread_state_t::waiting:
							if (!task_queue_.empty())
							{
								if (!task.first)
								{
									task = task_queue_.front();
									task_queue_.pop();
								}
							}
							else
							{
								break;
							}

							if (task.first && task.second)
							{
								threadsync__(task.first, lock, sync_lock);
							}
							else
							{
								break;
							}

							continue;
						case thread_state_t::finalizing:
							if (!task_queue_.empty())
							{
								if (!task.first)
								{
									task = task_queue_.front();
									task_queue_.pop();
								}
							}
							else
							{
								break;
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
						case thread_state_t::terminating:
							break;
					}
					break;
				}
			}

			--thread_ready_;
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
		void threadsync__(std::function<void()> * & task, std::unique_lock<std::mutex> & lock,
						  std::unique_lock<std::mutex> & sync_lock)
		{
			--thread_ready_;
			++thread_sync_ready_;
			lock.unlock();

			while (thread_state_ != thread_state_t::terminating)
			{
				do thread_sync_alert_.wait_for(sync_lock, std::chrono::milliseconds(1));
				while (!thread_state_changed_ && !thread_sync_executed_);

				if (thread_state_changed_)
				{
					--thread_state_changed_;
				}

				switch (thread_state_)
				{
					case thread_state_t::running:
						if (thread_sync_executed_)
						{
							if (thread_sync_count_-- == 1) // Some synchronization trickery
							{
								thread_sync_executed_ = false;
							}
							else
							{
								while (thread_sync_executed_);
							}

							(*task)();
							task = nullptr;

							lock.lock();
							--thread_sync_run_;
							
							break;
						}

					case thread_state_t::waiting:
						continue;
					case thread_state_t::finalizing:
						(*task)();
						task = nullptr;

					case thread_state_t::terminating:
						lock.lock();
						--thread_sync_ready_;

						break;
				}
				break;
			}

			++thread_ready_;
		}
	};
}

#endif//SIMPLE_THREAD_POOLS_H_
