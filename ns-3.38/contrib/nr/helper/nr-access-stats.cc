#include "nr-access-stats.h"

#include <fstream>
#include <iostream>

namespace ns3
{

std::mutex&
NrAccessStats::Mutex()
{
    static std::mutex m;
    return m;
}

uint64_t&
NrAccessStats::PreambleTx()
{
    static uint64_t v = 0;
    return v;
}

uint64_t&
NrAccessStats::RarTimeout()
{
    static uint64_t v = 0;
    return v;
}

uint64_t&
NrAccessStats::RaSuccess()
{
    static uint64_t v = 0;
    return v;
}

uint64_t&
NrAccessStats::RaFailed()
{
    static uint64_t v = 0;
    return v;
}

uint64_t&
NrAccessStats::RrcConnected()
{
    static uint64_t v = 0;
    return v;
}

uint64_t&
NrAccessStats::RrcTimeout()
{
    static uint64_t v = 0;
    return v;
}

void
NrAccessStats::IncPreambleTx()
{
    std::lock_guard<std::mutex> lock(Mutex());
    ++PreambleTx();
}

void
NrAccessStats::IncRarTimeout()
{
    std::lock_guard<std::mutex> lock(Mutex());
    ++RarTimeout();
}

void
NrAccessStats::IncRaSuccess()
{
    std::lock_guard<std::mutex> lock(Mutex());
    ++RaSuccess();
}

void
NrAccessStats::IncRaFailed()
{
    std::lock_guard<std::mutex> lock(Mutex());
    ++RaFailed();
}

void
NrAccessStats::IncRrcConnected()
{
    std::lock_guard<std::mutex> lock(Mutex());
    ++RrcConnected();
}

void
NrAccessStats::IncRrcTimeout()
{
    std::lock_guard<std::mutex> lock(Mutex());
    ++RrcTimeout();
}

void
NrAccessStats::Reset()
{
    std::lock_guard<std::mutex> lock(Mutex());
    PreambleTx() = 0;
    RarTimeout() = 0;
    RaSuccess() = 0;
    RaFailed() = 0;
    RrcConnected() = 0;
    RrcTimeout() = 0;
}

uint64_t
NrAccessStats::GetPreambleTx()
{
    std::lock_guard<std::mutex> lock(Mutex());
    return PreambleTx();
}

uint64_t
NrAccessStats::GetRarTimeout()
{
    std::lock_guard<std::mutex> lock(Mutex());
    return RarTimeout();
}

uint64_t
NrAccessStats::GetRaSuccess()
{
    std::lock_guard<std::mutex> lock(Mutex());
    return RaSuccess();
}

uint64_t
NrAccessStats::GetRaFailed()
{
    std::lock_guard<std::mutex> lock(Mutex());
    return RaFailed();
}

uint64_t
NrAccessStats::GetRrcConnected()
{
    std::lock_guard<std::mutex> lock(Mutex());
    return RrcConnected();
}

uint64_t
NrAccessStats::GetRrcTimeout()
{
    std::lock_guard<std::mutex> lock(Mutex());
    return RrcTimeout();
}

void
NrAccessStats::Dump(const std::string& path)
{
    std::lock_guard<std::mutex> lock(Mutex());
    std::ofstream f(path.c_str(), std::ofstream::out | std::ofstream::trunc);
    if (!f.is_open())
    {
        std::cerr << "NrAccessStats: cannot open " << path << std::endl;
        return;
    }
    f << "preamble_tx=" << PreambleTx() << "\n"
      << "rar_timeout=" << RarTimeout() << "\n"
      << "ra_success=" << RaSuccess() << "\n"
      << "ra_failed=" << RaFailed() << "\n"
      << "rrc_connected=" << RrcConnected() << "\n"
      << "rrc_timeout=" << RrcTimeout() << "\n";
    f.close();
}

} // namespace ns3
