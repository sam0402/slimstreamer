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

#include "slim/log/log.hpp"
#include "slim/wave/Destination.hpp"


namespace slim
{
	namespace wave
	{
		void Destination::consume(Chunk& chunk)
		{
			auto size{chunk.getDataSize()};

			waveFile.write(chunk.getBuffer(), size);

			LOG(DEBUG) << "Written " << (size / bytesPerFrame) << " frames";
		}
	}
}
