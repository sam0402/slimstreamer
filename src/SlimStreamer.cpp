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

#include <chrono>
#include <conwrap/ProcessorAsio.hpp>
#include <csignal>
#include <exception>
#include <g3log/logworker.hpp>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "slim/alsa/Parameters.hpp"
#include "slim/alsa/Source.hpp"
#include "slim/Exception.hpp"
#include "slim/log/ConsoleSink.hpp"
#include "slim/log/log.hpp"
#include "slim/Pipeline.hpp"
#include "slim/Streamer.hpp"
#include "slim/wave/Destination.hpp"


static volatile bool running = true;


void signalHandler(int sig)
{
	running = false;
}


auto createPipelines()
{
	std::vector<std::tuple<unsigned int, std::string>> rates
	{
		{5512,   "hw:1,1,1"},
		{8000,   "hw:1,1,2"},
		{11025,  "hw:1,1,3"},
		{16000,  "hw:1,1,4"},
		{22050,  "hw:1,1,5"},
		{32000,  "hw:1,1,6"},
		{44100,  "hw:1,1,7"},
		{48000,  "hw:2,1,1"},
		{64000,  "hw:2,1,2"},
		{88200,  "hw:2,1,3"},
		{96000,  "hw:2,1,4"},
		{176400, "hw:2,1,5"},
		{192000, "hw:2,1,6"},
	};

	slim::alsa::Parameters      parameters{"", 3, SND_PCM_FORMAT_S32_LE, 0, 128, 0};
	std::vector<slim::Pipeline> pipelines;
	unsigned int                chunkDurationMilliSecond{100};

	for (auto& rate : rates)
	{
		auto rateValue{std::get<0>(rate)};
		auto deviceValue{std::get<1>(rate)};

		parameters.setRate(rateValue);
		parameters.setDeviceName(deviceValue);
		parameters.setFramesPerChunk((rateValue * chunkDurationMilliSecond) / 1000);

		pipelines.emplace_back(slim::alsa::Source{parameters}, slim::wave::Destination{std::to_string(std::get<0>(rate)) + ".wav", 2, std::get<0>(rate), 32});
	}

	return pipelines;
}


int main(int argc, char *argv[])
{
	// initializing log
	auto logWorkerPtr = g3::LogWorker::createLogWorker();
	g3::initializeLogging(logWorkerPtr.get());
	g3::only_change_at_initialization::setLogLevel(ERROR, true);

	// adding custom sinks
    logWorkerPtr->addSink(std::make_unique<ConsoleSink>(), &ConsoleSink::print);

    signal(SIGHUP, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGINT, signalHandler);

	try
	{
        // creating Streamer object with ALSA Parameters within Processor
		conwrap::ProcessorAsio<slim::Streamer> processorAsio{createPipelines()};

        // start streaming
        processorAsio.getResource()->start();

        // waiting for Control^C
        while(running)
        {
			std::this_thread::sleep_for(std::chrono::milliseconds{200});
        }

		// stop streaming
		processorAsio.getResource()->stop();
	}
	catch (const slim::Exception& error)
	{
		LOG(ERROR) << error;
	}
	catch (const std::exception& error)
	{
		LOG(ERROR) << error.what();
	}
	catch (...)
	{
		LOG(ERROR) << "Unknown exception";
	}

	return 0;
}
