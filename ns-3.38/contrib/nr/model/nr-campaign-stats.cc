// Copyright (c) 2026
// SPDX-License-Identifier: GPL-2.0-only

#include "nr-campaign-stats.h"

#include <ns3/fatal-error.h>
#include <ns3/simulator.h>

#include <algorithm>
#include <iterator>

namespace ns3
{

std::map<std::string, uint64_t>&
NrCampaignStats::Map()
{
    static std::map<std::string, uint64_t> m;
    return m;
}

bool&
NrCampaignStats::StrictFlag()
{
    static bool strict = false;
    return strict;
}

void
NrCampaignStats::SoftFail(const std::string& site, uint64_t bytes)
{
    if (StrictFlag())
    {
        NS_FATAL_ERROR("[strictPhy] soft-fail site " << site << " hit at t="
                                                     << Simulator::Now().GetSeconds() << " s");
    }
    auto& m = Map();
    m["sf." + site + ".count"] += 1;
    if (bytes > 0)
    {
        m["sf." + site + ".bytes"] += bytes;
    }
    const uint64_t nowUs = static_cast<uint64_t>(Simulator::Now().GetMicroSeconds());
    auto first = m.find("sf." + site + ".first_us");
    if (first == m.end())
    {
        m["sf." + site + ".first_us"] = nowUs;
    }
    m["sf." + site + ".last_us"] = nowUs;
}

void
NrCampaignStats::Inc(const std::string& key, uint64_t n)
{
    Map()[key] += n;
}

void
NrCampaignStats::Max(const std::string& key, uint64_t value)
{
    auto& v = Map()[key];
    v = std::max(v, value);
}

void
NrCampaignStats::Set(const std::string& key, uint64_t value)
{
    Map()[key] = value;
}

uint64_t
NrCampaignStats::Get(const std::string& key)
{
    auto it = Map().find(key);
    return it == Map().end() ? 0 : it->second;
}

void
NrCampaignStats::SetStrict(bool strict)
{
    StrictFlag() = strict;
}

bool
NrCampaignStats::IsStrict()
{
    return StrictFlag();
}

void
NrCampaignStats::Reset()
{
    Map().clear();
}

uint64_t
NrCampaignStats::TotalSoftFails()
{
    uint64_t total = 0;
    const std::string suffix = ".count";
    for (const auto& kv : Map())
    {
        const auto& k = kv.first;
        if (k.rfind("sf.", 0) == 0 && k.size() > suffix.size() &&
            k.compare(k.size() - suffix.size(), suffix.size(), suffix) == 0)
        {
            total += kv.second;
        }
    }
    return total;
}

uint64_t
NrCampaignStats::SumPrefix(const std::string& prefix, uint64_t* nonZero)
{
    uint64_t sum = 0;
    uint64_t nz = 0;
    for (const auto& kv : Map())
    {
        if (kv.first.rfind(prefix, 0) == 0)
        {
            sum += kv.second;
            nz += kv.second > 0 ? 1 : 0;
        }
    }
    if (nonZero)
    {
        *nonZero = nz;
    }
    return sum;
}

void
NrCampaignStats::ErasePrefix(const std::string& prefix)
{
    auto& m = Map();
    for (auto it = m.begin(); it != m.end();)
    {
        it = (it->first.rfind(prefix, 0) == 0) ? m.erase(it) : std::next(it);
    }
}

void
NrCampaignStats::Dump(std::ostream& os)
{
    os << "sf.total=" << TotalSoftFails() << "\n";
    os << "strict=" << (StrictFlag() ? 1 : 0) << "\n";
    for (const auto& kv : Map())
    {
        os << kv.first << "=" << kv.second << "\n";
    }
}

} // namespace ns3
