#include <util/system.h>
#include <numeric>

#include <util/system.h>
#include <tradelayer/ce.h>
#include <tradelayer/fees.h>
#include <tradelayer/insurancefund.h>
#include <tradelayer/log.h>
#include <tradelayer/mdex.h>
#include <tradelayer/register.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/tupleutils.hpp>

// TODO: refactor out of tally.cpp to math_util.hpp or smth
extern bool isOverflow(int64_t a, int64_t b);

//! Dummy fund smart contract address
static const std::string FUND_ADDRESS{"Wdj12J6FZgaY34ZNx12pVpTeF9NQdmpGzj"};
static const uint32_t CONTRACT_ID = 1;

//! Futures contracts native/oracle and spot fees
std::unique_ptr<FeesCache> g_fees(MakeUnique<FeesCache>());

namespace mc = mastercore;

//! Implementation details
struct FundInternals
{
    // std::shared_ptr<CDInfo::Entry> contract_entry;

    // static std::shared_ptr<CDInfo::Entry> MakeContractEntry()
    // {
    //     CDInfo::Entry cd;
    //     return mc::_my_cds->getCD(1, cd), std::make_shared<CDInfo::Entry>(cd);
    // }

    // FundInternals() : contract_entry(MakeContractEntry())
    // {
    // }
};

//! Must be explicitly instantiated as it relies on the register cache: mp_register_map
InsuranceFund::InsuranceFund() : m_internals(MakeUnique<FundInternals>())
{
    // Ensure we have an entry on the register
    if (mc::mp_register_map.find(FUND_ADDRESS) == mc::mp_register_map.end())
        mc::mp_register_map.insert(std::make_pair(FUND_ADDRESS, Register()));
}

InsuranceFund::~InsuranceFund()
{
}

int64_t InsuranceFund::GetFeesTotal(uint32_t pid) const
{
    auto n1 = get_fees_balance(g_fees->native_fees, pid);
    auto n2 = get_fees_balance(g_fees->oracle_fees, pid);
    auto n3 = get_fees_balance(g_fees->spot_fees, pid);
    return n1 + n2 + n3;
}

ContractInfo InsuranceFund::GetContractInfo() const
{
    ContractInfo ci;
    ci.contract_id = CONTRACT_ID;
    ci.collateral_currency = ALL;
    ci.collateral_balance = getMPbalance(FUND_ADDRESS, 1, CONTRACTDEX_RESERVE);
    ci.short_position = mc::getContractRecord(FUND_ADDRESS, CONTRACT_ID, CONTRACT_POSITION);
    ci.is_native = true;
    return ci;
}

FeesCache& InsuranceFund::GetFees() const
{
    return *g_fees;
}

bool InsuranceFund::IsFundAddress(std::string address) const
{
    return FUND_ADDRESS == address;
}

bool InsuranceFund::AccrueFees(uint32_t pid, int64_t amount)
{
    auto p = g_fees->spot_fees.find(pid);
    if (p == g_fees->spot_fees.end()) {
        LOG("ERROR: propertyId=<%d> not found!\n", pid);
        return false;
    }

    auto& fees_available = p->second;

    if (isOverflow(fees_available, amount)) {
        LOG("ERROR: arithmetic overflow [%d + %d]\n", fees_available, amount);
        return false;
    }

    // if (fees_available + amount < 0) {
    //     PrintToLog("%s(): insufficient funds! (amount_remaining: %d, amount: %d)\n", __func__, fees_available, amount);
    //     return false;
    // }

    return fees_available += amount, true;
}

std::tuple<bool, int64_t> InsuranceFund::PayOut(uint32_t pid, int64_t amount)
{
    assert(amount > 0);

    auto p1 = g_fees->spot_fees.find(pid);
    auto a1 = p1 == g_fees->spot_fees.end() ? 0 : p1->second;
    if (a1 >= amount) {
        return p1->second -= amount,
               std::make_tuple(true, amount);
    }

    auto p2 = g_fees->native_fees.find(pid);
    auto a2 = p2 == g_fees->native_fees.end() ? 0 : p2->second;
    if (a1 + a2 >= amount) {
        return p1->second = 0,
               p2->second -= amount - a1,
               std::make_tuple(true, amount);
    }

    auto p3 = g_fees->oracle_fees.find(pid);
    auto a3 = p3 == g_fees->oracle_fees.end() ? 0 : p3->second;
    auto fees_total = a1 + a2 + a3;
    if (fees_total >= amount) {
        return p1->second = 0,
               p2->second = 0,
               p3->second -= amount - a2 - a1,
               std::make_tuple(true, amount);
    }

    if (fees_total <= 0) {
        LOG("Insufficient funds, propertyId=<%d>! (fees amount available: %d, amount needed: %d)\n", pid, fees_total, amount);
        return std::make_tuple(false, 0);
    }

    LOG("Partial payout, propertyId=<%d> (fees amount available: %d, amount needed: %d)\n", pid, fees_total, amount);

    return p1->second = p2->second = p3->second = 0, std::make_tuple(false, amount);
}

//! Number of contracts on the register
static inline uint32_t get_registered_contracts(uint32_t cid)
{
    auto p = mc::mp_register_map.find(FUND_ADDRESS);
    if (p == mc::mp_register_map.end()) return 0;

    // Get position entries from the register
    auto e = p->second.getEntries(cid);
    return !e ? 0 :
        std::accumulate(e->cbegin(), e->cend(), 0L, [](long a, const Entries::value_type& b) { return a + b.first; });
}

//! Last traded price of a contract
static inline uint32_t get_contract_price(uint32_t cid)
{
    auto p = mc::cdexlastprice.find(cid);

    // small change (we've changeg cdexlastprice map)
    if (p != mc::cdexlastprice.end()) {
        const auto& bMap = p->second;
        const auto& it = bMap.rbegin();
        if (it != bMap.rend()) {
            const auto& vPrices = it->second;
            const auto& itt = vPrices.rbegin();
            if (itt != vPrices.rend()) {
                return *(itt);
            }
        }

    }

    return 0;
}

//! Market order
static inline void dex_sumbit_morder(uint32_t cid, int block, int64_t amount)
{
    mc::ContractDex_ADD_MARKET_PRICE(FUND_ADDRESS, cid, amount, block, uint256(), 0, (amount > 0) ? sell : buy, 0, false);
}

//! Limit order trade
static inline void dex_sumbit_lorder(int block, uint32_t cid, int64_t amount)
{
    mc::ContractDex_ADD_ORDERBOOK_EDGE(FUND_ADDRESS, cid, amount, block, uint256(), 0, (amount > 0) ? sell : buy, 0, false);
}

void InsuranceFund::UpdateFundOrders(int block)
{
    auto fees = GetFeesTotal(ALL);
    if (fees == 0) return;

    // we need to hedge/cover 1/2 of ALL units/fees
    auto price = get_contract_price(CONTRACT_ID);
    auto n1 = get_registered_contracts(CONTRACT_ID);
    auto n2 = fees / 2 / price;
    auto n = n2 - n1;

    if (n) {
        LOG("Placing limit order to trade %d ALL contracts\n", n);

        // buy if negative (to reduce position), and sell otherwise
        //dex_sumbit_lorder(block, CONTRACT_ID, n);
    }
}

void InsuranceFund::CoverContracts(int block, uint32_t amount)
{
    LOG("Placing limit order to sell %d ALL contracts\n", amount);

    //dex_sumbit_lorder(block, CONTRACT_ID, amount);
}
