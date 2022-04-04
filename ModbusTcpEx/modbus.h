#pragma once
#ifndef MODBUSPP_MODBUS_H
#define MODBUSPP_MODBUS_H

#include <cstring>
#include <stdint.h>
#include <string>

#ifdef ENABLE_MODBUSPP_LOGGING
#include <cstdio>
#define LOG(fmt, ...) printf("[ modbuspp ]" fmt, ##__VA_ARGS__)
#else
#define LOG(...) (void)0
#endif

#ifdef _WIN32
 // WINDOWS socket
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
using X_SOCKET = SOCKET;
using ssize_t = int;

#define X_ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
#define X_CLOSE_SOCKET(s) closesocket(s)
#define X_ISCONNECTSUCCEED(s) ((s) != SOCKET_ERROR)

#else
 // Berkeley socket
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
using X_SOCKET = int;

#define X_ISVALIDSOCKET(s) ((s) >= 0)
#define X_CLOSE_SOCKET(s) close(s)
#define X_ISCONNECTSUCCEED(s) ((s) >= 0)
#endif

using SOCKADDR = struct sockaddr;
using SOCKADDR_IN = struct sockaddr_in;

#define MAX_MSG_LENGTH 260

///Function Code
#define READ_COILS 0x01
#define READ_INPUT_BITS 0x02
#define READ_REGS 0x03
#define READ_INPUT_REGS 0x04
#define WRITE_COIL 0x05
#define WRITE_REG 0x06
#define WRITE_COILS 0x0F
#define WRITE_REGS 0x10


/// Modbus Operator Class
class modbus
{

public:
    bool err{};
    int err_no{};
    std::string error_msg;

    modbus(std::string host, uint16_t port);
    ~modbus();

    bool modbus_connect();
    void modbus_close() const;
    bool is_connected() const { return _connected; }
    void modbus_set_slave_id(int id);
    int modbus_write_register(uint16_t address, const uint16_t& value);

private:
    bool _connected{};
    uint16_t PORT{};
    uint32_t _msg_id{};
    int _slaveid{};
    std::string HOST;

    X_SOCKET _socket{};
    SOCKADDR_IN _server{};

#ifdef _WIN32
    WSADATA wsadata;
#endif

    void modbus_build_request(uint8_t* to_send, uint16_t address, int func) const;
    int modbus_write(uint16_t address, uint16_t amount, int func, const uint16_t* value);
    ssize_t modbus_send(uint8_t* to_send, size_t length);
};

/**
 * Main Constructor of Modbus Connector Object
 * @param host IP Address of Host
 * @param port Port for the TCP Connection
 * @return     A Modbus Connector Object
 */
inline modbus::modbus(std::string host, uint16_t port = 502)
{
    HOST = host;
    PORT = port;
    _slaveid = 1;
    _msg_id = 1;
    _connected = false;
    err = false;
    err_no = 0;
    error_msg = "";
}

/**
 * Destructor of Modbus Connector Object
 */
inline modbus::~modbus(void) = default;

/**
 * Modbus Slave ID Setter
 * @param id  ID of the Modbus Server Slave
 */
inline void modbus::modbus_set_slave_id(int id)
{
    _slaveid = id;
}

/**
 * Build up a Modbus/TCP Connection
 * @return   If A Connection Is Successfully Built
 */
inline bool modbus::modbus_connect()
{
    if (HOST.empty() || PORT == 0)
    {
        LOG("Missing Host and Port");
        return false;
    }
    else
    {
        LOG("Found Proper Host %s and Port %d", HOST.c_str(), PORT);
    }

#ifdef _WIN32
    if (WSAStartup(0x0202, &wsadata))
    {
        return false;
    }
#endif

    _socket = socket(AF_INET, SOCK_STREAM, 0);
    if (!X_ISVALIDSOCKET(_socket))
    {
        LOG("Error Opening Socket");
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }
    else
    {
        LOG("Socket Opened Successfully");
    }

#ifdef WIN32
    const DWORD timeout = 20;
#else
    struct timeval timeout
    {
    };
    timeout.tv_sec = 20; // after 20 seconds connect() will timeout
    timeout.tv_usec = 0;
#endif

    setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    _server.sin_family = AF_INET;
    _server.sin_addr.s_addr = inet_addr(HOST.c_str());
    _server.sin_port = htons(PORT);

    if (!X_ISCONNECTSUCCEED(connect(_socket, (SOCKADDR*)&_server, sizeof(_server))))
    {
        LOG("Connection Error");
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }

    LOG("Connected");
    _connected = true;
    return true;
}

/**
 * Close the Modbus/TCP Connection
 */
inline void modbus::modbus_close() const
{
    X_CLOSE_SOCKET(_socket);
#ifdef _WIN32
    WSACleanup();
#endif
    LOG("Socket Closed");
}

/**
 * Modbus Request Builder
 * @param to_send   Message Buffer to Be Sent
 * @param address   Reference Address
 * @param func      Modbus Functional Code
 */
inline void modbus::modbus_build_request(uint8_t * to_send, uint16_t address, int func) const
{
    to_send[0] = (uint8_t)(_msg_id >> 8u);
    to_send[1] = (uint8_t)(_msg_id & 0x00FFu);
    to_send[2] = 0;
    to_send[3] = 0;
    to_send[4] = 0;
    to_send[6] = (uint8_t)_slaveid;
    to_send[7] = (uint8_t)func;
    to_send[8] = (uint8_t)(address >> 8u);
    to_send[9] = (uint8_t)(address & 0x00FFu);
}


inline int modbus::modbus_write(uint16_t address, uint16_t amount, int func, const uint16_t * value)
{
    int status = 0;
    uint8_t* to_send;
    if (func == WRITE_COIL || func == WRITE_REG)
    {
        to_send = new uint8_t[12];
        modbus_build_request(to_send, address, func);
        to_send[5] = 6;
        to_send[10] = (uint8_t)(value[0] >> 8u);
        to_send[11] = (uint8_t)(value[0] & 0x00FFu);
        status = modbus_send(to_send, 12);
    }
    else if (func == WRITE_REGS)
    {
        to_send = new uint8_t[13 + 2 * amount];
        modbus_build_request(to_send, address, func);
        to_send[5] = (uint8_t)(7 + 2 * amount);
        to_send[10] = (uint8_t)(amount >> 8u);
        to_send[11] = (uint8_t)(amount & 0x00FFu);
        to_send[12] = (uint8_t)(2 * amount);
        for (int i = 0; i < amount; i++)
        {
            to_send[13 + 2 * i] = (uint8_t)(value[i] >> 8u);
            to_send[14 + 2 * i] = (uint8_t)(value[i] & 0x00FFu);
        }
        status = modbus_send(to_send, 13 + 2 * amount);
    }
    else if (func == WRITE_COILS)
    {
        to_send = new uint8_t[14 + (amount - 1) / 8];
        modbus_build_request(to_send, address, func);
        to_send[5] = (uint8_t)(7 + (amount + 7) / 8);
        to_send[10] = (uint8_t)(amount >> 8u);
        to_send[11] = (uint8_t)(amount & 0x00FFu);
        to_send[12] = (uint8_t)((amount + 7) / 8);
        for (int i = 0; i < (amount + 7) / 8; i++)
            to_send[13 + i] = 0; // init needed before summing!
        for (int i = 0; i < amount; i++)
        {
            to_send[13 + i / 8] += (uint8_t)(value[i] << (i % 8u));
        }
        status = modbus_send(to_send, 14 + (amount - 1) / 8);
    }
    //delete[] to_send;
    return status;
}

/**
 * Write Single Register
 * FUCTION 0x06
 * @param address   Reference Address
 * @param value     Value to Be Written to Register
 */
inline int modbus::modbus_write_register(uint16_t address, const uint16_t& value)
{
    if (_connected)
        modbus_write(address, 1, WRITE_REG, &value);

    return 0;
}


/**
 * Data Sender
 * @param to_send Request to Be Sent to Server
 * @param length  Length of the Request
 * @return        Size of the request
 */
inline ssize_t modbus::modbus_send(uint8_t * to_send, size_t length)
{
    _msg_id++;
    return send(_socket, (const char*)to_send, (size_t)length, 0);
}


#endif //MODBUSPP_MODBUS_H
