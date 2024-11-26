#pragma once
#include <set>
#include <functional>
#include <mutex>

namespace snet
{
    namespace utils
    {
        template <class T>
        class NotifyQueue
        {
        public:
            NotifyQueue() {}
            void push(const T &item)
            {
                m_mutex.lock();
                m_data.emplace(std::move(item));
                m_mutex.unlock();
            }

            bool empty()
            {
                std::lock_guard<std::mutex> guard(m_mutex);
                return m_data.empty();
            }

            bool peek()
            {
                return !m_data.empty();
            }
            template <typename Processor, typename CompleteHandler>
            void process(Processor processor, CompleteHandler completion)
            {
                do
                {
                    std::lock_guard<std::mutex> guard(m_mutex);
                    if (m_cache.empty())
                    {
                        m_data.swap(m_cache);
                    }
                } while (0);
                for (auto &item : m_cache)
                {
                    processor(item);
                }

                m_cache.clear();
                completion();
            }
            // single thread
            template <typename Processor>
            void process(Processor processor)
            {
                do
                {
                    std::lock_guard<std::mutex> guard(m_mutex);
                    if (m_cache.empty())
                    {
                        m_data.swap(m_cache);
                    }
                } while (0);

                for (auto &item : m_cache)
                {
                    processor(item);
                }
                m_cache.clear();
            }

        private:
            std::mutex m_mutex;
            std::set<T> m_data;
            std::set<T> m_cache;
        };

    }
}