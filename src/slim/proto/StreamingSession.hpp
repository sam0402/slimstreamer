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

#include <conwrap2/ProcessorProxy.hpp>
#include <functional>
#include <memory>
#include <sstream>  // std::stringstream
#include <string>
#include <type_safe/optional.hpp>

#include "slim/Chunk.hpp"
#include "slim/EncoderBase.hpp"
#include "slim/EncoderBuilder.hpp"
#include "slim/log/log.hpp"
#include "slim/util/BigInteger.hpp"
#include "slim/util/BufferedAsyncWriter.hpp"


namespace slim
{
	namespace proto
	{
		namespace ts = type_safe;

		template<typename ConnectionType>
		class StreamingSession
		{
			public:
				StreamingSession(conwrap2::ProcessorProxy<std::unique_ptr<ContainerBase>> pp, std::reference_wrapper<ConnectionType> co, std::string id, EncoderBuilder eb)
				: processorProxy{pp}
				, connection{co}
				, clientID{id}
				, bufferedWriter{co}
				{
					LOG(DEBUG) << LABELS{"proto"} << "HTTP session object was created (id=" << this << ")";

					eb.setEncodedCallback([&](auto* data, auto size)
					{
						// do not feed writer with more data if there is no room in transfer buffer
						if (bufferedWriter.isBufferAvailable())
						{
							bufferedWriter.writeAsync(data, size, [](auto error, auto written) {});
						}
						else
						{
							LOG(WARNING) << LABELS{"proto"} << "Transfer buffer is full - skipping encoded chunk";
						}
					});
					encoderPtr = std::move(eb.build());
					encoderPtr->start();

					// creating response string
					std::stringstream ss;
					ss << "HTTP/1.1 200 OK\r\n"
					   << "Server: SlimStreamer ("
					   // TODO: provide version to the constructor
					   << VERSION
					   << ")\r\n"
					   << "Connection: close\r\n"
					   << "Content-Type: " << encoderPtr->getMIME() << "\r\n"
					   << "\r\n";

					// sending response string
					connection.get().write(ss.str());
				}

				~StreamingSession()
				{
					// canceling deferred operation
					ts::with(timer, [&](auto& timer)
					{
						timer.cancel();
					});

					LOG(DEBUG) << LABELS{"proto"} << "HTTP session object was deleted (id=" << this << ")";
				}

				StreamingSession(const StreamingSession&) = delete;             // non-copyable
				StreamingSession& operator=(const StreamingSession&) = delete;  // non-assignable
				StreamingSession(StreamingSession&& rhs) = delete;              // non-movable
				StreamingSession& operator=(StreamingSession&& rhs) = delete;   // non-movable-assignable

				inline auto getClientID()
				{
					return clientID;
				}

				inline auto getFramesProvided()
				{
					return framesProvided;
				}

				inline bool isRunning()
				{
					return running;
				}

				inline void onRequest(unsigned char* data, std::size_t size)
				{
					if (!running)
					{
						return;
					}

					// TODO: make more strick validation
					std::string get{"GET"};
					std::string s{(char*)data, get.size()};
					if (get.compare(s))
					{
						// stopping this session due an error
						LOG(WARNING) << LABELS{"proto"} << "Wrong HTTP method provided";
						flush([&]
						{
							connection.get().stop();
						});
					}
				}

				static auto parseClientID(std::string header)
				{
					auto result{ts::optional<std::string>{ts::nullopt}};
					auto separator{std::string{"="}};
					auto index{header.find(separator)};

					if (index != std::string::npos)
					{
						result = std::string{header.c_str() + index + separator.length(), header.length() - index - separator.length()};
					}

					return result;
				}

				inline void start()
				{
					running = true;
				}

				template <typename CallbackType>
				inline void stop(CallbackType callback)
				{
					if (!running)
					{
						callback();
						return;
					}

					flush([&, callback = std::move(callback)]
					{
						running = false;
						callback();
					});
				}

				inline void streamChunk(const Chunk& chunk)
				{
					if (!running)
					{
						return;
					}

					// stopping this session in case sampling rates do not match
					if (chunk.getSamplingRate() != encoderPtr->getSamplingRate())
					{
						flush([&]
						{
							LOG(ERROR) << LABELS{"proto"} << "Closing HTTP connection due to different sampling rate used by a client (session rate=" << encoderPtr->getSamplingRate() << "; data rate=" << chunk.getSamplingRate() << ")";
							connection.get().stop();
						});
						return;
					}

					encoderPtr->encode(chunk.getData(), chunk.getSize());
					framesProvided += chunk.getFrames();

					if (chunk.isEndOfStream())
					{
						// stopping this session
						flush([&]
						{
							connection.get().stop();
						});
					}
				}

			protected:
				template <typename CallbackType>
				void flush(CallbackType callback)
				{
					if (encoderPtr->isRunning())
					{
						encoderPtr->stop([&, callback = std::move(callback)]
						{
							flush(std::move(callback));
						});
					}
					else
					{
						if (bufferedWriter.isBufferAvailable())
						{
							// submitting an 'empty' chunk so that a callback gets invoked when all data has been transferred
							bufferedWriter.writeAsync(nullptr, 0, [callback = std::move(callback)](auto error, auto written)
							{
								callback();
							});
						}
						else
						{
							// waiting until data is transferred so a new 'empty' chunk can be submitted
							timer = ts::ref(processorProxy.processWithDelay([&, callback = std::move(callback)]
							{
								flush(std::move(callback));
							}, std::chrono::milliseconds{1}));
						}
					}
				}

			private:
				conwrap2::ProcessorProxy<std::unique_ptr<ContainerBase>> processorProxy;
				std::reference_wrapper<ConnectionType>                   connection;
				std::string                                              clientID;
				// TODO: parameterize
				util::BufferedAsyncWriter<ConnectionType, 128>           bufferedWriter;
				std::unique_ptr<EncoderBase>                             encoderPtr;
				bool                                                     running{false};
				ts::optional_ref<conwrap2::Timer>                        timer{ts::nullopt};
				util::BigInteger                                         framesProvided{0};
		};
	}
}
