/* Copyright (C) 2015-2022, Wazuh Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include "eventEndpoint.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <logging/logging.hpp>

#include "defaultSettings.hpp"
#include "parseEvent.hpp"

using uvw::ErrorEvent;
using uvw::Loop;

namespace engineserver::endpoints
{

namespace
{
constexpr int BIND_SOCK_ERROR {-1};                ///< Error code for bindUnixDatagramSocket function
constexpr unsigned int MAX_MSG_SIZE {65536 + 512}; ///< Maximum message size (TODO: I think this should be 65507)

/**
 * @brief Provides a wrapper for the flooding file
 *
 * @warning this struct is not thread safe
 */
struct floodingFile
{
    // append and don't create if not exists
    const std::ios::openmode FLAGS = std::ios::out | std::ios::app | std::ios::ate; ///< Flags for the flooding file
    std::ofstream m_file; ///< File stream for the flooding file
    std::string m_error;  ///< Error message if the file is not good

    /**
     * @brief Construct a new flooding File object
     *
     * @param path (const std::string&) Path to the flooding file
     */
    floodingFile(const std::string& path)
        : m_file(path, FLAGS)
        , m_error {}
    {
        if (!m_file.good())
        {
            m_error = strerror(errno);
        }
    }

    /**
     * @brief Checks if the file is good (i.e. it is open and ready to write)
     *
     * @return true if the file is good
     * @return false otherwise
     */
    std::optional<std::string> getError() const
    {
        if (m_file.good())
        {
            return std::nullopt;
        }
        return m_error;
    }

    /**
     * @brief Writes a message to the flooding file
     * @param message (const std::string&) Message to write
     */
    bool write(const std::string& message)
    {
        if (m_file.good())
        {
            m_file << message.c_str() << std::endl;
            return true;
        }
        return false;
    }
};

} // namespace

/**
 * @brief This function opens, binds and configures a unix datagram socket.
 * @todo The code was extracted from the Wazuh source, so it must be adapted when the
 * engine is integrated to the rest of the Wazuh code to avoid code duplicity.
 * @param path (const char *) Contains the absolute path to the unix datagram socket
 * @return (int) Returns either the file descriptor value or -1
 */
static inline int bindUnixDatagramSocket(const char* path)
{
    struct sockaddr_un n_us;

    /* TODO: Check the unlink's parameter before unlinking it (to be sure that it is a
     * socket and not a regular file) */
    unlink(path);

    memset(&n_us, 0, sizeof(n_us));
    n_us.sun_family = AF_UNIX;
    strncpy(n_us.sun_path, path, sizeof(n_us.sun_path) - 1);

    const int socketFd {socket(PF_UNIX, SOCK_DGRAM, 0)};
    if (0 > socketFd)
    {
        return BIND_SOCK_ERROR;
    }

    if (bind(socketFd, (struct sockaddr*)&n_us, SUN_LEN(&n_us)) < 0)
    {
        close(socketFd);
        return BIND_SOCK_ERROR;
    }

    /* Change permissions */
    if (chmod(path, 0660) < 0)
    {
        close(socketFd);
        return BIND_SOCK_ERROR;
    }

    int len;
    socklen_t optlen {sizeof(len)};

    /* Get current maximum size */
    if (-1 == getsockopt(socketFd, SOL_SOCKET, SO_RCVBUF, (void*)&len, &optlen))
    {
        len = 0;
    }

    /* Set maximum message size */
    if (MAX_MSG_SIZE > len)
    {
        len = MAX_MSG_SIZE;
        if (setsockopt(socketFd, SOL_SOCKET, SO_RCVBUF, (const void*)&len, optlen) < 0)
        {
            close(socketFd);
            return BIND_SOCK_ERROR;
        }
    }

    // Set close-on-exec
    if (-1 == fcntl(socketFd, F_SETFD, FD_CLOEXEC))
    {
        WAZUH_LOG_ERROR("Engine event endpoints: The flag `close-on-exec` cannot be set "
                        "on datagram socket ({}): {} ({}).",
                        path,
                        strerror(errno),
                        errno);
    }

    return (socketFd);
}

EventEndpoint::EventEndpoint(const std::string& path,
                             std::shared_ptr<concurrentQueue> eventQueue,
                             std::shared_ptr<metricsManager::IMetricsScope> metricsScope,
                             std::shared_ptr<metricsManager::IMetricsScope> metricsScopeDelta)
    : BaseEndpoint {path}
    , m_eventQueue {eventQueue}
    , m_loop {Loop::getDefault()}
    , m_handle {m_loop->resource<DatagramSocketHandle>()}
    , m_spMetricsScope {metricsScope}
    , m_spMetricsScopeDelta {metricsScopeDelta}
{
    auto bytesReceivedPerSecond = m_spMetricsScopeDelta->getCounterUInteger("BytesReceivedPerSecond");
    auto eventsReceivedPerSecond = m_spMetricsScopeDelta->getCounterUInteger("EventsReceivedPerSecond");
    auto eventsReceived = m_spMetricsScope->getCounterUInteger("EventsReceived");
    auto bytesReceived = m_spMetricsScope->getCounterUInteger("BytesReceived");
    // auto queuedEvents = m_spMetricsScope->getCounterUInteger("QueuedEvents");
    // auto dumpedEvents = m_spMetricsScope->getCounterUInteger("DumpedEvents");
    // auto usedQueue = m_spMetricsScope->getGaugeDouble("UsedQueue", 0);

    m_handle->on<ErrorEvent>(
        [this](const ErrorEvent& event, DatagramSocketHandle& datagramSocketHandle)
        {
            WAZUH_LOG_ERROR("Engine event endpoints: Event error on datagram socket "
                            "({}): code=[{}]; name=[{}]; message=[{}].",
                            m_path,
                            event.code(),
                            event.name(),
                            event.what());
        });

    m_handle->on<DatagramSocketEvent>(
        [=, this](const DatagramSocketEvent& eventSocket, DatagramSocketHandle& handle)
        {
            auto strRequest = std::string {eventSocket.data.get(), eventSocket.length};

            // Received bytes per seconds
            bytesReceivedPerSecond->addValue(static_cast<uint64_t>(eventSocket.length));

            // Received events per seconds
            eventsReceivedPerSecond->addValue(1UL);

            // Total events received
            eventsReceived->addValue(1UL);

            // Total bytes received
            bytesReceived->addValue(static_cast<uint64_t>(eventSocket.length));

            // // Used Queue
            // usedQueue->setValue(m_eventQueue->size_approx());

            base::Event event;
            try
            {
                event = base::parseEvent::parseOssecEvent(strRequest);
            }
            catch (const std::exception& e)
            {
                WAZUH_LOG_WARN("Engine event endpoint: Error parsing event: '{}' (discarting...)", e.what());
                return;
            }

            m_eventQueue->push(std::move(event));
        });

    m_socketFd = bindUnixDatagramSocket(m_path.c_str());

    if (0 >= m_socketFd)
    {
        WAZUH_LOG_ERROR("Engine event endpoints: It was not possible to open the "
                        "datagram socket ({}): {} ({}).",
                        m_path,
                        strerror(errno),
                        errno);
    }
}

void EventEndpoint::configure(void)
{
    if (0 < m_socketFd)
    {
        m_handle->open(m_socketFd);
        m_handle->recv();
    }
    else
    {
        WAZUH_LOG_ERROR("Engine event endpoints: The file descriptor of the datagram "
                        "socket ({}) is invalid ({}).",
                        m_path,
                        m_socketFd);
    }
}

void EventEndpoint::run(void)
{
    // Size in bytes per second received
    m_loop->run<Loop::Mode::DEFAULT>();
}

void EventEndpoint::close(void)
{
    if (m_loop->alive())
    {
        // The loop is stoped
        m_loop->stop();
        // Every handle's closing callback is triggered
        m_loop->walk([](uvw::BaseHandle& handle) { handle.close(); });
        // The loop is run again, so every handle is able to receive its close callback
        m_loop->run();
        m_loop->clear();
        m_loop->close();
        WAZUH_LOG_INFO("Engine event endpoints: All the endpoints were closed.");
    }
    else
    {
        WAZUH_LOG_INFO("Engine event endpoints: Loop is already closed.");
    }
}

EventEndpoint::~EventEndpoint(void)
{
    close();
}

std::shared_ptr<base::queue::ConcurrentQueue<base::Event>> EventEndpoint::getEventQueue(void) const
{
    return m_eventQueue;
}

} // namespace engineserver::endpoints