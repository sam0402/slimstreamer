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

#include <algorithm>
#include <memory>
#include <vector>

#include "slim/util/buffer/HeapBuffer.hpp"


namespace slim
{
namespace util
{
namespace buffer
{

template
<
    typename ElementType
>
class PooledBufferStorage
{
    public:
        using PointerType = std::unique_ptr<ElementType[], std::function<void(ElementType*)>>;
        using SizeType    = std::size_t;

        inline explicit PooledBufferStorage(PointerType d, const SizeType& s)
        : data{std::move(d)}
        , size{s} {}

        inline PooledBufferStorage()
        : data{}
        , size{0} {}

        PointerType data;
        SizeType    size;
};

template
<
    typename ElementType,
    template<typename, template <typename> class StorageType> class BufferType = HeapBuffer
>
class BufferPool
{
    public:
        using SizeType         = typename BufferType<ElementType, DefaultHeapBufferStorage>::SizeType;
        using PooledBufferType = BufferType<ElementType, PooledBufferStorage>;

    public:
        inline explicit BufferPool(const SizeType& poolSize, const SizeType& bufferSize)
        : bufferWrappersPtr{std::make_unique<std::vector<BufferWrapper>>()}
        {
            for (SizeType i = 0; i < poolSize; i++)
            {
                bufferWrappersPtr->emplace_back(BufferWrapper{BufferType{DefaultHeapBufferStorage<ElementType>{bufferSize}}, true});
            }
        }

        inline PooledBufferType allocate()
        {
            for (SizeType i = 0; i < getSize(); i++)
            {
                auto& bufferWrapper = (*bufferWrappersPtr)[i];

                if (bufferWrapper.free)
                {
                    // creating a 'proxy' object, which wraps an existing buffer and will release it back to the pool upon destruction
                    auto bufferProxyPtr = std::unique_ptr<ElementType[], std::function<void(ElementType*)>>{bufferWrapper.buffer.getData(), [&bufferWrappers = *bufferWrappersPtr, index = i](auto* data)
                    {
                        bufferWrappers[index].free = true;
                    }};
                    auto pooledBufferStorage = PooledBufferStorage<ElementType>{std::move(bufferProxyPtr), bufferWrapper.buffer.getSize()};

                    // marking this buffer as used
                    bufferWrapper.free = false;

                    return PooledBufferType{std::move(pooledBufferStorage)};
                }
            }

            // this point is reach if no available buffer was found
            return PooledBufferType{PooledBufferStorage<ElementType>{}};
        }

        inline const auto getAvailableSize() const
        {
            // guarding against cases when BufferPool content was moved to a different object
            if (!bufferWrappersPtr)
            {
                return std::size_t{0};
            }

            // auto may not be used here as count_if returns signed value
            SizeType s = std::count_if(bufferWrappersPtr->begin(), bufferWrappersPtr->end(), [](const auto& bufferWrapper)
            {
                return bufferWrapper.free;
            });

            return s;
        }

        inline const auto getSize() const
        {
            // guarding against cases when BufferPool content was moved to a different object
            if (!bufferWrappersPtr)
            {
                return std::size_t{0};
            }

            return bufferWrappersPtr->size();
        }

    protected:
        struct BufferWrapper
        {
            BufferType<ElementType, DefaultHeapBufferStorage> buffer;
            bool                                              free;
        };

    private:
        // using std::unique_ptr<...> to make this pool movable
        std::unique_ptr<std::vector<BufferWrapper>> bufferWrappersPtr;
};

}
}
}
