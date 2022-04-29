#ifndef INSURANCE_FUND_H
#define INSURANCE_FUND_H

#include <memory>
#include <mutex>

struct FundInternals;
struct FeesCache;

struct ContractInfo
{
    uint32_t contract_id;
    uint32_t collateral_currency;
    int64_t collateral_balance;
    int64_t short_position;
    bool is_native;
};

class InsuranceFund
{
    std::unique_ptr<FundInternals> m_internals;

public:
    InsuranceFund();
    ~InsuranceFund();

    //! Smart contract properties
    ContractInfo GetContractInfo() const;

    //! Get treasury cache
    FeesCache& GetFees() const;

    //! Given a property id returns grand total
    int64_t GetFeesTotal(uint32_t pid) const;

    //! 
    bool IsFundAddress(std::string address) const;

    //! Given a property id and a loss value, payout what is available
    bool AccrueFees(uint32_t pid, int64_t amount);

    //! Return true if there was enough in the fund balance or 
    //! false if it was a partial coverage or empty fund along with
    //! the amount effected/removed.
    std::tuple<bool, int64_t> PayOut(uint32_t pid, int64_t amount);

    //! Rebalance orderbook based on how many contracts needed to sell
    //! at each tick to keep the asset value of the fund 50/50 between
    //! long ALL and sLTC (or short contract positions x1)
    void UpdateFundOrders(int block);

    //! Order to cover contracts
    void CoverContracts(int block, uint32_t amount);
};

extern std::unique_ptr<InsuranceFund> g_fund;

#endif // INSURANCE_FUND_H