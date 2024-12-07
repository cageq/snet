#pragma once
#include <cmath>
#include <functional>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>
#include <algorithm>

namespace snet
{
	namespace utils {
	struct NoneMutex
	{
		inline void lock() {}
		inline void unlock() {}
	};

	template <class T, class Compare = std::less<T>, class Mutex = NoneMutex>
	class MinHeap
	{

	public:
		explicit MinHeap(uint32_t initSize = 1024) { heap_data.reserve(initSize); }
		inline size_t size() const
		{
			std::lock_guard<Mutex> guard(heap_mutex);
			return heap_data.size();
		}
		inline bool empty() const
		{
			std::lock_guard<Mutex> guard(heap_mutex);
			return heap_data.empty();
		}
		void insert(const T &item)
		{
			std::lock_guard<Mutex> guard(heap_mutex);
			heap_data.push_back(item);
			if (heap_data.size() > 1)
			{
				std::push_heap(heap_data.begin(), heap_data.end(), Compare{});
			}
		}

		void dump(std::function<void(uint32_t, T)> output)
		{
			std::lock_guard<Mutex> guard(heap_mutex);
			uint32_t idx = 0;
			for (auto &item : heap_data)
			{
				output(idx++, item);
			}
		}

		std::tuple<bool, T> pop()
		{
			static T empty;
			std::lock_guard<Mutex> guard(heap_mutex);
			if (!heap_data.empty())
			{
				auto &top = heap_data[0];
				std::pop_heap(heap_data.begin(), heap_data.end(), Compare{});
				heap_data.pop_back();
				return std::tuple<bool, T>{true, top};
			}
			return std::tuple<bool, T>{false, empty};
		}

		// T pop() {
		//	if (!heap_data.empty()) {
		//		auto & top = heap_data[0];
		//		std::swap(heap_data[0], heap_data[heap_data.size() - 1]);
		//		heap_data.pop_back();
		//		sift_down(0);
		//		return top;
		//	}
		//	return T();
		//}

		std::tuple<bool, T> top()
		{
			static T empty;
			std::lock_guard<Mutex> guard(heap_mutex);
			if (heap_data.empty())
			{
				return std::tuple<bool, T>{false, empty};
			}
			return std::tuple<bool, T>{true, heap_data[0]};
		}

	private:
		std::vector<T> heap_data;
		Mutex heap_mutex;
	};
	}
}
