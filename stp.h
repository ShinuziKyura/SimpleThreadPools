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

	template <class ReturnType>
	class task
	{
	public:
		bool running()
		{
			return running_;
		}
		bool ready()
		{
			return result_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
		}
		ReturnType result()
		{
			if (result_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			{
				throw std::runtime_error("Future not ready");
			}

			return result_.get();
		}

		task<ReturnType>() = delete;
		template <class FuncType, class ... ArgType>
		task<ReturnType>(FuncType && func, ArgType && ... args) : 
			running_(false),
			package_(std::bind(std::forward<FuncType>(func), std::forward<ArgType>(args) ...)),
			task_([this] { running_ = true; package_(); running_ = false; }),
			result_(package_.get_future())
		{
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
		std::atomic<bool> running_;
		std::packaged_task<ReturnType()> package_;
		std::function<void()> task_;
		std::future<ReturnType> result_;

		friend class threadpool;
	};

	class threadpool
	{
	public:
		template <class ReturnType>
		void new_task(task<ReturnType> & task)
		{
			if (thread_state_ != thread_state_t::finalizing)
			{
				thread_lock_.lock();

				task_queue_.emplace(&task.task_, false);

				thread_lock_.unlock();

				if (thread_state_ == thread_state_t::running)
				{
					thread_alert_.notify_one();
				}
			}
		}
		template <class ReturnType>
		void new_sync_task(task<ReturnType> & task)
		{
			if (thread_state_ != thread_state_t::finalizing)
			{
				thread_lock_.lock();

				task_queue_.emplace(&task.task_, true);

				thread_lock_.unlock();

				if (thread_state_ == thread_state_t::running)
				{
					thread_alert_.notify_one();
				}
			}			
		}
		void sync_run()
		{
			if (thread_state_ == thread_state_t::running)
			{
				if (thread_sync_executed_)
				{
					throw std::runtime_error("Threadpool synchronizing");
				}

				if (thread_sync_count_ = static_cast<size_t>(thread_sync_ready_))
				{
					thread_sync_executed_ = true;

					thread_sync_alert_.notify_all();
				}
			}
		}
		void run()
		{
			if (thread_state_ == thread_state_t::waiting)
			{
				thread_state_ = thread_state_t::running;
				thread_state_changed_ = thread_number_;

				thread_alert_.notify_all();
				thread_sync_alert_.notify_all();
			}
		}
		void stop()
		{
			if (thread_state_ == thread_state_t::running)
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
		size_t ready()
		{
			return thread_ready_;
		}
		size_t running()
		{
			return thread_running_;
		}				
		size_t sync_ready()
		{
			return thread_sync_ready_;
		}
		size_t sync_running()
		{
			return thread_sync_running_;
		}
		size_t size()
		{
			return (thread_state_ != thread_state_t::finalizing ? thread_number_ : size_t());
		}
		bool available()
		{
			return thread_ready_ != 0;
		}

		threadpool() = delete;
		threadpool(size_t thread_number, thread_state thread_status = thread_state::waiting) :
			thread_number_(thread_number > 0 ? thread_number : static_cast<size_t>(std::thread::hardware_concurrency())),
			thread_ready_(0),
			thread_running_(0),
			thread_sync_ready_(0),
			thread_sync_running_(0),
			thread_state_(static_cast<thread_state_t::state>((static_cast<int>(thread_status) & 0xFFFFFFFE) == 0 ?
				thread_status :
				thread_state::waiting)),
			thread_state_changed_(0),
			thread_sync_executed_(false),
			thread_sync_count_(0)
		{
			for (size_t n = 0; n < thread_number_; ++n)
			{
				thread_array_.emplace_back(&threadpool::threadpool__, this);
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

			for (size_t n = 0; n < thread_number_; ++n)
			{
				thread_array_[n].join();
			}
		}
	private:
		size_t const thread_number_;
		std::atomic<size_t> thread_ready_;
		std::atomic<size_t> thread_running_;
		std::atomic<size_t> thread_sync_ready_;
		std::atomic<size_t> thread_sync_running_;
		std::queue<std::pair<std::function<void()> *, bool>> task_queue_;
		std::deque<std::thread> thread_array_;
		std::mutex thread_lock_;
		std::condition_variable thread_alert_;
		std::condition_variable thread_sync_alert_;
		std::atomic<thread_state_t::state> thread_state_;
		std::atomic<size_t> thread_state_changed_;
		std::atomic<bool> thread_sync_executed_;
		std::atomic<size_t> thread_sync_count_;

		void threadpool__()
		{
			std::pair<std::function<void()> *, bool> task(nullptr, false);
			std::mutex sync_mutex;
			std::unique_lock<std::mutex> lock(thread_lock_);
			std::unique_lock<std::mutex> sync_lock(sync_mutex);

			++thread_ready_;

			while (thread_state_ != thread_state_t::terminating)
			{
				do
				{
					thread_alert_.wait_for(lock, std::chrono::milliseconds(1));
				}
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
			lock.unlock();
			--thread_ready_;
			++thread_running_;			

			(*task)();
			task = nullptr;
			
			--thread_running_;
			++thread_ready_;
			lock.lock();
		}
		void threadsync__(std::function<void()> * & task, std::unique_lock<std::mutex> & lock,
			std::unique_lock<std::mutex> & sync_lock)
		{
			lock.unlock();
			--thread_ready_;
			++thread_sync_ready_;			

			while (thread_state_ != thread_state_t::terminating)
			{
				do
				{
					thread_sync_alert_.wait_for(sync_lock, std::chrono::milliseconds(1));
				}
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

							--thread_sync_ready_;
							++thread_sync_running_;

							(*task)();
							task = nullptr;

							--thread_sync_running_;

							break;
						}

					case thread_state_t::waiting:
						continue;
					case thread_state_t::finalizing:
						(*task)();
						task = nullptr;

					case thread_state_t::terminating:						
						--thread_sync_ready_;

						break;
				}
				break;
			}

			++thread_ready_;
			lock.lock();
		}
	};
}

#endif//SIMPLE_THREAD_POOLS_H_
