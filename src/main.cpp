#define NTDDI_VERSION NTDDI_VISTA
#define WINVER _WIN32_WINNT_VISTA
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "GatewayHelper.hpp"

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


void AddressToString(DWORD address, PSTR str)
{
    if (!inet_ntop(AF_INET, &address, str, 16))
    {
        fprintf(stderr, "Failed to convert %lu to an IPv4 ip", address);
        exit(-1);
    }
}

int ReadConfig()
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
                GatewayHelper::DeleteGateways(interfaceIndex);
                auto gateway = addresses[currentGateway];
                GatewayHelper::AddGateway(interfaceIndex, interfaceMetric, 1, gateway);

                auto ip = new char[16];
                AddressToString(gateway, ip);
                printf("New gateway: %s\n", ip);
                delete[] ip;
            }
            break;

        case WM_KEYUP:
            is_control_down &= key_info->vkCode != VK_CONTROL &&
                               key_info->vkCode != VK_LCONTROL &&
                               key_info->vkCode != VK_RCONTROL;

            if (is_control_down && key_info->vkCode == VK_F12)
            {
                auto ip = new char[16];
                AddressToString(addresses[currentGateway], ip);
                printf("Current gateway: %s\n", ip);
                delete[] ip;
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
    if (!ReadConfig())
    {
        fprintf(stderr, "Couldn't open config.json file");
        return -1;
    }

    {
        auto inf = GatewayHelper::GetInterface(interfaceLuid);
        interfaceIndex = inf.InterfaceIndex;
        interfaceMetric = inf.Metric;
    }

    {
        auto currentGateways = GatewayHelper::GetGateways(interfaceIndex);

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
            GatewayHelper::DeleteGateways(interfaceIndex);
            GatewayHelper::AddGateway(interfaceIndex, interfaceMetric, 1, addresses[0]);
            currentGateway = 1;
        }
    }

    SetWindowsHookExA(WH_KEYBOARD_LL, proc, NULL, 0);
    return run();
}