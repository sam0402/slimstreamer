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

#include <conwrap/ProcessorAsioProxy.hpp>
#include <functional>
#include <memory>

#include "slim/ContainerBase.hpp"
#include "slim/log/log.hpp"


namespace slim
{
	template<typename StreamerType, typename CommandServerType, typename StreamingServerType, typename DiscoveryServerType, typename SchedulerType>
	class Container : public ContainerBase
	{
		public:
			Container(std::unique_ptr<StreamerType> st, std::unique_ptr<CommandServerType> cse, std::unique_ptr<StreamingServerType> sse, std::unique_ptr<DiscoveryServerType> dse, std::unique_ptr<SchedulerType> sc)
			: streamerPtr{std::move(st)}
			, commandServerPtr{std::move(cse)}
			, streamingServerPtr{std::move(sse)}
			, discoveryServerPtr{std::move(dse)}
			, schedulerPtr{std::move(sc)} {}

			// using Rule Of Zero
			virtual ~Container() = default;
			Container(const Container&) = delete;             // non-copyable
			Container& operator=(const Container&) = delete;  // non-assignable
			Container(Container&& rhs) = default;
			Container& operator=(Container&& rhs) = default;

			// virtualization is required as Processor uses std::unique_ptr<ContainerBase>
			virtual void setProcessorProxy(conwrap::ProcessorAsioProxy<ContainerBase>* p) override
			{
				ContainerBase::setProcessorProxy(p);
				streamerPtr->setProcessorProxy(p);
				commandServerPtr->setProcessorProxy(p);
				streamingServerPtr->setProcessorProxy(p);
				discoveryServerPtr->setProcessorProxy(p);
				schedulerPtr->setProcessorProxy(p);
			}

			virtual void start() override
			{
				commandServerPtr->start();
				streamingServerPtr->start();
				discoveryServerPtr->start();
				schedulerPtr->start();
			}

			virtual void stop() override
			{
				schedulerPtr->stop();
				discoveryServerPtr->stop();
				streamingServerPtr->stop();
				commandServerPtr->stop();
			}

		private:
			std::unique_ptr<StreamerType>        streamerPtr;
			std::unique_ptr<CommandServerType>   commandServerPtr;
			std::unique_ptr<StreamingServerType> streamingServerPtr;
			std::unique_ptr<DiscoveryServerType> discoveryServerPtr;
			std::unique_ptr<SchedulerType>       schedulerPtr;
	};
}
