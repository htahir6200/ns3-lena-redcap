// Copyright (c) 2026
// SPDX-License-Identifier: GPL-2.0-only
//
// Process-wide counters for dense RedCap campaigns (IoTJ R1 revision).
//
// Two uses:
//  1. Soft-fail accounting. Every "dense hardening" site that used to be an
//     NS_FATAL_ERROR / NS_ASSERT and now drops or ignores an event calls
//     NrCampaignStats::SoftFail("SITE_ID", bytes). In strict mode
//     (NrCampaignStats::SetStrict(true)) the same call aborts the simulation,
//     restoring the original fatal behaviour.
//  2. Scheduler / control-plane / queue counters (Inc / Max / Set), dumped as
//     key=value lines at the end of a run.
//
// ns-3 is single-threaded per process, so plain static maps are sufficient.

#ifndef NR_CAMPAIGN_STATS_H
#define NR_CAMPAIGN_STATS_H

#include <cstdint>
#include <map>
#include <ostream>
#include <string>

namespace ns3
{

class NrCampaignStats
{
  public:
    /// Count one soft-fail at a hardening site; abort if strict mode is on.
    static void SoftFail(const std::string& site, uint64_t bytes = 0);

    /// Increment a generic counter.
    static void Inc(const std::string& key, uint64_t n = 1);
    /// Keep the maximum value seen for a key.
    static void Max(const std::string& key, uint64_t value);
    /// Overwrite a value.
    static void Set(const std::string& key, uint64_t value);
    /// Read a value (0 if absent).
    static uint64_t Get(const std::string& key);

    static void SetStrict(bool strict);
    static bool IsStrict();

    static void Reset();
    /// Write all counters as "key=value" lines, sorted by key.
    static void Dump(std::ostream& os);
    /// Sum of all values whose key starts with prefix; nonZero gets the count of keys > 0.
    static uint64_t SumPrefix(const std::string& prefix, uint64_t* nonZero = nullptr);
    /// Remove all keys with the given prefix.
    static void ErasePrefix(const std::string& prefix);
    /// Total soft-fail events over all sites.
    static uint64_t TotalSoftFails();

  private:
    static std::map<std::string, uint64_t>& Map();
    static bool& StrictFlag();
};

} // namespace ns3

#endif // NR_CAMPAIGN_STATS_H
