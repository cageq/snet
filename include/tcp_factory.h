#pragma once 




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

		this->on_create(conn);
		return conn;
	}

	virtual void release(ConnectionPtr conn)
	{
		if (releaser)
		{
			releaser(conn);
		}
		else
		{
			on_release(conn);
		}
	}

	virtual void on_create(ConnectionPtr conn) {}
	virtual void on_release(ConnectionPtr conn) {}

	std::function<ConnectionPtr()> creator;
	std::function<void(ConnectionPtr)> releaser;
};
