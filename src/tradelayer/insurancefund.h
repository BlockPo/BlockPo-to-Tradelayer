#ifndef INSURANCE_FUND_H
#define INSURANCE_FUND_H

#include <memory>
#include <mutex>

struct FundInternals;
struct FeesCache;
class Register;

class InsuranceFund
{
    std::unique_ptr<FundInternals> m_internals;

public:
    InsuranceFund();
    ~InsuranceFund();

    //! Smart contracts register
    Register& GetRegister() const;

    //! Get treasury cache
    FeesCache& GetFees() const;
    
    //! Given a property id returns grand fees total
    int64_t GetFeesTotal(uint32_t pid) const;

    //! Given a property id and a loss value, payout what is available
    bool AddSpotFees(uint32_t pid, int64_t amount);

    //! Given a property id and a loss value, pays out what is available
    int64_t PayOut(uint32_t pid, int64_t loss_amount);

    //! Rebalance orderbook based on how many contracts needed to sell 
    //! at each tick to keep the asset value of the fund 50/50 between 
    //! long ALL and sLTC (or short contract positions x1)
    void Rebalance();
};

extern std::unique_ptr<InsuranceFund> g_fund;

#endif // INSURANCE_FUND_H