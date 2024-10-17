#pragma once

#include "tcp_connector.h"
#include "tcp_connection.h"
#include "tcp_listener.h"

#include "pipe_connection.h"
#include "pipe_factory.h"
#include "pipe_session.h"
#include <memory>
#include <unistd.h>
#include <unordered_map>

namespace snet
{

	template <class UserSession>
	class TcpPipe
	{

	public:
		using PipeTcpConnection = PipeConnection<UserSession>;

		using UserSessionPtr = std::shared_ptr<UserSession>;

		TcpPipe() : tcp_client(&pipe_factory), tcp_server(&pipe_factory) {}

		void start(const std::string &host = "0.0.0.0", uint16_t port = 9999, PipeMode pipeMode = PIPE_DUET_MODE)
		{
			pipe_mode = pipeMode;

			if ((pipe_mode & PIPE_SERVER_MODE) && port > 0)
			{
				tcp_server.start(port);
			}

			if (pipe_mode & PIPE_CLIENT_MODE)
			{
				tcp_client.start();
			}
		}

		template <class... Args>
		UserSessionPtr add_remote_pipe(const std::string &pipeId,
									   const std::string &host, uint16_t port,
									   Args &&...args)
		{
			auto session = std::make_shared<UserSession>(std::forward<Args>(args)...);
			session->pipe_id = pipeId;
			pipe_factory.add_session(pipeId, session);
			auto conn = tcp_client.connect(host, port, session, &pipe_factory);
			return session;
		}

		template <class... Args>
		UserSessionPtr add_local_pipe(const std::string &pipeId, Args &&...args)
		{
			auto session = std::make_shared<UserSession>(std::forward<Args>(args)...);
			session->pipe_id = pipeId;
			pipe_factory.add_session(pipeId, session);
			return session;
		}

		void attach(UserSessionPtr pipe, const std::string &host = "",
					uint16_t port = 0)
		{
			if (pipe)
			{
			}
		}

	private:
		PipeFactory<UserSession> pipe_factory;
		TcpConnector<PipeTcpConnection> tcp_client;
		TcpListener<PipeTcpConnection> tcp_server;
		PipeMode pipe_mode;
	};

}