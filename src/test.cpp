#define NTDDI_VERSION NTDDI_VISTA
#define WINVER _WIN32_WINNT_VISTA
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include <winsock2.h>
#include <stdio.h>
#include <windows.h>
#include <ws2def.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <netioapi.h>

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

char *addressToString(DWORD address)
{
    in_addr in;
    in.s_addr = address;
    return inet_ntoa(in);
}

void test()
{
    auto *table = getForwardTable();

    for (DWORD i = 0; i < table->dwNumEntries; ++i)
    {
        auto *row = &table->table[i];
        if (row->dwForwardDest == 0)
        {
            printf("GATEWAY %d\n", i);
            printf("dwForwardDest: %s\n", addressToString(row->dwForwardDest));
            printf("dwForwardMask: %s\n", addressToString(row->dwForwardMask));
            printf("dwForwardPolicy: %s\n", addressToString(row->dwForwardPolicy));
            printf("dwForwardNextHop: %s\n", addressToString(row->dwForwardNextHop));
            printf("dwForwardIfIndex: %u\n", row->dwForwardIfIndex);
            printf("dwForwardType: %u\n", row->dwForwardType);
            printf("dwForwardProto: %u\n", row->dwForwardProto);
            printf("dwForwardAge: %u\n", row->dwForwardAge);
            printf("dwForwardNextHopAS: %s\n", addressToString(row->dwForwardNextHopAS));
            printf("dwForwardMetric1: %u\n", row->dwForwardMetric1);
            printf("dwForwardMetric2: %u\n", row->dwForwardMetric2);
            printf("dwForwardMetric3: %u\n", row->dwForwardMetric3);
            printf("dwForwardMetric4: %u\n", row->dwForwardMetric4);
            printf("dwForwardMetric5: %u\n\n\n", row->dwForwardMetric5);
        }
    }

    free(table);
}

void testv2()
{
    auto *pIPAddrTable = getAddrTable();

    in_addr IPAddr;

    printf("\tNum Entries: %ld\n", pIPAddrTable->dwNumEntries);
    for (int i = 0; i < (int)pIPAddrTable->dwNumEntries; i++)
    {
        auto *row = &pIPAddrTable->table[i];
        printf("\n\tInterface Index[%d]:\t%ld\n", i, pIPAddrTable->table[i].dwIndex);
        IPAddr.s_addr = (u_long)pIPAddrTable->table[i].dwAddr;
        printf("\tIP Address[%d]:     \t%s\n", i, inet_ntoa(IPAddr));
        IPAddr.s_addr = (u_long)pIPAddrTable->table[i].dwMask;
        printf("\tSubnet Mask[%d]:    \t%s\n", i, inet_ntoa(IPAddr));
        IPAddr.s_addr = (u_long)pIPAddrTable->table[i].dwBCastAddr;
        printf("\tBroadCast[%d]:      \t%s (%ld%)\n", i, inet_ntoa(IPAddr), pIPAddrTable->table[i].dwBCastAddr);
        printf("\tReassembly size[%d]:\t%ld\n", i, pIPAddrTable->table[i].dwReasmSize);
        printf("\tType and State[%d]:", i);
        if (pIPAddrTable->table[i].wType & MIB_IPADDR_PRIMARY)
            printf("\tPrimary IP Address");
        if (pIPAddrTable->table[i].wType & MIB_IPADDR_DYNAMIC)
            printf("\tDynamic IP Address");
        if (pIPAddrTable->table[i].wType & MIB_IPADDR_DISCONNECTED)
            printf("\tAddress is on disconnected interface");
        if (pIPAddrTable->table[i].wType & MIB_IPADDR_DELETED)
            printf("\tAddress is being deleted");
        if (pIPAddrTable->table[i].wType & MIB_IPADDR_TRANSIENT)
            printf("\tTransient address");
        printf("\n");
    }

    free(pIPAddrTable);
}

ULONG getMetric(DWORD ifIndex)
{
    MIB_IPINTERFACE_ROW *inf = (MIB_IPINTERFACE_ROW *)malloc(sizeof(MIB_IPINTERFACE_ROW));
    memset(inf, 0, sizeof(MIB_IPINTERFACE_ROW));

    inf->Family = AF_INET;
    inf->InterfaceIndex = ifIndex;

    DWORD outcome = GetIpInterfaceEntry(inf);
    if (outcome != NO_ERROR)
    {
        if (inf)
            free(inf);

        fprintf(stderr, "Failed to fetch network interface. (%u)", outcome);
        exit(-1);
        return 0;
    }

    auto metric = inf->Metric;
    free(inf);
    return metric;
}

int main(int argc, char *argv[])
{
    IP_ADAPTER_ADDRESSES *adapters = NULL;
    ULONG buffer_size = 0;

    DWORD outcome = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_DNS_SERVER, NULL, adapters, &buffer_size);
    if (outcome == ERROR_BUFFER_OVERFLOW)
    {
        adapters = (IP_ADAPTER_ADDRESSES *)malloc(buffer_size);
        outcome = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_DNS_SERVER, NULL, adapters, &buffer_size);
    }

    do
    {
        printf("Name: %s\n", adapters->AdapterName);
        printf("Description: %ls\n", adapters->Description);
        printf("FriendlyName: %ls\n", adapters->FriendlyName);
        printf("IfIndex: %d\n", adapters->IfIndex);
        printf("Luid: %llu\n", adapters->Luid.Value);
        printf("Ipv4Enabled: %d\n", adapters->Ipv4Enabled);
        printf("Ipv4Metric: %d\n\n\n", adapters->Ipv4Metric);
    } while (adapters = adapters->Next);

    return 0;
}