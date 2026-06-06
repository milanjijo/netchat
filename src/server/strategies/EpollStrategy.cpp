#include "server/strategies/EpollStrategy.h"

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

static constexpr int MAX_EVENTS = 64;

EpollStrategy::EpollStrategy(NetworkManager* net, size_t numWorkers)
    : net_(net), numWorkers_(numWorkers) {
    epollFd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epollFd_ < 0)
        throw std::runtime_error(std::string("[EpollStrategy] epoll_create1: ") + std::strerror(errno));

    if (::pipe2(wakePipe_, O_CLOEXEC | O_NONBLOCK) < 0)
        throw std::runtime_error(std::string("[EpollStrategy] pipe2: ") + std::strerror(errno));

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = wakePipe_[0];
    ::epoll_ctl(epollFd_, EPOLL_CTL_ADD, wakePipe_[0], &ev);
}

EpollStrategy::~EpollStrategy() {
    stop();
    if (epollFd_ >= 0) {
        ::close(epollFd_);
        epollFd_ = -1;
    }
    if (wakePipe_[0] >= 0) {
        ::close(wakePipe_[0]);
        wakePipe_[0] = -1;
    }
    if (wakePipe_[1] >= 0) {
        ::close(wakePipe_[1]);
        wakePipe_[1] = -1;
    }
}

void EpollStrategy::run(int serverSocket, IOEventCallback onEvent) {
    running_ = true;
    serverSocket_ = serverSocket;
    onEvent_ = std::move(onEvent);

    std::cout << "[EpollStrategy] Starting with " << numWorkers_ << " workers\n";

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = serverSocket_;
    if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, serverSocket_, &ev) < 0)
        throw std::runtime_error(std::string("[EpollStrategy] epoll_ctl (server fd): ") + std::strerror(errno));

    workers_.reserve(numWorkers_);
    for (size_t i = 0; i < numWorkers_; ++i)
        workers_.emplace_back(&EpollStrategy::workerLoop, this);

    monitorThread_ = std::thread(&EpollStrategy::monitorLoop, this);
    monitorThread_.join();

    queueCV_.notify_all();
    for (auto& w : workers_)
        if (w.joinable()) w.join();

    std::cout << "[EpollStrategy] Stopped\n";
}

void EpollStrategy::stop() {
    if (!running_.exchange(false)) return;

    std::cout << "[EpollStrategy] Stopping...\n";
    char byte = 1;
    ::write(wakePipe_[1], &byte, 1);
    queueCV_.notify_all();
}

void EpollStrategy::addSocket(int socket) {
    ++activeConnections_;
    epollAdd(socket);
}

void EpollStrategy::removeSocket(int socket) {
    if (activeConnections_ > 0) --activeConnections_;
    epollDel(socket);
}

size_t EpollStrategy::activeConnectionCount() const {
    return activeConnections_.load();
}

std::string EpollStrategy::stats() const {
    std::ostringstream oss;
    oss << "EpollStrategy | workers=" << numWorkers_
        << " | active=" << activeConnections_.load()
        << " | totalEvents=" << totalEvents_.load();
    return oss.str();
}

void EpollStrategy::monitorLoop() {
    epoll_event events[MAX_EVENTS];

    while (running_) {
        int n = ::epoll_wait(epollFd_, events, MAX_EVENTS, -1);

        if (n < 0) {
            if (errno == EINTR) continue;
            if (running_)
                std::cerr << "[EpollStrategy] epoll_wait: " << std::strerror(errno) << "\n";
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            if (fd == wakePipe_[0]) {
                running_ = false;
                break;
            }

            if (fd == serverSocket_) {
                while (true) {
                    int clientSock = ::accept4(serverSocket_, nullptr, nullptr,
                                               SOCK_NONBLOCK | SOCK_CLOEXEC);
                    if (clientSock < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        std::cerr << "[EpollStrategy] accept4: " << std::strerror(errno) << "\n";
                        break;
                    }
                    std::cout << "[EpollStrategy] New connection fd=" << clientSock << "\n";
                    {
                        std::lock_guard<std::mutex> lk(queueMutex_);
                        workQueue_.push({clientSock, true});
                    }
                    queueCV_.notify_one();
                }
                continue;
            }

            ++totalEvents_;
            {
                std::lock_guard<std::mutex> lk(queueMutex_);
                workQueue_.push({fd, false});
            }
            queueCV_.notify_one();
        }
    }
}

void EpollStrategy::workerLoop() {
    std::cout << "[EpollStrategy] Worker " << std::this_thread::get_id() << " started\n";

    while (true) {
        WorkItem item;
        {
            std::unique_lock<std::mutex> lk(queueMutex_);
            queueCV_.wait(lk, [this] { return !workQueue_.empty() || !running_; });
            if (workQueue_.empty()) break;
            item = workQueue_.front();
            workQueue_.pop();
        }

        if (item.isHandshake) {
            onEvent_({IOEvent::Type::NewConnection, item.socket, {}});
        } else {
            std::string data = net_->receiveMessage(item.socket);
            if (data.empty()) {
                onEvent_({IOEvent::Type::Disconnected, item.socket, {}});
            } else {
                onEvent_({IOEvent::Type::DataAvailable, item.socket, std::move(data)});
                epollRearm(item.socket);
            }
        }
    }

    std::cout << "[EpollStrategy] Worker " << std::this_thread::get_id() << " stopped\n";
}

bool EpollStrategy::epollAdd(int fd) {
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.fd = fd;
    if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        std::cerr << "[EpollStrategy] epoll_ctl ADD fd=" << fd
                  << ": " << std::strerror(errno) << "\n";
        return false;
    }
    return true;
}

bool EpollStrategy::epollRearm(int fd) {
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.fd = fd;
    if (::epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
        if (errno != EBADF && errno != ENOENT)
            std::cerr << "[EpollStrategy] epoll_ctl MOD fd=" << fd
                      << ": " << std::strerror(errno) << "\n";
        return false;
    }
    return true;
}

void EpollStrategy::epollDel(int fd) {
    epoll_event ev{};
    ::epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev);
}
