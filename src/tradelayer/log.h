#ifndef TRADELAYER_LOG_H
#define TRADELAYER_LOG_H

#include "util.h"
#include "tinyformat.h"

#include <string>

/** Prints to the log file. */
int LogFilePrint(const std::string& str);

/** Prints to the console. */
int ConsolePrint(const std::string& str);

/** Determine whether to override compiled debug levels. */
void InitDebugLogLevels();

/** Scrolls log file, if it's getting too big. */
void ShrinkDebugLog();

// Debug flags
extern bool msc_debug_parser_data;
extern bool msc_debug_parser_readonly;
extern bool msc_debug_parser;
extern bool msc_debug_verbose;
extern bool msc_debug_verbose2;
extern bool msc_debug_verbose3;
extern bool msc_debug_vin;
extern bool msc_debug_script;
extern bool msc_debug_send;
extern bool msc_debug_tokens;
extern bool msc_debug_spec;
extern bool msc_debug_exo;
extern bool msc_debug_tally;
extern bool msc_debug_sp;
extern bool msc_debug_txdb;
extern bool msc_debug_persistence;
extern bool msc_debug_ui;
extern bool msc_debug_pending;
extern bool msc_debug_packets;
extern bool msc_debug_packets_readonly;
extern bool msc_debug_walletcache;
extern bool msc_debug_consensus_hash;
extern bool msc_debug_consensus_hash_every_block;
extern bool msc_debug_consensus_hash_every_transaction;
extern bool msc_debug_alerts;
extern bool msc_debug_metadex1;
extern bool msc_debug_metadex2;
extern bool msc_debug_metadex3;
extern bool msc_debug_tradedb;
extern bool msc_debug_dex;
extern bool msc_debug_fees;
extern bool msc_debug_sto;
extern bool msc_debug_parser_data;
extern bool msc_debug_handle_dex_payment;
extern bool msc_debug_handle_instant;
extern bool msc_debug_handler_tx;
extern bool msc_debug_margin_main;


extern bool msc_debug_pos_margin;
extern bool msc_debug_contract_inst_fee;
extern bool msc_debug_instant_x_trade;
extern bool msc_debug_x_trade_bidirectional;
extern bool msc_debug_contractdex_fees;
extern bool msc_debug_metadex_fees;
extern bool msc_debug_metadex1;
extern bool msc_debug_metadex2;
extern bool msc_debug_metadex3;
extern bool msc_debug_metadex_add;
extern bool msc_debug_contractdex_tx;
extern bool msc_debug_contractdex_add;
extern bool msc_debug_contract_add_market;
extern bool msc_debug_contract_cancel_every;
extern bool msc_debug_contract_cancel_forblock;
extern bool msc_debug_contract_cancel_inorder;
extern bool msc_debug_add_orderbook_edge;
extern bool msc_debug_close_position;
extern bool msc_debug_get_pair_market_price;
extern bool msc_debug_create_pegged;
extern bool msc_debug_accept_offerbtc;
extern bool msc_debug_set_oracle;
extern bool msc_debug_send_pegged;
extern bool msc_debug_commit_channel;
extern bool msc_debug_withdrawal_from_channel;
extern bool msc_debug_instant_trade;
extern bool msc_create_channel;
extern bool msc_debug_contract_instant_trade;
extern bool msc_debug_new_id_registration;
extern bool msc_debug_wallettxs;
extern bool msc_calling_settlement;
extern bool msc_debug_vesting;
extern bool msc_debug_oracle_twap;
extern bool msc_debug_search_all;
extern bool msc_debug_add_contract_ltc_vol;
extern bool msc_debug_update_last_block;
extern bool msc_debug_send_reward;
extern bool msc_debug_contract_cancel;
extern bool msc_debug_fee_cache_buy;
extern bool msc_debug_check_attestation_reg;

/* When we switch to C++11, this can be switched to variadic templates instead
 * of this macro-based construction (see tinyformat.h).
 */
#define MAKE_TRADE_LAYER_ERROR_AND_LOG_FUNC(n)                                    \
    template<TINYFORMAT_ARGTYPES(n)>                                            \
    static inline int PrintToLog(const char* format, TINYFORMAT_VARARGS(n))     \
    {                                                                           \
        return LogFilePrint(tfm::format(format, TINYFORMAT_PASSARGS(n)));       \
    }                                                                           \
    template<TINYFORMAT_ARGTYPES(n)>                                            \
    static inline int PrintToLog(TINYFORMAT_VARARGS(n))                         \
    {                                                                           \
        return LogFilePrint(tfm::format("%s", TINYFORMAT_PASSARGS(n)));         \
    }                                                                           \
    template<TINYFORMAT_ARGTYPES(n)>                                            \
    static inline int PrintToConsole(const char* format, TINYFORMAT_VARARGS(n)) \
    {                                                                           \
        return ConsolePrint(tfm::format(format, TINYFORMAT_PASSARGS(n)));       \
    }                                                                           \
    template<TINYFORMAT_ARGTYPES(n)>                                            \
    static inline int PrintToConsole(TINYFORMAT_VARARGS(n))                     \
    {                                                                           \
        return ConsolePrint(tfm::format("%s", TINYFORMAT_PASSARGS(n)));         \
    }

TINYFORMAT_FOREACH_ARGNUM(MAKE_TRADE_LAYER_ERROR_AND_LOG_FUNC)

#undef MAKE_TRADE_LAYER_ERROR_AND_LOG_FUNC


#endif // TRADELAYER_LOG_H
