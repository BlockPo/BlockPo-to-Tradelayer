#include <tradelayer/rpcrequirements.h>
#include <tradelayer/ce.h>
#include <tradelayer/dex.h>
#include <tradelayer/mdex.h>
#include <tradelayer/register.h>
#include <tradelayer/rules.h>
#include <tradelayer/sp.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/tradelayer_matrices.h>
#include <tradelayer/uint256_extensions.h>
#include <tradelayer/utilsbitcoin.h>

#include <amount.h>
#include <arith_uint256.h>
#include <rpc/protocol.h>
#include <sync.h>
#include <tinyformat.h>
#include <validation.h>

#include <stdint.h>
#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>

using namespace mastercore;

using boost::algorithm::token_compress_on;
typedef boost::rational<boost::multiprecision::checked_int128_t> rational_t;

void RequireBalance(const std::string& address, uint32_t propertyId, int64_t amount)
{
    int64_t balance = getMPbalance(address, propertyId, BALANCE);
    if (balance < amount) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender has insufficient property balance");
    }
    int64_t balanceUnconfirmed = getUserAvailableMPbalance(address, propertyId);
    if (balanceUnconfirmed < amount) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender has insufficient balance (due to pending transactions)");
    }
}

void RequireCollateral(const std::string& address, std::string name_traded, int64_t amount, uint64_t leverage)
{
    //NOTE: add changes to inverse quoted
    int64_t uPrice = COIN;
    uint32_t contractId = 0;
    CDInfo::Entry cd;
    std::pair<int64_t, int64_t> factor;

    getContractFromName(name_traded, contractId, cd);

    // max = 2.5 basis point in oracles, max = 1.0 basis point in natives
    (cd.isOracle()) ? (factor.first = 100025, factor.second = 100000) : (cd.isNative()) ? (factor.first = 10001, factor.second = 10000) : (factor.first = 1, factor.second = 1);

    arith_uint256 amountTR = (ConvertTo256(factor.first) * ConvertTo256(COIN) * ConvertTo256(amount) * ConvertTo256(cd.margin_requirement)) / (ConvertTo256(leverage) * ConvertTo256(uPrice) * ConvertTo256(factor.second));
    int64_t amountToReserve = ConvertTo64(amountTR);

    int64_t nBalance = getMPbalance(address, cd.collateral_currency, BALANCE);

    if (nBalance < amountToReserve || nBalance == 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender has insufficient balance for collateral");

    int64_t balanceUnconfirmed = getUserAvailableMPbalance(address, cd.collateral_currency);
    if (balanceUnconfirmed == 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender has insufficient balance (due to pending transactions)");

}

void RequirePosition(const std::string& address, uint32_t contractId)
{
    const int64_t position = getContractRecord(address, contractId, CONTRACT_POSITION);
    if (position == 0 ) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender has not position in this contract");
    }
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
    if(ContractDex_CHECK_ORDERS(sender, propertyId)) {
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
  if (!IsPropertyIdValid(propertyId)) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier does not exist");
  }
}


void RequireDifferentIds(uint32_t propertyId, uint32_t otherId)
{
    if (propertyId == otherId) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifiers must not be the same");
    }
}

void RequireManagedProperty(uint32_t propertyId)
{
    LOCK(cs_tally);
    CMPSPInfo::Entry sp;
    if (!_my_sps->getSP(propertyId, sp)) {
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
  if (!_my_sps->getSP(propertyId, sp)) {
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
    if (!_my_sps->getSP(propertyId, sp)) {
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
    if (blockHeight < 0 || GetHeight() < blockHeight) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height is out of range");
    }
}

void RequireNotVesting(uint32_t propertyId)
{
  LOCK(cs_tally);
  CMPSPInfo::Entry sp;
  if (!_my_sps->getSP(propertyId, sp)) {
    throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
  }

  const CConsensusParams &params = ConsensusParams();
  if (sp.attribute_type == ALL_PROPERTY_TYPE_VESTING && lastBlockg < params.MSC_VESTING_BLOCK + 210240) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "Vesting Tokens can not be traded at DEx before one year\n");
  }
}

void RequireOracleContract(uint32_t contractId)
{
    LOCK(cs_register);
    CDInfo::Entry cd;
    if (!_my_cds->getCD(contractId, cd)) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve contract");
    }

    if (!cd.isOracle()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "contractId must be oracle future contract\n");
    }
}

void RequirePeggedCurrency(uint32_t propertyId)
{
    LOCK(cs_tally);
    CMPSPInfo::Entry sp;
    if (!_my_sps->getSP(propertyId, sp)) {
      throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
    }

    if (!sp.isPegged()) {
      throw JSONRPCError(RPC_INVALID_PARAMETER, "propertyId must be a pegged currency\n");
    }
}

void RequireForPegged(const std::string& address, uint32_t propertyId, uint32_t contractId, uint64_t amount)
{
    uint64_t balance = static_cast<uint64_t>(getMPbalance(address, propertyId, BALANCE));
    int64_t position = getMPbalance(address, contractId, CONTRACT_BALANCE);
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
    if (!DEx_offerExists(address, propertyId)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "No matching sell offer on the distributed exchange");
    }
}

void RequireNoOtherDExOffer(const std::string& address, uint32_t propertyId)
{
    LOCK(cs_tally);
    if (DEx_offerExists(address, propertyId)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Another active sell offer from the given address already exists on the distributed exchange");
    }
}

void RequireSaneDExFee(const std::string& address, uint32_t propertyId)
{
    LOCK(cs_tally);
    const CMPOffer* poffer = DEx_getOffer(address, propertyId);
    if (poffer == nullptr) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Unable to load sell offer from the distributed exchange");
    }

    if (poffer->getMinFee() > 1000000) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Minimum accept fee is higher than 0.01 LTC (use override = true to continue)");
    }
}

void RequireSaneDExPaymentWindow(const std::string& address, uint32_t propertyId)
{
    LOCK(cs_tally);
    const CMPOffer* poffer = DEx_getOffer(address, propertyId);
    if (poffer == nullptr) {
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

    CDInfo::Entry cd;
    if (!_my_cds->getCD(contractId, cd)) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
    }

    const int64_t notionalSize = static_cast<int64_t>(cd.notional_size);
    const int64_t position = getContractRecord(fromAddress, contractId, CONTRACT_POSITION);

    if (position >= 0) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Must have short position\n");
    }

    const arith_uint256 Contracts = ConvertTo256(amount) * ConvertTo256(notionalSize) / (ConvertTo256(COIN) * ConvertTo256(COIN));
    contractsNeeded = ConvertTo64(Contracts);

    if (contractsNeeded > abs(position)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Not enough short position\n");
    }

}

void RequireContractTxId(std::string& txid)
{
    std::string result;
    uint256 tx;
    tx.SetHex(txid);
    if (!p_txlistdb->getTX(tx, result)) {
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
    uint32_t nextSPID = _my_sps->peekNextSPID();
    for (uint32_t propertyId = 1; propertyId < nextSPID; propertyId++) {
        CMPSPInfo::Entry sp;
        if (_my_sps->getSP(propertyId, sp)) {
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

  if (!ContractDex_CHECK_ORDERS(fromAddress, contractId)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,"There's no order in this future contract\n");
  }

}


void RequireFeatureActivated(const uint16_t feature)
{
    if (!IsFeatureActivated(feature, GetHeight())) {
       throw JSONRPCError(RPC_TYPE_ERROR, "Feature is not Activated");
    }
}


void RequireAmountForFee(const std::string& address, uint32_t propertyId, int64_t amount)
{
    const arith_uint256 am = ConvertTo256(amount) + DivideAndRoundUp(ConvertTo256(amount) * ConvertTo256(5), ConvertTo256(BASISPOINT) * ConvertTo256(BASISPOINT));
    int64_t amountToReserve = ConvertTo64(am);

    int64_t nBalance = getMPbalance(address, propertyId, BALANCE);

    if (nBalance < amountToReserve || nBalance == 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender has insufficient balance");

    int64_t balanceUnconfirmed = getUserAvailableMPbalance(address, propertyId);
    if (balanceUnconfirmed == 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender has insufficient balance (due to pending transactions)");

}

// input block must be greater or equal to blockheight
void RequireBlockHeight(const int& block)
{
    const int height = GetHeight();
    if (block < height)
        throw JSONRPCError(RPC_TYPE_ERROR, "Block height needed not reached");

}
