#ifndef OMNICORE_TX_H
#define OMNICORE_TX_H

class CMPMetaDEx;
class CMPOffer;
class CTransaction;
////////////////////////////
/** New things for Contracts */
class CMPContractDex;
////////////////////////////
#include "omnicore/omnicore.h"

#include "uint256.h"
#include "utilstrencodings.h"

#include <stdint.h>
#include <string.h>

#include <string.h>
#include <string>

using mastercore::strTransactionType;

/** The class is responsible for transaction interpreting/parsing.
 *
 * It invokes other classes and methods: offers, accepts, tallies (balances).
 */

class CMPTransaction
{
    friend class CMPMetaDEx;
    friend class CMPOffer;
    ////////////////////////////
    /** New things for Contracts */
    friend class CMPContractDex;

private:
    uint256 txid;
    int block;
    int64_t blockTime;  // internally nTime is still an "unsigned int"
    unsigned int tx_idx;  // tx # within the block, 0-based
    uint64_t tx_fee_paid;

    int pkt_size;
    unsigned char pkt[65535];
    int encodingClass;  // No Marker = 0, Class A = 1, Class B = 2, Class C = 3

    std::string sender;
    std::string receiver;

    unsigned int type;
    unsigned short version; // = MP_TX_PKT_V0;

    // SimpleSend, SendToOwners, TradeOffer, MetaDEx, AcceptOfferBTC,
    // CreatePropertyFixed, CreatePropertyVariable, GrantTokens, RevokeTokens
    uint64_t nValue;
    uint64_t nNewValue;

    // SimpleSend, SendToOwners, TradeOffer, MetaDEx, AcceptOfferBTC,
    // CreatePropertyFixed, CreatePropertyVariable, CloseCrowdsale,
    // CreatePropertyMananged, GrantTokens, RevokeTokens, ChangeIssuer
    unsigned int property;

    // CreatePropertyFixed, CreatePropertyVariable, CreatePropertyMananged, MetaDEx, SendAll
    unsigned char ecosystem;
    unsigned int distribution_property;

    // CreatePropertyFixed, CreatePropertyVariable, CreatePropertyMananged
    unsigned short prop_type;
    unsigned int prev_prop_id;
    char category[SP_STRING_FIELD_LEN];
    char subcategory[SP_STRING_FIELD_LEN];
    /* New things for contracts */
    char stxid[SP_STRING_FIELD_LEN];
    //////////////////////////////////
    char name[SP_STRING_FIELD_LEN];
    char url[SP_STRING_FIELD_LEN];
    char data[SP_STRING_FIELD_LEN];
    uint64_t deadline;
    unsigned char early_bird;
    unsigned char percentage;

    // MetaDEx
    unsigned int desired_property;
    uint64_t desired_value;
    int64_t amount_forsale;
    unsigned char action; // depreciated

    // DEX 1
    uint64_t amountDesired;
    uint8_t timeLimit;
    uint64_t minFee;
    uint8_t subAction;
    uint8_t option; // buy=1 , sell=2
    ////////////////////////////////////
    /** New things for Contract */
    uint64_t effective_price;
    uint8_t trading_action;
    uint32_t propertyId;
    uint32_t contractId;
    uint64_t amount;
    /* uint64_t ticksize; */
    /*uint32_t nextContractId;*/
    uint32_t blocks_until_expiration;
    uint32_t notional_size;
    uint32_t collateral_currency;
    uint32_t margin_requirement;
    /*uint32_t numerator;*/
    uint32_t denomination;
    // int block;
    ////////////////////////////

    // Alert
    uint16_t alert_type;
    uint32_t alert_expiry;
    char alert_text[SP_STRING_FIELD_LEN];

    // Activation
    uint16_t feature_id;
    uint32_t activation_block;
    uint32_t min_client_version;

    // TradeOffer
    uint64_t amount_desired;
    unsigned char blocktimelimit;
    uint64_t min_fee;
    unsigned char subaction;

    // Indicates whether the transaction can be used to execute logic
    bool rpcOnly;

    /** Checks whether a pointer to the payload is past it's last position. */
    bool isOverrun(const char* p);

    /**
     * Variable Integers
     */
    std::vector<uint8_t> GetNextVarIntBytes(int &i);

    /**
     * Payload parsing
     */
    bool interpret_TransactionType();
    bool interpret_SimpleSend();
    bool interpret_SendAll();
    bool interpret_CreatePropertyFixed();
    bool interpret_CreatePropertyVariable();
    bool interpret_CloseCrowdsale();
    bool interpret_CreatePropertyManaged();
    bool interpret_GrantTokens();
    bool interpret_RevokeTokens();
    bool interpret_ChangeIssuer();
    bool interpret_Activation();
    bool interpret_Deactivation();
    bool interpret_Alert();
    bool interpret_AcceptOfferBTC();
    ///////////////////////////////////////////////
    /** New things for Contract */
    bool interpret_ContractDexTrade();
    bool interpret_CreateContractDex();
    bool interpret_ContractDexCancelPrice();
    bool interpret_ContractDexCancelEcosystem();
    bool interpret_CreatePeggedCurrency();
    bool interpret_RedemptionPegged();
    bool interpret_SendPeggedCurrency();
    bool interpret_ContractDexClosePosition();
    bool interpret_ContractDex_Cancel_Orders_By_Block();
    bool interpret_MetaDExTrade();
    bool interpret_TradeOffer();
    bool interpret_DExBuy();
    ///////////////////////////////////////////////

    /**
     * Logic and "effects"
     */
    int logicMath_SimpleSend();
    int logicMath_SendAll();
    int logicMath_CreatePropertyFixed();
    int logicMath_CreatePropertyVariable();
    int logicMath_CloseCrowdsale();
    int logicMath_CreatePropertyManaged();
    int logicMath_GrantTokens();
    int logicMath_RevokeTokens();
    int logicMath_ChangeIssuer();
    int logicMath_Activation();
    int logicMath_Deactivation();
    int logicMath_Alert();
    ///////////////////////////////////////////////
    /** New things for Contract */
    int logicMath_ContractDexTrade();
    int logicMath_CreateContractDex();
    int logicMath_ContractDexCancelPrice();
    int logicMath_ContractDexCancelEcosystem();
    int logicMath_CreatePeggedCurrency();
    int logicMath_RedemptionPegged();
    int logicMath_SendPeggedCurrency();
    int logicMath_ContractDexClosePosition();
    int logicMath_ContractDex_Cancel_Orders_By_Block();
    int logicMath_MetaDExTrade();
    int logicMath_TradeOffer();
    int logicMath_AcceptOfferBTC();
    int logicMath_DExBuy();
    ///////////////////////////////////////////////

    /**
     * Logic helpers
     */
    int logicHelper_CrowdsaleParticipation();

public:
  //! DEx and MetaDEx action values
  enum ActionTypes
  {
      INVALID = 0,

      // DEx
      NEW = 1,
      UPDATE = 2,
      CANCEL = 3,

      // MetaDEx
      ADD                 = 1,
      CANCEL_AT_PRICE     = 2,
      CANCEL_ALL_FOR_PAIR = 3,
      CANCEL_EVERYTHING   = 4,
  };
    uint256 getHash() const { return txid; }
    unsigned int getType() const { return type; }
    std::string getTypeString() const { return strTransactionType(getType()); }
    unsigned int getProperty() const { return property; }
    unsigned short getVersion() const { return version; }
    unsigned short getPropertyType() const { return prop_type; }
    uint64_t getFeePaid() const { return tx_fee_paid; }
    std::string getSender() const { return sender; }
    std::string getReceiver() const { return receiver; }
    std::string getPayload() const { return HexStr(pkt, pkt + pkt_size); }
    uint64_t getAmount() const { return nValue; }
    uint64_t getNewAmount() const { return nNewValue; }
    uint8_t getEcosystem() const { return ecosystem; }
    uint32_t getPreviousId() const { return prev_prop_id; }
    std::string getSPCategory() const { return category; }
    std::string getSPSubCategory() const { return subcategory; }
    std::string getSPTxId() const { return stxid; }
    std::string getSPName() const { return name; }
    std::string getSPUrl() const { return url; }
    std::string getSPData() const { return data; }
    int64_t getDeadline() const { return deadline; }
    uint8_t getEarlyBirdBonus() const { return early_bird; }
    uint8_t getIssuerBonus() const { return percentage; }
    bool isRpcOnly() const { return rpcOnly; }
    int getEncodingClass() const { return encodingClass; }
    uint16_t getAlertType() const { return alert_type; }
    uint32_t getAlertExpiry() const { return alert_expiry; }
    std::string getAlertMessage() const { return alert_text; }
    int getPayloadSize() const { return pkt_size; }
    uint16_t getFeatureId() const { return feature_id; }
    uint32_t getActivationBlock() const { return activation_block; }
    uint32_t getMinClientVersion() const { return min_client_version; }
    unsigned int getIndexInBlock() const { return tx_idx; }
    ////////////////////////////////
    /** New things for Contracts */
    uint32_t getMarginRequirement() const { return margin_requirement; }
    uint32_t getNotionalSize() const { return notional_size; }
    uint32_t getContractId() const { return contractId; }
    uint64_t getContractAmount() const { return amount; }
    ////////////////////////////////

    ///////////////////////////////////////////////
    /** New things for Contract */
    int getLogicMath_ContractDexTrade() { return logicMath_ContractDexTrade(); }
    int getLogicMath_CreateContractDex() { return logicMath_CreateContractDex(); }
    ///////////////////////////////////////////////

    /** Creates a new CMPTransaction object. */
    CMPTransaction()
    {
        SetNull();
    }

    /** Resets and clears all values. */
    void SetNull()
    {
        txid.SetNull();
        block = -1;
        blockTime = 0;
        tx_idx = 0;
        tx_fee_paid = 0;
        pkt_size = 0;
        memset(&pkt, 0, sizeof(pkt));
        encodingClass = 0;
        sender.clear();
        receiver.clear();
        type = 0;
        version = 0;
        nValue = 0;
        nNewValue = 0;
        property = 0;
        ecosystem = 0;
        prop_type = 0;
        prev_prop_id = 0;
        memset(&category, 0, sizeof(category));
        memset(&subcategory, 0, sizeof(subcategory));
        memset(&name, 0, sizeof(name));
        memset(&url, 0, sizeof(url));
        memset(&data, 0, sizeof(data));
        memset(&stxid, 0, sizeof(stxid));
        deadline = 0;
        early_bird = 0;
        percentage = 0;
        alert_type = 0;
        alert_expiry = 0;
        memset(&alert_text, 0, sizeof(alert_text));
        rpcOnly = true;
        feature_id = 0;
        activation_block = 0;
        min_client_version = 0;
        desired_property = 0;
        desired_value = 0;
        action = 0;
        amount_desired = 0;
        blocktimelimit = 0;
        min_fee = 0;
        subaction = 0;
        alert_type = 0;
        alert_expiry = 0;
        distribution_property = 0;
        ////////////////////////////////////
        /** New things for Contracts */
        effective_price = 0;
        trading_action = 0;
        propertyId = 0;
        contractId = 0;
        amount = 0;
        amountDesired = 0;
        timeLimit = 0;
        denomination = 0;
        ////////////////////////////////////
    }

    /** Sets the given values. */
    void Set(const uint256& t, int b, unsigned int idx, int64_t bt)
    {
        txid = t;
        block = b;
        tx_idx = idx;
        blockTime = bt;
    }

    /** Sets the given values. */
    void Set(const std::string& s, const std::string& r, uint64_t n, const uint256& t,
        int b, unsigned int idx, unsigned char *p, unsigned int size, int encodingClassIn, uint64_t txf)
    {
        sender = s;
        receiver = r;
        txid = t;
        block = b;
        tx_idx = idx;
        pkt_size = size < sizeof (pkt) ? size : sizeof (pkt);
        nValue = n;
        nNewValue = n;
        encodingClass = encodingClassIn;
        tx_fee_paid = txf;
        memcpy(&pkt, p, pkt_size);
    }

    /** Parses the packet or payload. */
    bool interpret_Transaction();

    /** Interprets the payload and executes the logic. */
    int interpretPacket();

    /** Enables access of interpretPacket. */
    void unlockLogic() { rpcOnly = false; };

    /** Compares transaction objects based on block height and position within the block. */
    bool operator<(const CMPTransaction& other) const
    {
        if (block != other.block) return block > other.block;
        return tx_idx > other.tx_idx;
    }
};

/** Parses a transaction and populates the CMPTransaction object. */
int ParseTransaction(const CTransaction& tx, int nBlock, unsigned int idx, CMPTransaction& mptx, unsigned int nTime=0);


#endif // OMNICORE_TX_H
