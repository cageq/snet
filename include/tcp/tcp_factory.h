#pragma once
#include <memory>
#include <mutex>
#include <thread>
#include <functional>

namespace snet
{
	namespace tcp
	{
		template <class Connection>
		class TcpFactory
		{
		public:
			using ConnectionPtr = std::shared_ptr<Connection>;

			virtual ~TcpFactory() = default;

			template <class... Args>
			ConnectionPtr create(bool passive, Args &&...args)
			{
				ConnectionPtr conn;
				if (connection_creator)
				{
					conn = connection_creator();
				}
				else
				{
					conn = std::make_shared<Connection>(std::forward<Args>(args)...);
				}
				conn->is_passive = passive;

				conn->factory = this;
				this->on_create(conn->get_cid(), conn);

				add_connection(conn->get_cid(), conn);

				return conn;
			}

			virtual void release(ConnectionPtr conn)
			{
				if (connection_releaser)
				{
					connection_releaser(conn->get_cid(), conn);
				}
				else
				{
					on_release(conn->get_cid(), conn);
				}

				remove_connection(conn->get_cid());
			}

			virtual void on_create(uint64_t cid, ConnectionPtr conn)
			{
			}

			virtual void on_release(uint64_t cid, ConnectionPtr conn)
			{
			}

			void add_connection(uint64_t cid, ConnectionPtr conn)
			{
				printf("add connection from factory %lu\n", cid);
				std::lock_guard<std::mutex> guard(connection_map_mutex);
				connection_map[cid] = conn;
			}

			int32_t remove_connection(uint64_t cid)
			{
				printf("remove connection from factory %lu\n", cid);
				std::lock_guard<std::mutex> guard(connection_map_mutex);
				return connection_map.erase(cid);
			}
	 

			ConnectionPtr find_connection(uint64_t cid)
			{
				std::lock_guard<std::mutex> guard(connection_map_mutex);
				auto itr = connection_map.find(cid);
				if (itr != connection_map.end())
				{
					return itr->second;
				}

				return nullptr;
			}
 

			std::function<ConnectionPtr()> connection_creator;
			std::function<void(uint64_t, ConnectionPtr)> connection_releaser;

			std::mutex connection_map_mutex;
			std::unordered_map<uint64_t, ConnectionPtr> connection_map;
 
		};

	}
}
