#define NTDDI_VERSION NTDDI_VISTA
#define WINVER _WIN32_WINNT_VISTA
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <cstdio>
#include <Windows.h>
#include <WinUser.h>
#include <ws2def.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <netioapi.h>

#include <fstream>
#include <vector>

#include <iostream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

BOOL is_control_down = FALSE;

ULONG interfaceMetric;
NET_IFINDEX interfaceIndex;
ULONG64 interfaceLuid;

std::vector<DWORD> addresses;
int currentGateway;

MIB_IPFORWARDTABLE *getForwardTable()
{
    MIB_IPFORWARDTABLE *table = NULL;
    ULONG buffer_size = 0;

    DWORD outcome = GetIpForwardTable(table, &buffer_size, FALSE);
    if (outcome == ERROR_INSUFFICIENT_BUFFER)
    {
        table = (MIB_IPFORWARDTABLE *)malloc(buffer_size);
        outcome = GetIpForwardTable(table, &buffer_size, FALSE);
    }

    if (outcome != NO_ERROR)
    {
        if (table)
            free(table);

        fprintf(stderr, "Failed to fetch forward table.");
        ExitProcess(-1);
        return NULL;
    }

    return table;
}

IP_ADAPTER_ADDRESSES *getAdapters()
{
    IP_ADAPTER_ADDRESSES *adapters = NULL;
    ULONG buffer_size = 0;

    DWORD outcome = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_DNS_SERVER, NULL, adapters, &buffer_size);
    if (outcome == ERROR_BUFFER_OVERFLOW)
    {
        adapters = (IP_ADAPTER_ADDRESSES *)malloc(buffer_size);
        outcome = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_DNS_SERVER, NULL, adapters, &buffer_size);
    }

    if (outcome != NO_ERROR)
    {
        if (adapters)
            free(adapters);

        fprintf(stderr, "Failed to fetch nework adapters informations.");
        exit(-1);
        return NULL;
    }

    return adapters;
}

MIB_IPADDRTABLE *getAddrTable()
{
    MIB_IPADDRTABLE *table = NULL;
    ULONG buffer_size = 0;

    DWORD outcome = GetIpAddrTable(table, &buffer_size, FALSE);
    if (outcome == ERROR_INSUFFICIENT_BUFFER)
    {
        table = (MIB_IPADDRTABLE *)malloc(buffer_size);
        outcome = GetIpAddrTable(table, &buffer_size, FALSE);
    }

    if (outcome != NO_ERROR)
    {
        if (table)
            free(table);

        fprintf(stderr, "Failed to fetch the interface-to-IPv4 mapping table.");
        exit(-1);
        return NULL;
    }

    return table;
}

ULONG getMetric(DWORD ifIndex)
{
    MIB_IPINTERFACE_ROW row;
    row.InterfaceIndex = ifIndex;

    DWORD outcome = GetIpInterfaceEntry(&row);
    if (outcome != NO_ERROR)
    {
        fprintf(stderr, "Failed to fetch network interface.");
        exit(-1);
        return 0;
    }

    return row.Metric;
}

std::vector<DWORD> getGateways(NET_IFINDEX ifIndex)
{
    std::vector<DWORD> gateways;

    auto *table = getForwardTable();
    for (DWORD i = 0; i < table->dwNumEntries; i++)
    {
        auto *row = &table->table[i];
        if (row->dwForwardIfIndex == ifIndex && row->dwForwardDest == 0)
        {
            gateways.push_back(row->dwForwardNextHop);
        }
    }
    free(table);

    return gateways;
}

MIB_IPINTERFACE_ROW getInterface(ULONG64 luid)
{
    MIB_IPINTERFACE_ROW inf;
    inf.Family = AF_INET;
    inf.InterfaceLuid.Value = luid;

    DWORD outcome = GetIpInterfaceEntry(&inf);
    if (outcome != NO_ERROR)
    {
        fprintf(stderr, "Couldn't find any interface with Luid %llu", luid);
        exit(-1);
        return inf;
    }

    return inf;
}

char *addressToString(DWORD address)
{
    char *str = (char *)malloc(16); // 000.000.000.000\0
    if (!inet_ntop(AF_INET, &address, str, 16))
    {
        fprintf(stderr, "Failed to convert %lu to an IPv4 ip", address);
        exit(-1);
        return NULL;
    }
    return str;
}

void deleteGateways(NET_IFINDEX ifIndex)
{
    auto *table = getForwardTable();
    for (DWORD i = 0; i < table->dwNumEntries; i++)
    {
        auto *row = &table->table[i];
        if (row->dwForwardIfIndex == ifIndex && row->dwForwardDest == 0)
        {
            DeleteIpForwardEntry(row);
        }
    }
    free(table);
}

void addGateway(NET_IFINDEX ifIndex, ULONG ifMetric, ULONG metric, DWORD gateway)
{
    auto *table = getForwardTable();

    MIB_IPFORWARDROW row;
    row.dwForwardProto = MIB_IPPROTO_NETMGMT; // Static route
    row.dwForwardDest = 0;                    // Default route; 0.0.0.0
    row.dwForwardMask = 0;                    // Subnet mask; 0.0.0.0
    row.dwForwardNextHop = gateway;           // The new gateway.
    row.dwForwardIfIndex = ifIndex;           // Interface index
    row.dwForwardMetric1 = ifMetric + metric;

    DWORD outcome = CreateIpForwardEntry(&row);

    if (outcome != NO_ERROR)
    {
        if (table)
            free(table);

        fprintf(stderr, "Error adding new forward entry.");
        exit(-1);
        return;
    }

    free(table);
}

int readConfig()
{
    std::ifstream config_file("config.json", std::ifstream::binary);

    if (!config_file.good())
        return FALSE;

    json j = json::parse(config_file);

    interfaceLuid = j["interfaceLuid"].get<ULONG64>();

    auto gateways = j["gateways"];
    for (auto it = gateways.begin(); it < gateways.end(); ++it)
    {
        std::string gateway = it.value().get<std::string>();

        DWORD address;
        if (inet_pton(AF_INET, gateway.c_str(), &address))
        {
            addresses.push_back(address);
        }
    }

    return TRUE;
}

LRESULT CALLBACK keyboard_callback(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION)
    {
        KBDLLHOOKSTRUCT *key_info = (KBDLLHOOKSTRUCT *)lParam;
        switch (wParam)
        {
        case WM_KEYDOWN:
            is_control_down |= key_info->vkCode == VK_CONTROL ||
                               key_info->vkCode == VK_LCONTROL ||
                               key_info->vkCode == VK_RCONTROL;

            if (is_control_down && key_info->vkCode == VK_F11)
            {
                currentGateway = (currentGateway + 1) % addresses.size();
                deleteGateways(interfaceIndex);
                auto gateway = addresses[currentGateway];
                addGateway(interfaceIndex, interfaceMetric, 1, gateway);

                auto ip = addressToString(gateway);
                printf("New gateway: %s\n", ip);
                free(ip);
            }
            break;

        case WM_KEYUP:
            is_control_down &= key_info->vkCode != VK_CONTROL &&
                               key_info->vkCode != VK_LCONTROL &&
                               key_info->vkCode != VK_RCONTROL;

            if (is_control_down && key_info->vkCode == VK_F12)
            {
                auto ip = addressToString(addresses[currentGateway]);
                printf("Current gateway: %s\n", ip);
                free(ip);
            }
            break;
        }
    }

    return CallNextHookEx(NULL, code, wParam, lParam);
}

HOOKPROC proc = keyboard_callback;

int run()
{
    BOOL outcome;
    MSG msg;
    while ((outcome = GetMessageA(&msg, NULL, 0, 0)))
    {
        if (outcome < 0)
            return outcome;

        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (!readConfig())
    {
        fprintf(stderr, "Couldn't open config.json file");
        return -1;
    }

    {
        auto inf = getInterface(interfaceLuid);
        interfaceIndex = inf.InterfaceIndex;
        interfaceMetric = inf.Metric;
    }

    {
        auto currentGateways = getGateways(interfaceIndex);

        currentGateway = -1;
        for (DWORD cGateway : currentGateways)
        {
            for (std::size_t i = 0; i < addresses.size(); ++i)
            {
                if (cGateway == addresses[i])
                {
                    currentGateway = i;
                    break;
                }
            }
        }

        if (currentGateway == -1)
        {
            deleteGateways(interfaceIndex);
            addGateway(interfaceIndex, interfaceMetric, 1, addresses[0]);
            currentGateway = 1;
        }
    }

    SetWindowsHookExA(WH_KEYBOARD_LL, proc, NULL, 0);
    return run();
}