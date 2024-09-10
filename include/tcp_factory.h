#pragma once
#include <memory>
#include <functional>

template <class Connection>
class TcpFactory
{
public:
	virtual ~TcpFactory() {}
	using ConnectionPtr = std::shared_ptr<Connection>;
	template <class... Args>
	ConnectionPtr create(Args &&...args)
	{
		ConnectionPtr conn;
		if (creator)
		{
			conn = creator();
		}
		else
		{
			conn = std::make_shared<Connection>(std::forward<Args>(args)...);
		}

		this->on_create(conn->get_cid(), conn);
		return conn;
	}

	virtual void release(ConnectionPtr conn)
	{
		if (releaser)
		{
			releaser(conn->get_cid(), conn);
		}
		else
		{
			on_release(conn->get_cid(), conn);
		}
	}


	 
	virtual void on_create(uint64_t cid, ConnectionPtr conn)
	{
		add_connection(cid, conn);
	}

	virtual void on_release(uint64_t cid, ConnectionPtr conn)
	{
		remove_connection(cid);
	}


	
	void add_connection(uint64_t cid, ConnectionPtr conn)
	{
		printf("add connection from factory %lu\n", cid); 
		connection_map[cid] = conn;
	}

	int32_t remove_connection(uint64_t  cid )
	{
		printf("remove connection from factory %lu\n", cid); 
		return connection_map.erase(cid);
	}

	ConnectionPtr find_connection(uint64_t cid)
	{
		auto itr = connection_map.find(cid);
		if (itr != connection_map.end())
		{
			return itr->second;
		}

		return nullptr;
	}

	
	std::function<ConnectionPtr()> creator;
	std::function<void(uint64_t, ConnectionPtr)> releaser;

	std::unordered_map<uint64_t, ConnectionPtr> connection_map;
};
