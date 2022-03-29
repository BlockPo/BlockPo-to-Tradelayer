#include <util/system.h>

#include <tradelayer/fees.h>
#include <tradelayer/insurancefund.h>
#include <tradelayer/log.h>
#include <tradelayer/register.h>
#include <tradelayer/tupleutils.hpp>

// TODO: refactor out of tally.cpp to math_util.hpp or smth
extern bool isOverflow(int64_t a, int64_t b);

//! Dummy fund smart contract address
static const std::string FUND_ADDRESS{"fund_address"};

//! Futures contracts native/oracle and spot fees
std::unique_ptr<FeesCache> g_fees(MakeUnique<FeesCache>());

//! Implementation details
struct FundInternals {
};

//! Must be explicitly instantiated as it relies on the register cache: mp_register_map
InsuranceFund::InsuranceFund() : m_internals(MakeUnique<FundInternals>())
{
    namespace mc = mastercore;
    if (mc::mp_register_map.find(FUND_ADDRESS) == mc::mp_register_map.end())
        mc::mp_register_map.insert(std::make_pair(FUND_ADDRESS, Register()));
}

InsuranceFund::~InsuranceFund() {}

int64_t InsuranceFund::GetFeesTotal(uint32_t pid) const
{
    auto n1 = get_fees_balance(g_fees->native_fees, pid);
    auto n2 = get_fees_balance(g_fees->oracle_fees, pid);
    auto n3 = get_fees_balance(g_fees->spot_fees, pid);
    return n1 + n2 + n3;
}

Register& InsuranceFund::GetRegister() const
{
    static Register null_register;
    namespace mc = mastercore;
    auto p = mc::mp_register_map.find(FUND_ADDRESS);
    assert(p != mc::mp_register_map.end());
    return p == mc::mp_register_map.end() ? null_register : p->second;
}

FeesCache& InsuranceFund::GetFees() const
{
    return *g_fees;
}

bool InsuranceFund::AddSpotFees(uint32_t pid, int64_t amount)
{
    auto p = g_fees->spot_fees.find(pid);
    if (p == g_fees->spot_fees.end()) {
        PrintToLog("%s(): ERROR: propertyId: <%d> not found!\n", __func__, pid);
        return false;
    }

    auto amount_remaining = p->second;

    if (isOverflow(amount_remaining, amount)) {
        PrintToLog("%s(): ERROR: arithmetic overflow [%d + %d]\n", __func__, amount_remaining, amount);
        return false;
    }

    if (amount_remaining + amount < 0) {
        PrintToLog("%s(): insufficient funds! (amount_remaining: %d, amount: %d)\n", __func__, amount_remaining, amount);
        return false;
    }

    return p->second += amount, true;
}

int64_t InsuranceFund::PayOut(uint32_t pid, int64_t loss_amount)
{
    assert(loss_amount > 0);

    auto p1 = g_fees->spot_fees.find(pid);
    auto a1 = p1 == g_fees->spot_fees.end() ? 0 : p1->second;
    if (a1 >= loss_amount) {
        return p1->second -= loss_amount, loss_amount;
    }

    auto p2 = g_fees->native_fees.find(pid);
    auto a2 = p2 == g_fees->native_fees.end() ? 0 : p2->second;
    if (a1 + a2 >= loss_amount) {
        return p1->second = 0,
               p2->second -= loss_amount - a1,
               loss_amount;
    }

    auto p3 = g_fees->oracle_fees.find(pid);
    auto a3 = a1 + a2 + (p3 == g_fees->oracle_fees.end() ? 0 : p3->second);
    if (a3 >= loss_amount) {
        return p1->second = 0,
               p2->second = 0,
               p3->second -= loss_amount - a2 - a1,
               loss_amount;
    }

    if (a3 <= 0) {
        PrintToLog("%s(): insufficient funds! (amount available: %d, amount needed: %d)\n", __func__, a3, loss_amount);
        return 0;
    }

    auto amount = loss_amount - a3;
    p1->second = p2->second = p3->second = 0;
    return amount;
}

void InsuranceFund::Rebalance()
{
    throw std::runtime_error("InsuranceFund::Rebalance(): NotImplemented!");
}