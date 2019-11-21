#include "GatewayHelper.hpp"

namespace GatewayHelper
{
    PMIB_IPFORWARDTABLE GetForwardTable()
    {
        MIB_IPFORWARDTABLE* table = NULL;
        ULONG buffer_size = 0;

        DWORD outcome = GetIpForwardTable(table, &buffer_size, FALSE);
        if (outcome == ERROR_INSUFFICIENT_BUFFER)
        {
            table = (MIB_IPFORWARDTABLE*)malloc(buffer_size);
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

    ULONG GetInterfaceMetric(DWORD ifIndex)
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

    std::vector<DWORD> GetGateways(NET_IFINDEX ifIndex)
    {
        std::vector<DWORD> gateways;

        auto table = GetForwardTable();
        for (DWORD i = 0; i < table->dwNumEntries; i++)
        {
            auto* row = &table->table[i];
            if (row->dwForwardIfIndex == ifIndex && row->dwForwardDest == 0)
            {
                gateways.push_back(row->dwForwardNextHop);
            }
        }
        free(table);

        return gateways;
    }

    MIB_IPINTERFACE_ROW GetInterface(ULONG64 luid)
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

    PIP_ADAPTER_ADDRESSES GetAdapters()
    {
        IP_ADAPTER_ADDRESSES* adapters = NULL;
        ULONG buffer_size = 0;

        DWORD outcome = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_DNS_SERVER, NULL, adapters, &buffer_size);
        if (outcome == ERROR_BUFFER_OVERFLOW)
        {
            adapters = (IP_ADAPTER_ADDRESSES*)malloc(buffer_size);
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

    PMIB_IPADDRTABLE GetAddrTable()
    {
        MIB_IPADDRTABLE* table = NULL;
        ULONG buffer_size = 0;

        DWORD outcome = GetIpAddrTable(table, &buffer_size, FALSE);
        if (outcome == ERROR_INSUFFICIENT_BUFFER)
        {
            table = (MIB_IPADDRTABLE*)malloc(buffer_size);
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

    void DeleteGateways(NET_IFINDEX ifIndex)
    {
        auto* table = GatewayHelper::GetForwardTable();
        for (DWORD i = 0; i < table->dwNumEntries; i++)
        {
            auto* row = &table->table[i];
            if (row->dwForwardIfIndex == ifIndex && row->dwForwardDest == 0)
            {
                DeleteIpForwardEntry(row);
            }
        }
        free(table);
    }

    void AddGateway(NET_IFINDEX ifIndex, ULONG ifMetric, ULONG metric, DWORD gateway)
    {
        auto* table = GatewayHelper::GetForwardTable();

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
}