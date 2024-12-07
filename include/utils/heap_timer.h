#pragma once
#include "min_heap.h"
#include <functional>
#include <unordered_map>
#include <time.h>
#include <chrono>
#include <thread>
#include <memory>

namespace snet
{

	namespace utils
	{
		// using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

		template <class T>
		struct TimeUnit
		{
			constexpr static const char *short_notion = "";
		};

		template <>
		struct TimeUnit<std::chrono::microseconds>
		{
			constexpr static const char *short_notion = "ms";
		};

		template <>
		struct TimeUnit<std::chrono::milliseconds>
		{
			constexpr static const char *short_notion = "us";
		};

		template <>
		struct TimeUnit<std::chrono::seconds>
		{
			constexpr static const char *short_notion = "s";
		};

		template <class TimeScale>
		inline static TimeScale get_now()
		{
			  auto now = std::chrono::system_clock::now();
    		return std::chrono::duration_cast<TimeScale>(now.time_since_epoch());
		}

		using TimerHandler = std::function<bool()>;

		template <class TimeScale>
		struct TimerNode
		{
			using TimerNodePtr = std::shared_ptr<TimerNode<TimeScale>>;

			TimerNode() {}
			TimerNode(int32_t t, const TimerHandler &h, bool l = true)
			{
				handler = h;
				interval = t;
				loop = l;
				expire_time = get_now<TimeScale>() + TimeScale(t);
			}

			uint64_t  timer_id = 0;
			TimerHandler handler = nullptr;
			int32_t interval = 0;
			bool loop = true;
			bool stopped = false;
			TimeScale expire_time;
		};

		template <class TimeScale>
		struct CompareTimerNode
		{
			bool operator()(const typename TimerNode<TimeScale>::TimerNodePtr &node, const typename TimerNode<TimeScale>::TimerNodePtr &other)
			{
				return node->expire_time > other->expire_time;
			}
		};

		template <class TimeScale = std::chrono::microseconds, class Mutex = std::mutex>
		class HeapTimer
		{

		public:
			using TimerNodePtr = typename TimerNode<TimeScale>::TimerNodePtr;
			HeapTimer()
			{
				timer_start_point = std::chrono::system_clock::now();
			}

			uint32_t start_timer(const TimerHandler &handler, uint32_t interval, bool loop = true)
			{
				auto node = std::make_shared<TimerNode<TimeScale>>(interval, handler, loop);
				return add_timer(node);
			}

			bool stop_timer(uint32_t timerId)
			{
				std::lock_guard<Mutex> guard(timer_mutex);
				auto itr = timer_nodes.find(timerId);
				if (itr != timer_nodes.end())
				{
					itr->second->stopped = true;
					timer_nodes.erase(itr);
					return true;
				}
				return false;
			}

			bool restart_timer(uint32_t timerId, uint32_t interval = 0)
			{
				std::lock_guard<Mutex> guard(timer_mutex);
				auto itr = timer_nodes.find(timerId);
				if (itr != timer_nodes.end())
				{
					itr->second->stopped = true;
					auto node = std::make_shared<TimerNode<TimeScale>>(interval > 0 ? interval : itr->second->interval, itr->second->handler, itr->second->loop);
					timer_nodes.erase(itr);
					heap_tree.insert(node);
					timer_nodes[node->timer_id] = node;
					return true;
				}
				return false;
			}

			void handle_timeout(TimerNodePtr node)
			{
				bool rst = node->handler();
				if (rst)
				{
					if (node->loop && !node->stopped)
					{
						node->expire_time = get_now<TimeScale>() + TimeScale(node->interval);
						heap_tree.insert(node);
					}
				}
				else
				{
					std::lock_guard<Mutex> guard(timer_mutex);
					node->stopped = true;
					timer_nodes.erase(node->timer_id);
				}
			}

			// return microseconds
			TimeScale timer_loop()
			{
				auto cur = get_now<TimeScale>();
				bool hasTop = false;
				TimerNodePtr node = nullptr;
				std::tie(hasTop, node) = heap_tree.top();
				while (hasTop && node->expire_time <= cur)
				{
					heap_tree.pop();
					if (!node->stopped)
					{
						handle_timeout(node);
						// heap_tree.dump([this](uint32_t idx, TimerNodePtr node ){
						// 		printf("[%u, %u, %llu%s] ",idx, node->timer_id,
						// 				std::chrono::duration_cast<std::chrono::microseconds>( node->expire_time - timer_start_point  ).count(),
						// 				TimeUnit<TimeScale>::short_notion  );
						// 		});
						// printf("\n");
					}
					std::tie(hasTop, node) = heap_tree.top();
					cur = get_now<TimeScale>();
				}

				if (node)
				{
					if (node->expire_time > cur)
					{
						auto nextExpire = node->expire_time - cur;


						if ( node->expire_time  > cur){

							return node->expire_time - cur;
						}

						return TimeScale(1);

 
					}
					else
					{
						timer_loop();
					}
				}
				else
				{
					// std::this_thread::sleep_for(TimeScale(1));
				}
				return TimeScale{1};  
			}

		private:
			static const uint32_t base_timer_index = 1024;
			uint32_t add_timer(TimerNodePtr node)
			{
				#if __cplusplus == 201103L
					static uint64_t timer_index = base_timer_index;
				#else 
					static std::atomic_uint64_t timer_index = base_timer_index;
				#endif // __cplusplus > 201103L
				node->timer_id = timer_index++;
				heap_tree.insert(node);

				std::lock_guard<Mutex> guard(timer_mutex);
				timer_nodes[node->timer_id] = node;
				return node->timer_id;
			}

			std::chrono::time_point<std::chrono::system_clock> timer_start_point;
			MinHeap<TimerNodePtr, CompareTimerNode<TimeScale>, Mutex> heap_tree;
			Mutex timer_mutex;
			std::unordered_map<uint32_t, TimerNodePtr> timer_nodes;
		};

	}
}