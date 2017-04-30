// The MIT License (MIT)

// Copyright (c) 2016, Microsoft

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <iostream>

#include "QueryLogRunnerBase.h"
#include "ThreadSynchronizer.h"


namespace BitFunnel
{
    //*************************************************************************
    //
    // QueryLogRunnerBase
    //
    //*************************************************************************
    QueryLogRunnerBase::QueryLogRunnerBase(size_t queryCount)
        : m_results(queryCount, Result())
    {
    }


    void QueryLogRunnerBase::Go(size_t threadCount)
    {
        // Initialize (or cleanup from prior run).
        m_warmupSynchronizer.reset(new ThreadSynchronizer(threadCount));
        m_measureSynchronizer.reset(new ThreadSynchronizer(threadCount));
        m_finishedSynchronizer.reset(new ThreadSynchronizer(threadCount));

        m_warmupQueryCount = m_results.size();
        m_measureQueryCount = m_results.size();

        m_processors.clear();
        m_threads.clear();

        // Create the query processors.
        for (size_t i = 0; i < threadCount; ++i)
        {
            m_processors.push_back(CreateProcessor());
        }

        // Start the threads.
        for (size_t i = 0; i < threadCount; ++i)
        {
            m_threads.push_back(std::thread(QueryProcessorBase::ThreadEntryPoint,
                                            m_processors[i].get()));
        }

        // Wait for the threads to finish.
        for (auto& thread : m_threads)
        {
            thread.join();
        }

        // TODO: rework synchronizers to get a tighter timing bound.
        double elapsedTime = m_measureSynchronizer->GetElapsedTime();

        size_t failedQueryCount = 0;
        for (size_t i = 0; i < m_results.size(); ++i)
        {
            if (!m_results[i].Succeeded())
            {
                ++failedQueryCount;
            }
        }

        std::cout << std::endl;
        std::cout << "=======================================" << std::endl;
        std::cout << std::endl;

        if (failedQueryCount > 0)
        {
            std::cout << "WARNING: "
                      << failedQueryCount
                      << " queries failed."
                      << std::endl
                      << std::endl;
        }

        std::cout << "Thread count: " << threadCount << std::endl;
        std::cout << "Query count: " << m_results.size() << std::endl;
        std::cout << "Total time: " << elapsedTime << std::endl;
        std::cout << "QPS: " << m_results.size() / elapsedTime << std::endl;
    }


    void QueryLogRunnerBase::Write(std::ostream &out) const
    {
        for (size_t i = 0; i < m_results.size(); ++i)
        {
            m_results[i].Write(out);
        }
    }


    //*************************************************************************
    //
    // QueryLogRunnerBase::QueryProcessorBase
    //
    //*************************************************************************
    QueryLogRunnerBase::QueryProcessorBase::QueryProcessorBase(QueryLogRunnerBase& runner)
        : m_runnerBase(runner)
    {
    }


    void QueryLogRunnerBase::QueryProcessorBase::Entry()
    {
        // Run the batch of queries once to warm up the system.
        m_runnerBase.m_warmupSynchronizer->Wait();
        ProcessLog(m_runnerBase.m_warmupQueryCount);

        // Take performance measurements on second run.
        m_runnerBase.m_measureSynchronizer->Wait();
        ProcessLog(m_runnerBase.m_measureQueryCount);
        m_runnerBase.m_finishedSynchronizer->Wait();
    }


    void QueryLogRunnerBase::QueryProcessorBase::ThreadEntryPoint(void *data)
    {
        reinterpret_cast<QueryProcessorBase*>(data)->Entry();
    }
}