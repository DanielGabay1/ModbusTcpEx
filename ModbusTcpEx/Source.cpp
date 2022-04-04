#include "modbus.h"

int main(int argc, char** argv)
{
    // create a modbus object
    modbus mb = modbus("127.0.0.1", 502);

    // set slave id
    mb.modbus_set_slave_id(1);

    // connect with the server
    mb.modbus_connect();

    // write single reg
    mb.modbus_write_register(0, 222);

    // close connection and free the memory
    mb.modbus_close();
    return 0;
}