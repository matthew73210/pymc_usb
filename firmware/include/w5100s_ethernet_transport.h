// =============================================================
// w5100s_ethernet_transport.h — RAK13800/W5100S Ethernet transport
// for nRF52 pyMC_modem targets.
// =============================================================
#pragma once

#include <Arduino.h>
#include <IPAddress.h>

namespace EthernetManager {
    void begin(const char* hostname = nullptr,
               bool useStaticIP = false,
               const IPAddress& staticIP = IPAddress((uint32_t)0),
               const IPAddress& gateway = IPAddress((uint32_t)0),
               const IPAddress& subnet = IPAddress((uint32_t)0),
               const IPAddress& dns1 = IPAddress((uint32_t)0),
               const IPAddress& dns2 = IPAddress((uint32_t)0));
    void end();
    void loop();
    bool isLinkUp();
    bool hasIP();
    const char* getIPString();
}

namespace TCPServer {
    void begin(uint16_t port, const String& token);
    void loop();
    void end();
    bool isClientReady();
    void write(const uint8_t* data, size_t len);
    String getClientIP();
}
