/*
 * Copyright 2017, Andrej Kislovskij
 *
 * This is PUBLIC DOMAIN software so use at your own risk as it comes
 * with no warranties. This code is yours to share, use and modify without
 * any restrictions or obligations.
 *
 * For more information see conwrap/LICENSE or refer refer to http://unlicense.org
 *
 * Author: gimesketvirtadieni at gmail dot com (Andrej Kislovskij)
 */

#pragma once

#include <functional>
#include <iostream>

#include "slim/log/log.hpp"
#include "slim/util/OutputStreamCallback.hpp"
#include "slim/wave/WAVEStream.hpp"


namespace slim
{
	namespace proto
	{
		template<typename ConnectionType>
		class StreamingSession
		{
			using OutputStreamCallback = util::OutputStreamCallback<std::function<std::streamsize(const char*, std::streamsize)>>;

			public:
				StreamingSession(ConnectionType& c)
				: connection(c)
				, outputStreamCallback{[&](auto* buffer, auto size) mutable
				{
					// TODO: work in progress
					connection.getNativeSocket();
					return 1;
				}}
				, waveStream{std::make_unique<std::ostream>(&outputStreamCallback), 2, 44100, 32}
				{
					LOG(INFO) << "HTTP session created";

					// TODO: work in progress
					waveStream.writeHeader();
				}

				~StreamingSession()
				{
					LOG(INFO) << "HTTP session deleted";
				}

				StreamingSession(const StreamingSession&) = delete;             // non-copyable
				StreamingSession& operator=(const StreamingSession&) = delete;  // non-assignable
				StreamingSession(StreamingSession&& rhs) = delete;              // non-movable
				StreamingSession& operator=(StreamingSession&& rhs) = delete;   // non-movable-assignable

				inline auto& getConnection()
				{
					return connection;
				}

				void onData(unsigned char* buffer, std::size_t receivedSize)
				{
					LOG(DEBUG) << "HTTP onData";

					for (unsigned int ii = 0; ii < receivedSize; ii++)
					{
						LOG(DEBUG) << buffer[ii];
					}
				}

			private:
				ConnectionType&      connection;
				OutputStreamCallback outputStreamCallback;
				wave::WAVEStream     waveStream;
		};
	}
}
