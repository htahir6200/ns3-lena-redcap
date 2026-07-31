#pragma once

#include <cstdint>
#include <mutex>
#include <string>

namespace ns3
{

/**
 * \brief Process-wide access / attach counters for RedCap capacity campaigns.
 *
 * Thread-safe increments from UE MAC/RRC; Dump() writes a one-line key=value file
 * next to flowOutput for parse_results.py.
 */
class NrAccessStats
{
  public:
    static void IncPreambleTx();
    static void IncRarTimeout();
    static void IncRaSuccess();
    static void IncRaFailed();
    static void IncRrcConnected();
    static void IncRrcTimeout();

    static void Reset();
    static void Dump(const std::string& path);

    static uint64_t GetPreambleTx();
    static uint64_t GetRarTimeout();
    static uint64_t GetRaSuccess();
    static uint64_t GetRaFailed();
    static uint64_t GetRrcConnected();
    static uint64_t GetRrcTimeout();

  private:
    static std::mutex& Mutex();
    static uint64_t& PreambleTx();
    static uint64_t& RarTimeout();
    static uint64_t& RaSuccess();
    static uint64_t& RaFailed();
    static uint64_t& RrcConnected();
    static uint64_t& RrcTimeout();
};

} // namespace ns3
