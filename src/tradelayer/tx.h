#ifndef TRADELAYER_TX_H
#define TRADELAYER_TX_H

class CMPMetaDEx;
class CMPOffer;
class CTransaction;
class CMPContractDex;

#include <tradelayer/tradelayer.h>

#include <uint256.h>
#include <util/strencodings.h>

#include <stdint.h>
#include <string>
#include <numeric>

using mastercore::strTransactionType;

/** The class is responsible for transaction interpreting/parsing.
 *
 * It invokes other classes and methods: offers, accepts, tallies (balances).
 */

class CMPTransaction
{
    friend class CMPMetaDEx;
    friend class CMPOffer;
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
    std::string special;

    unsigned int type;
    unsigned short version; // = MP_TX_PKT_V0;

    // SimpleSend, SendToOwners, TradeOffer, MetaDEx, AcceptOfferBTC,
    // CreatePropertyFixed, GrantTokens, RevokeTokens
    uint64_t nValue;
    uint64_t nNewValue;

    // SendMany
    std::vector<std::string> recipients;
    std::vector<uint64_t> values;

    // SimpleSend, SendToOwners, TradeOffer, MetaDEx, AcceptOfferBTC,
    // CreatePropertyFixed,
    // CreatePropertyMananged, GrantTokens, RevokeTokens, ChangeIssuer
    unsigned int property;

    // CreatePropertyFixed,  CreatePropertyMananged, MetaDEx, SendAll
    unsigned int distribution_property;

    // CreatePropertyFixed, CreatePropertyMananged
    unsigned short prop_type;
    unsigned int prev_prop_id;
    char category[SP_STRING_FIELD_LEN];
    char subcategory[SP_STRING_FIELD_LEN];
    /* New things for contracts */
    char stxid[SP_STRING_FIELD_LEN];
    char name_traded[SP_STRING_FIELD_LEN];
    //////////////////////////////////
    char name[SP_STRING_FIELD_LEN];
    char url[SP_STRING_FIELD_LEN];
    char data[SP_STRING_FIELD_LEN];

    uint64_t deadline;
    unsigned char early_bird;
    unsigned char percentage;

    // MetaDEx
    unsigned int desired_property;
    int64_t desired_value;
    int64_t amount_forsale;
    unsigned char action; // depreciated

    // DEX 1
    uint64_t amountDesired;
    uint8_t timeLimit;
    uint64_t minFee;
    uint8_t subAction;
    uint8_t option; // buy=1 , sell=2

    /** New things for Contract */
    uint64_t effective_price;
    uint8_t trading_action;
    uint32_t propertyId;
    uint32_t contractId;
    uint64_t amount;
    uint32_t numerator;
    uint32_t denominator;
    bool inverse_quoted = true;

    /* uint64_t ticksize; */
    /*uint32_t nextContractId;*/
    uint64_t oracle_high;
    uint64_t oracle_low;
    uint64_t oracle_close;
    uint32_t blocks_until_expiration;
    uint32_t notional_size;
    uint32_t collateral_currency;
    uint64_t margin_requirement;
    uint32_t attribute_type;
    int64_t leverage;

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

    //Multisig channels
    char channel_address[SP_STRING_FIELD_LEN];
    uint64_t amount_commited;
    uint64_t amount_to_withdraw;
    uint64_t pnl_amount;
    uint64_t vOut;
    uint64_t price;
    uint64_t ileverage;
    uint64_t itrading_action;
    uint64_t instant_amount;
    bool address_option;  // false: first address, true: second address
    uint64_t amount_transfered;


    //KYC
    char company_name[SP_STRING_FIELD_LEN];
    char website[SP_STRING_FIELD_LEN];
    char hash[SP_STRING_FIELD_LEN];
    int block_forexpiry;
    uint8_t tokens, ltc, natives, oracles;
    std::vector<int64_t> kyc_Ids; //kyc vector


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
    bool interpret_SendMany();
    bool interpret_SendAll();
    bool interpret_CreatePropertyFixed();
    bool interpret_CreatePropertyManaged();
    bool interpret_GrantTokens();
    bool interpret_RevokeTokens();
    bool interpret_ChangeIssuer();
    bool interpret_Activation();
    bool interpret_Deactivation();
    bool interpret_Alert();
    bool interpret_AcceptOfferBTC();
    bool interpret_SubmitNodeAddr();
    bool interpret_ClaimNodeReward();

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
    bool interpret_MetaDExCancelAll();
    bool interpret_DExSell();
    bool interpret_DExBuy();
    bool interpret_SendVestingTokens();
    bool interpret_CreateOracleContract();
    bool interpret_Change_OracleAdm();
    bool interpret_Set_Oracle();
    bool interpret_OracleBackup();
    bool interpret_CloseOracle();
    bool interpret_CommitChannel();
    bool interpret_Withdrawal_FromChannel();
    bool interpret_Instant_Trade();
    bool interpret_Update_PNL();
    bool interpret_Transfer();
    bool interpret_Create_Channel();
    bool interpret_Contract_Instant();
    bool interpret_New_Id_Registration();
    bool interpret_Update_Id_Registration();
    bool interpret_DEx_Payment();
    bool interpret_Attestation();
    bool interpret_Revoke_Attestation();
    bool interpret_ContractDExCancel();
    bool interpret_Instant_LTC_Trade();
    bool interpret_MetaDExCancel();
    bool interpret_MetaDExCancel_ByPair();
    bool interpret_MetaDExCancel_ByPrice();
    bool interpret_Close_Channel();
    bool interpret_SendDonation();

    /**
     * Logic and "effects"
     */
    int logicMath_SimpleSend();
    int logicMath_SendMany();
    int logicMath_SendAll();
    int logicMath_CreatePropertyFixed();
    int logicMath_CreatePropertyManaged();
    int logicMath_GrantTokens();
    int logicMath_RevokeTokens();
    int logicMath_ChangeIssuer();
    int logicMath_Activation();
    int logicMath_Deactivation();
    int logicMath_Alert();
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
    int logicMath_MetaDExCancelAll();
    int logicMath_AcceptOfferBTC();
    int logicMath_DExSell();
    int logicMath_DExBuy();
    int logicMath_SendVestingTokens();
    int logicMath_CreateOracleContract();
    int logicMath_Change_OracleAdm();
    int logicMath_Set_Oracle();
    int logicMath_OracleBackup();
    int logicMath_CloseOracle();
    int logicMath_CommitChannel();
    int logicMath_Withdrawal_FromChannel();
    int logicMath_Instant_Trade();
    int logicMath_Update_PNL();
    int logicMath_Transfer();
    int logicMath_Create_Channel();
    int logicMath_Contract_Instant();
    int logicMath_New_Id_Registration();
    int logicMath_Update_Id_Registration();
    int logicMath_DEx_Payment();
    int logicMath_Attestation();
    int logicMath_Revoke_Attestation();
    int logicMath_ContractDExCancel();
    int logicMath_Instant_LTC_Trade();
    int logicMath_MetaDExCancel();
    int logicMath_MetaDExCancel_ByPair();
    int logicMath_MetaDExCancel_ByPrice();
    int logicMath_Close_Channel();
    int logicMath_SubmitNodeAddr();
    int logicMath_ClaimNodeReward();
    int logicMath_SendDonation();

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
    std::string getSpecial() const { return special; }
    std::string getPayload() const { return HexStr(pkt, pkt + pkt_size); }
    uint64_t getAmount() const { return nValue; }
    uint64_t getAmountTotal() const { return std::accumulate(values.cbegin(), values.cend(), static_cast<uint64_t>(0)); }
    uint64_t getNewAmount() const { return nNewValue; }
    uint64_t getXAmount() const { return amount; }
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
    uint8_t getTimeLimit() const { return timeLimit; }
    uint64_t getMinFee() const { return minFee; };
    uint8_t getSubAction() const { return subAction; };
    uint32_t getPropertyId() const { return propertyId; }

    /** Oracles */
    uint64_t getHighPrice() const { return oracle_high; }
    uint64_t getLowPrice() const { return oracle_low; }
    uint64_t getClosePrice() const { return oracle_close; }
    int getBlock() const { return block; }

    /** Instan trade (Channels) */
    uint64_t getAmountForSale() const { return amount_forsale; }
    uint32_t getDesiredProperty() const { return desired_property; }
    uint64_t getDesiredValue() const { return desired_value; }
    uint64_t getAmountCommited() const { return amount_commited; }
    uint64_t getAmountToWith() const { return amount_to_withdraw; }
    int getBlockForExpiry() const { return block_forexpiry; }
    uint64_t getPrice() const { return price; }
    uint8_t getItradingAction() { return itrading_action; }
    uint64_t getIleverage() { return ileverage; }


    /** New things for Contracts */
    uint64_t getMarginRequirement() const { return margin_requirement; }
    uint32_t getNotionalSize() const { return notional_size; }
    uint32_t getContractId() const { return contractId; }
    uint64_t getContractAmount() const { return amount; }
    uint32_t getCollateral() const { return collateral_currency; }
    int getBlockUntilExpiration() const { return blocks_until_expiration;}
    int getLogicMath_ContractDexTrade() { return logicMath_ContractDexTrade(); }
    int getLogicMath_CreateContractDex() { return logicMath_CreateContractDex(); }
    uint8_t getTradingAction() { return trading_action; }
    uint64_t getLeverage() { return leverage; }
    uint64_t getEffectivePrice() { return effective_price; }



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
        special.clear();
        values.clear();
        type = 0;
        version = 0;
        nValue = 0;
        nNewValue = 0;
        property = 0;
        prop_type = 0;
        prev_prop_id = 0;
        memset(&category, 0, sizeof(category));
        memset(&subcategory, 0, sizeof(subcategory));
        memset(&name, 0, sizeof(name));
        memset(&url, 0, sizeof(url));
        memset(&data, 0, sizeof(data));
        memset(&stxid, 0, sizeof(stxid));
	      memset(&name_traded, 0, sizeof(name_traded));
        memset(&channel_address, 0, sizeof(channel_address));
        memset(&website, 0, sizeof(website));
        memset(&hash, 0, sizeof(hash));
        memset(&company_name, 0, sizeof(company_name));
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

        /** New things for Contracts */
        effective_price = 0;
        trading_action = 0;
        propertyId = 0;
        contractId = 0;
        amount = 0;
        amountDesired = 0;
        timeLimit = 0;
        denominator = 0;
        numerator = 0;
        leverage = 0;

        //Multisig channels
        amount_commited = 0;
        amount_to_withdraw = 0;
        vOut = 0;
        block_forexpiry = 0;
        pnl_amount= 0;
        ileverage = 0;
        itrading_action = 0;
        instant_amount = 0;

        //Kyc
        tokens = 0;
        ltc = 0;
        natives = 0;
        oracles = 0;
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
    void Set(const std::string& s, const std::string& r, const std::string& sp, uint64_t n, const uint256& t,
        int b, unsigned int idx, unsigned char *p, unsigned int size, int encodingClassIn, uint64_t txf)
    {
        sender = s;
        receiver = r;
        special  = sp;
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

    void SetAddresses(const std::vector<std::string>& addresses)
    {
      this->recipients = addresses;
    }

    /** Parses the packet or payload. */
    bool interpret_Transaction();

    /** Interprets the payload and executes the logic. */
    int interpretPacket();

    /** Enables access of interpretPacket. */
    void unlockLogic() { rpcOnly = false; };

    /** Compares transaction objects based on block height and position within the block. */
    bool operator < (const CMPTransaction& other) const
    {
        if (block != other.block) return block > other.block;
        return tx_idx > other.tx_idx;
    }
};

/** Parses a transaction and populates the CMPTransaction object. */
int ParseTransaction(const CTransaction& tx, int nBlock, unsigned int idx, CMPTransaction& mptx, unsigned int nTime=0);

struct FutureContractObject
{
  static FutureContractObject Empty;

  uint32_t fco_numerator;
  uint32_t fco_denominator;
  uint32_t fco_blocks_until_expiration;
  uint32_t fco_notional_size;
  uint32_t fco_collateral_currency;
  uint64_t fco_margin_requirement;
  uint32_t fco_propertyId;
  uint16_t fco_prop_type;
  bool fco_expirated;
  bool fco_quoted;

  int fco_init_block;
  std::string fco_name;
  // std::string fco_subcategory;
  std::string fco_issuer;
  std::string fco_backup_address;

  FutureContractObject():fco_numerator(0), fco_denominator(0),
    fco_blocks_until_expiration(0), fco_notional_size(0), fco_collateral_currency(0),
    fco_margin_requirement(0), fco_propertyId(0), fco_prop_type(0), fco_expirated(false),
    fco_quoted(false), fco_init_block(0), fco_name(""), fco_issuer(""), fco_backup_address("")  {}
};

struct TokenDataByName
{
  static TokenDataByName Empty;

  uint32_t data_denominator;
  uint32_t data_blocks_until_expiration;
  uint32_t data_notional_size;
  uint32_t data_collateral_currency;
  uint64_t data_margin_requirement;
  uint32_t data_propertyId;

  int data_init_block;
  std::string data_name;
  std::string data_subcategory;
  std::string data_issuer;

  TokenDataByName() : data_denominator(), data_blocks_until_expiration(), data_notional_size(), 
    data_collateral_currency(), data_margin_requirement(), data_propertyId(), data_init_block() {}
};

/**********************************************************************/
/**Class for Node Reward**/

class BlockClass
{
 private:

   int m_BlockInit;
   int m_BlockNow;
   double RewardSecndI;
   double RewardFirstI;

 public:

 BlockClass(int BlockInit, int BlockNow) : m_BlockInit(BlockInit), m_BlockNow(BlockNow) {}
 BlockClass(const BlockClass &p) : m_BlockInit(p.m_BlockInit), m_BlockNow(p.m_BlockNow) {}
 ~BlockClass() {}
 BlockClass &operator=(const BlockClass &p) {
   if (this != &p)
   {
     m_BlockInit = p.m_BlockInit; m_BlockNow = p.m_BlockNow;
   }

   return *this;
 }
 void SendNodeReward(std::string sender);
};

int64_t LosingSatoshiLongTail(int BlockNow, int64_t Reward);
/**********************************************************************/


struct oracledata
{
  int64_t high;
  int64_t low;
  int64_t close;
  uint32_t contractId;
};


struct withdrawalAccepted
{
  std::string address;
  int deadline_block;
  uint32_t propertyId;
  uint64_t amount;
  uint256 txid;

  withdrawalAccepted() : address(""), deadline_block(0), propertyId(0), amount(0) {}
};

//! Prices for each oracle contract
extern std::map<uint32_t,std::map<int,oracledata>> oraclePrices;

//! Pending withdrawals
extern std::map<std::string,vector<withdrawalAccepted>> withdrawal_Map;

FutureContractObject getFutureContractObject(std::string identifier);
TokenDataByName getTokenDataByName(std::string identifier);
TokenDataByName getTokenDataById(uint32_t propertyId);

#endif // TRADELAYER_TX_H
