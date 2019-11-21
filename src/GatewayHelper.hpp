#pragma once

#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>

#include <vector>

namespace GatewayHelper {
    PMIB_IPFORWARDTABLE GetForwardTable();
    ULONG GetInterfaceMetric(DWORD ifIndex);
    std::vector<DWORD> GetGateways(NET_IFINDEX ifIndex);
    MIB_IPINTERFACE_ROW GetInterface(ULONG64 luid);
    PIP_ADAPTER_ADDRESSES GetAdapters();
    PMIB_IPADDRTABLE GetAddrTable();
    void DeleteGateways(NET_IFINDEX ifIndex);
    void AddGateway(NET_IFINDEX ifIndex, ULONG ifMetric, ULONG metric, DWORD gateway);
}