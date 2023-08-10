// #include "Tintin_reporter.hpp"
#include "Matt_daemon.hpp"

// namespace fs = std::filesystem;

Matt_daemon::~Matt_daemon()
{
}

void Matt_daemon::create_socket()
{
    _socket = socket(AF_INET, SOCK_STREAM, 0);
    if (_socket == -1)
    {
        Tintin_reporter::getInstance().log_message_1("ERROR", "Matt_daemon", "socket() failed");
        throw std::runtime_error("socket() failed");
    }
}

void Matt_daemon::bind_socket()
{
    _addr.sin_family = AF_INET;
    _addr.sin_port = htons(LISTEN_PORT);
    _addr.sin_addr.s_addr = INADDR_ANY;

    // Set SO_REUSEADDR option
    int reuseAddr = 1;
    if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr)) == -1)
    {
        Tintin_reporter::getInstance().log_message_1("ERROR", "Matt_daemon", "setsockopt() failed");
        throw std::runtime_error("setsockopt() failed");
    }

    if (bind(_socket, (struct sockaddr *)&_addr, sizeof(_addr)) == -1)
    {
        Tintin_reporter::getInstance().log_message_1("ERROR", "Matt_daemon", "bind() failed");
        throw std::runtime_error("bind() failed");
    }

    if (fcntl(_socket, F_SETFL, O_NONBLOCK) == -1)
    {
        Tintin_reporter::getInstance().log_message_1("ERROR", "Matt_daemon", "fcntl() failed (O_NONBLOCK)");
        throw std::runtime_error("fcntl() failed");
    }
}

void Matt_daemon::listen_socket()
{
    if (listen(_socket, MAX_CLIENTS) == -1)
    {
        Tintin_reporter::getInstance().log_message_1("ERROR", "Matt_daemon", "listen() failed");
        throw std::runtime_error("listen() failed");
    }
}

void Matt_daemon::accept_socket()
{
    int newClient = accept(_socket, (struct sockaddr *)&_addr, &_addrlen);
    if (newClient == -1)
    {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) // check this
        {
            Tintin_reporter::getInstance().log_message_1("ERROR", "Matt_daemon", "accept() Just for check");
            throw std::runtime_error("accept() Just for check");
            return;
        }
        else
        {
            Tintin_reporter::getInstance().log_message_1("ERROR", "Matt_daemon", "accept() failed");
            throw std::runtime_error("accept() failed");
        }
    }
    _clients.push_back(newClient);
}

void Matt_daemon::read_socket(int client, int *clientCount)
{
    char buffer[1024] = {0};
    int valread = read(client, buffer, sizeof(buffer));
    if (valread == -1)
    {
        Tintin_reporter::getInstance().log_message_1("ERROR", "Matt_daemon", "read() failed");
        throw std::runtime_error("read() failed");
    }
    if (valread == 0)
    {
        for (std::vector<int>::iterator it = this->_clients.begin(); it != this->_clients.end(); it++)
        {
            if (*it == client)
            {
                this->_clients.erase(it);
                break;
            }
        }
        this->fds[*clientCount].fd = -1;
        this->shiftArray(clientCount);
        // (*clientCount)--;
        close(client);
    }
    else
    {
        std::string message = buffer;
        if (message == "quit\n" )
        {
            for(auto it = this->_clients.begin(); it != this->_clients.end(); it++)
                close(*it);
            close(this->fds[0].fd);
            Tintin_reporter::getInstance().log_message_2("INFO", "Matt_daemon", "Quitting");
            exit(1);
        }
        Tintin_reporter::getInstance().log_message_1("LOG", "Matt_daemon", "User input: " + message);
    }
}

void Matt_daemon::write_socket(int client)
{
    std::string hello = "Hello from server";
    send(client, hello.c_str(), hello.length(), 0);
}

void Matt_daemon::close_socket()
{
    close(_socket);
}

bool Matt_daemon::checkMaxClients()
{
    if (this->_clients.size() > MAX_CLIENTS)
    {
        Tintin_reporter::getInstance().log_message_1("ERROR", "Matt_daemon", "Max clients reached");
        return false;
    }
    return true;
}

void Matt_daemon::run()
{
    create_socket();
    bind_socket();
    listen_socket();

    memset(fds, 0, sizeof(fds));

    fds[0].fd = _socket;
    fds[0].events = POLLIN;

    int clientCount = 0;

    while (true)
    {
        int result = poll(fds, clientCount + 1, -1);

        // log_message_1("ERROR", "Matt_daemon", "PolaPola");
        if (result == -1)
        {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("poll() failed");
        }

        // if (checkMaxClients())
        // {
        checkMaxClients();
        if (fds[0].revents & POLLIN)
        {
            accept_socket();
            fds[++clientCount].fd = _clients.back();
            fds[clientCount].events = POLLIN;
        }


        for (int i = 1; i <= clientCount; i++)
        {
            if (fds[i].revents & POLLIN)
            {
                read_socket(fds[i].fd, &clientCount);
                // write_socket(fds[i].fd);
            }
        }

        // }
    }

    close_socket();
}