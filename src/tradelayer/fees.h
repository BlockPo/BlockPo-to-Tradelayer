#ifndef FEES_H
#define FEES_H

#include <map>
#include <memory>

using MoneyMap = std::map<uint32_t, int64_t>;

inline int64_t get_fees_balance(const MoneyMap& money, uint32_t pid, int64_t none = 0)
{
    auto p = money.find(pid);
    return p == money.end() ? none : p->second;
}

struct FeesCache {
    MoneyMap native_fees;
    MoneyMap oracle_fees;
    MoneyMap spot_fees;
};

extern std::unique_ptr<FeesCache> g_fees;

#endif // FEES_H