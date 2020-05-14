#include "tradelayer/rpcrequirements.h"

#include "tradelayer/dex.h"
#include "tradelayer/mdex.h"
#include "tradelayer/tradelayer.h"
#include "tradelayer/sp.h"
#include "tradelayer/utilsbitcoin.h"
#include "tradelayer/uint256_extensions.h"

#include "arith_uint256.h"
#include "amount.h"
#include "validation.h"
#include "rpc/protocol.h"
#include "sync.h"
#include "tinyformat.h"

#include <boost/algorithm/string.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>

#include <stdint.h>
#include <string>
#include "tradelayer_matrices.h"

using boost::algorithm::token_compress_on;
typedef boost::rational<boost::multiprecision::checked_int128_t> rational_t;
extern int64_t factorE;
extern uint64_t marketP[NPTYPES];
extern int lastBlockg;
extern int vestingActivationBlock;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> market_priceMap;

void RequireBalance(const std::string& address, uint32_t propertyId, int64_t amount)
{
    int64_t balance = getMPbalance(address, propertyId, BALANCE);
    if (balance < amount) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender has insufficient balance for that property");
    }
    int64_t balanceUnconfirmed = getUserAvailableMPbalance(address, propertyId);
    if (balanceUnconfirmed < amount) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender has insufficient balance (due to pending transactions)");
    }
}

void RequireCollateral(const std::string& address, std::string name_traded, int64_t amount, uint64_t leverage)
{
    int64_t uPrice = 1;

    struct FutureContractObject *pfuture = getFutureContractObject(name_traded);
    uint32_t propertyId = (pfuture) ? pfuture->fco_collateral_currency : 0;
    bool inverse_quoted = (pfuture) ? pfuture->fco_quoted : false;

    if(inverse_quoted  && market_priceMap[pfuture->fco_numerator][pfuture->fco_denominator] > 0)
    {
        uPrice = market_priceMap[pfuture->fco_numerator][pfuture->fco_denominator];

    } else if (!inverse_quoted)
        uPrice = COIN;


    arith_uint256 amountTR = (COIN * mastercore::ConvertTo256(amount) * mastercore::ConvertTo256(pfuture->fco_margin_requirement))/(mastercore::ConvertTo256(leverage) * mastercore::ConvertTo256(uPrice));
    int64_t amountToReserve = mastercore::ConvertTo64(amountTR);

    int64_t nBalance = getMPbalance(address, propertyId, BALANCE);

    PrintToLog("%s(): nBalance: %d , amountToReserve: %d \n",__func__, nBalance, amountToReserve);

    if (nBalance < amountToReserve || nBalance == 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender has insufficient balance for collateral");

    int64_t balanceUnconfirmed = getUserAvailableMPbalance(address, propertyId);
    if (balanceUnconfirmed == 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender has insufficient balance (due to pending transactions)");

}

void RequirePrimaryToken(uint32_t propertyId)
{
    if (propertyId < 1 || 2 < propertyId) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier must be 1 (TL) or 2 (TTL)");
    }
}

/* checking there's no active orders */
void RequireNoOrders(std::string sender, uint32_t propertyId)
{
    if(mastercore::ContractDex_CHECK_ORDERS(sender, propertyId)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cancel orders before close position\n");
    }

}

void RequirePropertyName(const std::string& name)
{
    if (name.empty()) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Property name must not be empty");
    }
}

void RequireDifferentAddrs(const std::string& oracleAddress,const std::string& backupAddress)
{
  if (oracleAddress == backupAddress) {
      throw JSONRPCError(RPC_TYPE_ERROR, "oracle and backup addresses must be differents");
  }
}

void RequireExistingProperty(uint32_t propertyId)
{
  LOCK(cs_tally);
  if (!mastercore::IsPropertyIdValid(propertyId)) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier does not exist");
  }
}


void RequireDifferentIds(uint32_t propertyId, uint32_t otherId)
{
    if (propertyId == otherId) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifiers must not be the same");
    }
}

void RequireCrowdsale(uint32_t propertyId)
{
    LOCK(cs_tally);
    CMPSPInfo::Entry sp;
    if (!mastercore::_my_sps->getSP(propertyId, sp)) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
    }
    if (sp.fixed || sp.manual) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier does not refer to a crowdsale");
    }
}

void RequireActiveCrowdsale(uint32_t propertyId)
{
    LOCK(cs_tally);
    if (!mastercore::isCrowdsaleActive(propertyId)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Property identifier does not refer to an active crowdsale");
    }
}

void RequireManagedProperty(uint32_t propertyId)
{
    LOCK(cs_tally);
    CMPSPInfo::Entry sp;
    if (!mastercore::_my_sps->getSP(propertyId, sp)) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
    }
    if (sp.fixed || !sp.manual) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier does not refer to a managed property");
    }
}

void RequireAssociation(uint32_t propertyId,uint32_t contractId)
{
  LOCK(cs_tally);
  CMPSPInfo::Entry sp;
  if (!mastercore::_my_sps->getSP(propertyId, sp)) {
      throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
  }
  if (sp.contract_associated != contractId) {
      throw JSONRPCError(RPC_INVALID_PARAMETER, "Pegged does not comes from this contract");
  }
}

void RequireTokenIssuer(const std::string& address, uint32_t propertyId)
{
    LOCK(cs_tally);
    CMPSPInfo::Entry sp;
    if (!mastercore::_my_sps->getSP(propertyId, sp)) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
    }
    if (address != sp.issuer) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender is not authorized to manage the property");
    }
}

void RequireSaneReferenceAmount(int64_t amount)
{
    if ((0.01 * COIN) < amount) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Reference amount higher is than 0.01 BTC");
    }
}

void RequireHeightInChain(int blockHeight)
{
    if (blockHeight < 0 || mastercore::GetHeight() < blockHeight) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height is out of range");
    }
}

void RequireNotVesting(uint32_t propertyId)
{
  LOCK(cs_tally);
  CMPSPInfo::Entry sp;
  if (!mastercore::_my_sps->getSP(propertyId, sp)) {
    throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
  }

  if (sp.attribute_type == ALL_PROPERTY_TYPE_VESTING && lastBlockg < vestingActivationBlock + 210240) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Vesting Tokens can not be traded at DEx before one year\n");
  }
}

void RequireNotContract(uint32_t propertyId)
{
    LOCK(cs_tally);
    CMPSPInfo::Entry sp;
    if (!mastercore::_my_sps->getSP(propertyId, sp)) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
    }
    if (sp.isContract()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Property must not be future contract\n");
    }
}

void RequireContract(uint32_t propertyId)
{
    LOCK(cs_tally);
    CMPSPInfo::Entry sp;
    if (!mastercore::_my_sps->getSP(propertyId, sp)) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
    }
    if (!sp.isContract()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "contractId must be future contract\n");
    }
}

// NOTE: improve this function
void RequireContract(std::string name_contract)
{
    struct FutureContractObject *pfuture = getFutureContractObject(name_contract);
    uint32_t propertyId = (pfuture) ? pfuture->fco_propertyId : 0;

    LOCK(cs_tally);
    CMPSPInfo::Entry sp;
    if (!mastercore::_my_sps->getSP(propertyId, sp)) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
    }
    if (!sp.isContract()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "contractId must be future contract\n");
    }
}

void RequireOracleContract(uint32_t propertyId)
{
    LOCK(cs_tally);
    CMPSPInfo::Entry sp;
    if (!mastercore::_my_sps->getSP(propertyId, sp)) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
    }

    if (!sp.isOracle()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "contractId must be oracle future contract\n");
    }
}

void RequirePeggedCurrency(uint32_t propertyId)
{
    LOCK(cs_tally);
    CMPSPInfo::Entry sp;
    if (!mastercore::_my_sps->getSP(propertyId, sp)) {
      throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
    }

    if (!sp.isPegged()) {
      throw JSONRPCError(RPC_INVALID_PARAMETER, "propertyId must be a pegged currency\n");
    }
}

void RequireForPegged(const std::string& address, uint32_t propertyId, uint32_t contractId, uint64_t amount)
{
    uint64_t balance = static_cast<uint64_t>(getMPbalance(address, propertyId, BALANCE));
    int64_t position = getMPbalance(address, contractId, NEGATIVE_BALANCE);
    if (balance < amount) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender has not enough amount of collateral currency");
    }

    if (position == 0) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender has not the short position required on this contract");
    }
    // int64_t balanceUnconfirmed = getUserAvailableMPbalance(address, propertyId); // check the pending amounts
    // if (balanceUnconfirmed > 0) {
    //     throw JSONRPCError(RPC_TYPE_ERROR, "Sender has insufficient balance (due to pending transactions)");
    // }


}

void RequireMatchingDExOffer(const std::string& address, uint32_t propertyId)
{
    LOCK(cs_tally);
    if (!mastercore::DEx_offerExists(address, propertyId)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "No matching sell offer on the distributed exchange");
    }
}

void RequireNoOtherDExOffer(const std::string& address, uint32_t propertyId)
{
    LOCK(cs_tally);
    if (mastercore::DEx_offerExists(address, propertyId)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Another active sell offer from the given address already exists on the distributed exchange");
    }
}

void RequireSaneDExFee(const std::string& address, uint32_t propertyId)
{
    LOCK(cs_tally);
    const CMPOffer* poffer = mastercore::DEx_getOffer(address, propertyId);
    if (poffer == NULL) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Unable to load sell offer from the distributed exchange");
    }
    if (poffer->getMinFee() > 1000000) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Minimum accept fee is higher than 0.01 BTC (use override = true to continue)");
    }
}

void RequireSaneDExPaymentWindow(const std::string& address, uint32_t propertyId)
{
    LOCK(cs_tally);
    const CMPOffer* poffer = mastercore::DEx_getOffer(address, propertyId);
    if (poffer == NULL) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Unable to load sell offer from the distributed exchange");
    }
    if (poffer->getBlockTimeLimit() < 10) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Payment window is less than 10 blocks (use override = true to continue)");
    }
}

void RequireShort(std::string& fromAddress, uint32_t contractId, uint64_t amount)
{
    LOCK(cs_tally);
    int64_t contractsNeeded = 0;
    // int index = static_cast<int>(contractId);

    CMPSPInfo::Entry sp;
    if (!mastercore::_my_sps->getSP(contractId, sp)) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
    }

    if (sp.isContract()) {
        int64_t notionalSize = static_cast<int64_t>(sp.notional_size);
        int64_t position = getMPbalance(fromAddress, contractId, NEGATIVE_BALANCE);
        // rational_t conv = mastercore::notionalChange(contractId);
        rational_t conv = rational_t(1,1);
        int64_t num = conv.numerator().convert_to<int64_t>();
        int64_t denom = conv.denominator().convert_to<int64_t>();
        arith_uint256 Amount = mastercore::ConvertTo256(num) * mastercore::ConvertTo256(amount) / mastercore::ConvertTo256(denom); // Alls needed
        arith_uint256 contracts = mastercore::DivideAndRoundUp(Amount * mastercore::ConvertTo256(notionalSize), mastercore::ConvertTo256(1)) ;
        contractsNeeded = mastercore::ConvertTo64(contracts);
        if (contractsNeeded > position) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Not enough short position\n");
        }

    } else  {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "contractId must be future contract\n");
    }
}

void RequireContractTxId(std::string& txid)
{
    std::string result;
    uint256 tx;
    tx.SetHex(txid);
    if (!mastercore::p_txlistdb->getTX(tx, result)) {
          throw JSONRPCError(RPC_INVALID_PARAMETER, "TxId doesn't exist\n");
    }

    std::vector<std::string> vstr;
    boost::split(vstr, result, boost::is_any_of(":"), token_compress_on);
    unsigned int type = atoi(vstr[2]);
    if (type != 29) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "TxId isn't a future contract trade\n");
    }

}

void RequireSaneName(std::string& name)
{
    LOCK(cs_tally);
    uint32_t nextSPID = mastercore::_my_sps->peekNextSPID();
    for (uint32_t propertyId = 1; propertyId < nextSPID; propertyId++) {
        CMPSPInfo::Entry sp;
        if (mastercore::_my_sps->getSP(propertyId, sp)) {
            PrintToConsole("Property Id: %d\n",propertyId);
            if (sp.name == name){
                throw JSONRPCError(RPC_INVALID_PARAMETER,"We have another property with the same name\n");
            }
        }
    }

}

void RequirePeggedSaneName(std::string& name)
{
    if (name.empty()) { throw JSONRPCError(RPC_INVALID_PARAMETER,"Pegged need a name!\n"); }
}

void RequireContractOrder(std::string& fromAddress, uint32_t contractId)
{

  if (!mastercore::ContractDex_CHECK_ORDERS(fromAddress, contractId)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,"There's no order in this future contract\n");
  }

}
