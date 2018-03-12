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
#include <optional>
#include <string>

#include "slim/Chunk.hpp"
#include "slim/log/log.hpp"
#include "slim/util/ExpandableBuffer.hpp"
#include "slim/util/StreamBufferWithCallback.hpp"
#include "slim/wave/WAVEStream.hpp"


namespace slim
{
	namespace proto
	{
		template<typename ConnectionType>
		class StreamingSession
		{
			using StreamBuffer = util::StreamBufferWithCallback<std::function<std::streamsize(const char*, std::streamsize)>>;

			public:
				StreamingSession(ConnectionType& co, unsigned int channels, unsigned int sr, unsigned int bitePerSample)
				: connection{co}
				, streamBuffer{[&](auto* buffer, auto size) mutable
				{
					connection.write(buffer, size);
					return size;
				}}
				, samplingRate{sr}
				, waveStream{std::make_unique<std::ostream>(&streamBuffer), channels, samplingRate, bitePerSample}
				, currentChunkPtr{std::make_unique<Chunk>(buffer1, samplingRate)}
				, nextChunkPtr{std::make_unique<Chunk>(buffer2, samplingRate)}
				{
					LOG(DEBUG) << LABELS{"proto"} << "HTTP session object was created (id=" << this << ")";

					// sending HTTP response with the headers
					waveStream.write("HTTP/1.1 200 OK\r\n");
					waveStream.write("Server: SlimStreamer (");
					// TODO: provide version to the constructor
					waveStream.write(VERSION);
					waveStream.write(")\r\n");
					waveStream.write("Connection: close\r\n");
					waveStream.write("Content-Type: audio/x-wave\r\n");
					waveStream.write("\r\n");
				}

			   ~StreamingSession()
				{
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

				inline auto getSamplingRate()
				{
					return samplingRate;
				}

				inline void onChunk(Chunk chunk)
				{
					if (samplingRate == chunk.getSamplingRate())
					{
						// saving chunk in a nextChunk object and initiating asynchronious processing
						if (nextChunkPtr->getBuffer().size() > 0)
						{
							LOG(WARNING) << LABELS{"proto"} << "Chunk was skipped due to slow data transfer (client id=" << *clientID << ")";
						}
						*nextChunkPtr = chunk;

						// sending chunk asynchroniously
						sendAsync();
					}
					else
					{
						LOG(WARNING) << LABELS{"proto"} << "Skipping chunk transmition due to different sampling rate used by a client";
					}
				}

				inline void onRequest(unsigned char* buffer, std::size_t size)
				{
					// LOG(DEBUG) << LABELS{"proto"} << "HTTP onRequest";

					//for (unsigned int i = 0; i < size; i++)
					//{
					//	LOG(DEBUG) << buffer[i];
					//}

					// TODO: make more strick validation
					std::string get{"GET"};
					std::string s{(char*)buffer, get.size()};
					if (get.compare(s))
					{
						throw slim::Exception("Wrong method provided");
					}

					clientID = parseClientID({(char*)buffer, size});
					if (!clientID.has_value())
					{
						throw slim::Exception("Missing client ID in HTTP request");
					}

					LOG(INFO) << LABELS{"proto"} << "Client ID was parsed from HTTP request (clientID=" << clientID.value() << ")";
				}

			protected:
				static auto parseClientID(std::string header)
				{
					auto result{std::optional<std::string>{std::nullopt}};
					auto separator{std::string{"="}};

					auto index = header.find(separator);
					if ( index != std::string::npos)
					{
						result = std::string{header.c_str() + index + separator.length(), header.length() - index - separator.length()};
					}

					return result;
				}

				// TODO: the below implementation works however it bypasses WAVEStream, which is not good enough; proper solution should introduce async buffer intead of StreamBufferWithCallback
				void sendAsync()
				{
					// if there is an available chunk and there is no ongoing transfer
					if (nextChunkPtr->getBuffer().size() > 0 && !currentChunkPtr->getBuffer().size())
					{
						// swapping next and current chunk pointers which will allow submitting new chunks
						std::swap(nextChunkPtr, currentChunkPtr);
					}

					// initiate data transfer if there is data available
					auto& buffer{currentChunkPtr->getBuffer()};
					if (buffer.size() > 0)
					{
						// to guarantee buffer is not a dangling reference, declaring a new buffer refence in capture list
						connection.writeAsync(buffer.data(), buffer.size(), [&, &buffer = buffer](const std::error_code& error, std::size_t bytes_transferred) mutable
						{
							if (!error)
							{
								// removing transferred data from the buffer
								buffer.shrinkLeft(bytes_transferred);

								// keep sending data if there is anything to send or there is next chunk pending
								if (buffer.size() > 0 || nextChunkPtr->getBuffer().size() > 0)
								{
									sendAsync();
								}
							}
							else
							{
								// buffer has to be flushed, otherwise sendAsync will not be invoked for new chunks
								buffer.clear();

								// TODO: consider additional error processing
								LOG(ERROR) << LABELS{"proto"} << "Error while transferring data (id=" << this << ", clientID=" << clientID.value() << ", error='" << error.message() << "')";
							}
						});
					}
				}

			private:
				ConnectionType&            connection;
				std::optional<std::string> clientID;
				StreamBuffer               streamBuffer;
				unsigned int               samplingRate;
				wave::WAVEStream           waveStream;
				util::ExpandableBuffer     buffer1;
				util::ExpandableBuffer     buffer2;
				std::unique_ptr<Chunk>     currentChunkPtr;
				std::unique_ptr<Chunk>     nextChunkPtr;
		};
	}
}
