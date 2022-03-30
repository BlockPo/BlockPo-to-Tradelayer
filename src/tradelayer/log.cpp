#include <tradelayer/log.h>

#include <chainparamsbase.h>
#include <util/system.h>
#include <util/time.h>

#include <assert.h>
#include <atomic>
#include <mutex>
#include <stdio.h>
#include <string>
#include <vector>

// Default log files
const std::string LOG_FILENAME    = "tradelayer.log";

// Options
static const long LOG_BUFFERSIZE  =  8000000; //  8 MB
static const long LOG_SHRINKSIZE  = 50000000; // 50 MB

// Debug flags
bool msc_debug_parser_data                      = 0;
bool msc_debug_parser_readonly                  = 0;
bool msc_debug_parser                           = 0;
bool msc_debug_verbose                          = 0;
bool msc_debug_verbose2                         = 0;
bool msc_debug_verbose3                         = 0;
bool msc_debug_vin                              = 0;
bool msc_debug_script                           = 0;
bool msc_debug_send                             = 0;
bool msc_debug_tokens                           = 0;
bool msc_debug_spec                             = 0;
bool msc_debug_exo                              = 0;
bool msc_debug_tally                            = 0;
bool msc_debug_sp                               = 0;
bool msc_debug_txdb                             = 0;
bool msc_debug_persistence                      = 0;
bool msc_debug_ui                               = 0;
bool msc_debug_pending                          = 0;
bool msc_debug_packets                          = 0;
bool msc_debug_packets_readonly                 = 1;
bool msc_debug_walletcache                      = 0;
bool msc_debug_consensus_hash                   = 0;
bool msc_debug_consensus_hash_every_block       = 0;
bool msc_debug_consensus_hash_every_transaction = 0;
bool msc_debug_alerts                           = 0;
bool msc_debug_handle_dex_payment               = 0;
bool msc_debug_handle_instant                   = 0;
bool msc_debug_handler_tx                       = 1;
bool msc_debug_tradedb                          = 0;
bool msc_debug_margin_main                      = 0;
bool msc_debug_pos_margin                       = 0;
bool msc_debug_contract_inst_fee                = 0;
bool msc_debug_instant_x_trade                  = 1;
bool msc_debug_x_trade_bidirectional            = 0;
bool msc_debug_contractdex_fees                 = 0;
bool msc_debug_metadex_fees                     = 0;
bool msc_debug_metadex1                         = 0;
bool msc_debug_metadex2                         = 0;
bool msc_debug_metadex3                         = 0;
bool msc_debug_metadex_add                      = 0;
bool msc_debug_contractdex_add                  = 0;
bool msc_debug_contract_add_market              = 0;
bool msc_debug_contract_cancel_every            = 0;
bool msc_debug_contract_cancel_forblock         = 0;
bool msc_debug_contract_cancel_inorder          = 0;
bool msc_debug_add_orderbook_edge               = 0;
bool msc_debug_close_position                   = 0;
bool msc_debug_get_pair_market_price            = 0;
bool msc_debug_dex                              = 0;
bool msc_debug_contractdex_tx                   = 0;
bool msc_debug_create_pegged                    = 0;
bool msc_debug_accept_offerbtc                  = 0;
bool msc_debug_set_oracle                       = 0;
bool msc_debug_send_pegged                      = 0;
bool msc_debug_commit_channel                   = 0;
bool msc_debug_instant_trade                    = 0;
bool msc_debug_contract_instant_trade           = 1;
bool msc_create_channel                         = 0;
bool msc_debug_new_id_registration              = 0;
bool msc_debug_wallettxs                        = 0;
bool msc_calling_settlement                     = 0;
bool msc_debug_vesting                          = 0;
bool msc_debug_oracle_twap                      = 0;
bool msc_debug_search_all                       = 0;
bool msc_debug_add_contract_ltc_vol             = 0;
bool msc_debug_update_last_block                = 0;
bool msc_debug_send_reward                      = 0;
bool msc_debug_contract_cancel                  = 0;
bool msc_debug_fee_cache_buy                    = 0;
bool msc_debug_check_attestation_reg            = 0;
bool msc_debug_sanity_checks                    = 0;
bool msc_debug_ltc_volume                       = 0;
bool msc_debug_mdex_volume                      = 0;
bool msc_debug_update_status                    = 0;
bool msc_debug_get_channel_addr                 = 0;
bool msc_debug_make_withdrawal                  = 0;
bool msc_debug_check_kyc_register               = 0;
bool msc_debug_update_id_register               = 0;
bool msc_debug_get_transaction_address          = 0;
bool msc_debug_is_mpin_block_range              = 0;
bool msc_debug_record_payment_tx                = 0;
bool msc_tx_valid_node_reward                   = 0;
bool msc_debug_delete_att_register              = 0;
bool msc_debug_get_upn_info                     = 0;
bool msc_debug_get_total_lives                  = 0;
bool msc_debug_activate_feature                 = 0;
bool msc_debug_deactivate_feature               = 0;
bool msc_debug_is_transaction_type_allowed      = 0;
bool msc_debug_instant_payment                  = 0;
bool msc_debug_settlement_algorithm_fifo        = 0;
bool msc_debug_clearing_operator_fifo           = 0;
bool msc_debug_counting_lives_longshorts        = 0;
bool msc_debug_calculate_pnl_forghost           = 0;
bool msc_debug_withdrawal_from_channel          = 0;
bool msc_debug_populate_rpc_transaction_obj     = 0;
bool msc_debug_fill_tx_input_cache              = 0;
bool msc_debug_try_add_second                   = 0;
bool msc_debug_liquidation_enginee              = 0;

/**
 * LogPrintf() has been broken a couple of times now
 * by well-meaning people adding mutexes in the most straightforward way.
 * It breaks because it may be called by global destructors during shutdown.
 * Since the order of destruction of static/global objects is undefined,
 * defining a mutex as a global object doesn't work (the mutex gets
 * destroyed, and then some later destructor calls OutputDebugStringF,
 * maybe indirectly, and you get a core dump at shutdown trying to lock
 * the mutex).
 */
static std::once_flag debugLogInitFlag;
/**
 * We use std::call_once() to make sure these are initialized
 * in a thread-safe manner the first time called:
 */
static FILE* fileout = nullptr;
static std::mutex* mutexDebugLog = nullptr;

/** Flag to indicate, whether the Trade Layer log file should be reopened. */
extern std::atomic<bool> fReopentradelayerLog;

/**
 * Returns path for debug log file.
 *
 * The log file can be specified via startup option "--tllogfile=/path/to/tradelayer.log",
 * and if none is provided, then the client's datadir is used as default location.
 */
static fs::path GetLogPath()
{
    fs::path pathLogFile;
    std::string strLogPath = gArgs.GetArg("-tllogfile", "");

    if (!strLogPath.empty()) {
        pathLogFile = fs::path(strLogPath);
        TryCreateDirectory(pathLogFile.parent_path());
    } else {
        pathLogFile = GetDataDir() / LOG_FILENAME;
    }

    return pathLogFile;
}

/**
 * Opens debug log file.
 */
static void DebugLogInit()
{
    assert(fileout == nullptr);
    assert(mutexDebugLog == nullptr);

    fs::path pathDebug = GetLogPath();
    fileout = fopen(pathDebug.string().c_str(), "a");

    if (fileout) {
        setbuf(fileout, nullptr); // Unbuffered
    } else {
        PrintToConsole("Failed to open debug log file: %s\n", pathDebug.string());
    }

    mutexDebugLog = new std::mutex();
}

/**
 * @return The current timestamp in the format: 2009-01-03 18:15:05
 */
 static std::string GetTimestamp()
 {
     return FormatISO8601DateTime(GetTime());
 }

/**
 * Prints to log file.
 *
 * The configuration options "-logtimestamps" can be used to indicate, whether
 * the message to log should be prepended with a timestamp.
 *
 * If "-printtoconsole" is enabled, then the message is written to the standard
 * output, usually the console, instead of a log file.
 *
 * @param str[in]  The message to log
 * @return The total number of characters written
 */
int LogFilePrint(const std::string& str)
{
    int ret = 0; // Number of characters written
    if (fPrintToConsole) {
        // Print to console
        ret = ConsolePrint(str);

    } else {
        static bool fStartedNewLine = true;
        std::call_once(debugLogInitFlag, &DebugLogInit);

        if (fileout == nullptr)
        {
            return ret;
        }

        std::lock_guard<std::mutex> lock(*mutexDebugLog);

        // Reopen the log file, if requested
        if (fReopenTradeLayerLog)
        {
            fReopenTradeLayerLog = false;
            fs::path pathDebug = GetLogPath();
            if (freopen(pathDebug.string().c_str(), "a", fileout) != nullptr)
                setbuf(fileout, nullptr); // Unbuffered

         }

         // Printing log timestamps can be useful for profiling
         if (fLogTimestamps && fStartedNewLine) {
           ret += fprintf(fileout, "%s ", GetTimestamp().c_str());
         }

         fStartedNewLine = (!str.empty() && str[str.size()-1] == '\n') ? true : false;

         ret += fwrite(str.data(), 1, str.size(), fileout);
    }

    return ret;
}


/**
 * Prints to the standard output, usually the console.
 *
 * The configuration option "-logtimestamps" can be used to indicate, whether
 * the message should be prepended with a timestamp.
 *
 * @param str[in]  The message to print
 * @return The total number of characters written
 */

int ConsolePrint(const std::string& str)
{
    int ret = 0; // Number of characters written
    static bool fStartedNewLine = true;

    if (fLogTimestamps && fStartedNewLine) {
        ret = fprintf(stdout, "%s %s", GetTimestamp().c_str(), str.c_str());
    } else {
        ret = fwrite(str.data(), 1, str.size(), stdout);
    }

    fStartedNewLine = (!str.empty() && str[str.size()-1] == '\n') ? true : false;

    fflush(stdout);

    return ret;
}

/**
 * Determine whether to override compiled debug levels via enumerating startup option --tldebug.
 *
 * Example usage (granular categories)    : --tldebug=parser --tldebug=metadex1 --tldebug=ui
 * Example usage (enable all categories)  : --tldebug=all
 * Example usage (disable all debugging)  : --tldebug=none
 * Example usage (disable all except XYZ) : --tldebug=none --tldebug=parser --tldebug=sto
 */
void InitDebugLogLevels()
{
  if (!gArgs.IsArgSet("-tradelayerdebug"))
      return;

  const std::vector<std::string>& debugLevels = gArgs.GetArgs("-tldebug");

  for (std::vector<std::string>::const_iterator it = debugLevels.begin(); it != debugLevels.end(); ++it) {
          if (*it == "parser_data") msc_debug_parser_data = true;
          if (*it == "parser_readonly") msc_debug_parser_readonly = true;
          if (*it == "parser") msc_debug_parser = true;
          if (*it == "verbose") msc_debug_verbose = true;
          if (*it == "verbose2") msc_debug_verbose2 = true;
          if (*it == "verbose3") msc_debug_verbose3 = true;
          if (*it == "vin") msc_debug_vin = true;
          if (*it == "script") msc_debug_script = true;
          if (*it == "send") msc_debug_send = true;
          if (*it == "tokens") msc_debug_tokens = true;
          if (*it == "spec") msc_debug_spec = true;
          if (*it == "exo") msc_debug_exo = true;
          if (*it == "tally") msc_debug_tally = true;
          if (*it == "sp") msc_debug_sp = true;
          if (*it == "txdb") msc_debug_txdb = true;
          if (*it == "persistence") msc_debug_persistence = true;
          if (*it == "ui") msc_debug_ui = true;
          if (*it == "pending") msc_debug_pending = true;
          if (*it == "packets") msc_debug_packets = true;
          if (*it == "packets_readonly") msc_debug_packets_readonly = true;
          if (*it == "walletcache") msc_debug_walletcache = true;
          if (*it == "consensus_hash") msc_debug_consensus_hash = true;
          if (*it == "consensus_hash_every_block") msc_debug_consensus_hash_every_block = true;
          if (*it == "consensus_hash_every_transaction") msc_debug_consensus_hash_every_transaction = true;
          if (*it == "alerts") msc_debug_alerts = true;
          if (*it == "none" || *it == "all") {
              bool allDebugState = false;
              if (*it == "all") allDebugState = true;
              msc_debug_parser_data = allDebugState;
              msc_debug_parser_readonly = allDebugState;
              msc_debug_parser = allDebugState;
              msc_debug_verbose = allDebugState;
              msc_debug_verbose2 = allDebugState;
              msc_debug_verbose3 = allDebugState;
              msc_debug_vin = allDebugState;
              msc_debug_script = allDebugState;
              msc_debug_send = allDebugState;
              msc_debug_tokens = allDebugState;
              msc_debug_spec = allDebugState;
              msc_debug_exo = allDebugState;
              msc_debug_tally = allDebugState;
              msc_debug_sp = allDebugState;
              msc_debug_txdb = allDebugState;
              msc_debug_persistence = allDebugState;
              msc_debug_ui = allDebugState;
              msc_debug_pending = allDebugState;
              msc_debug_packets =  allDebugState;
              msc_debug_packets_readonly =  allDebugState;
              msc_debug_walletcache = allDebugState;
              msc_debug_alerts = allDebugState;
          }
      }
}

/**
 * Scrolls debug log, if it's getting too big.
 */
void ShrinkDebugLog()
{
    fs::path pathLog = GetLogPath();
    FILE* file = fopen(pathLog.string().c_str(), "r");

    if (file && fs::file_size(pathLog) > LOG_SHRINKSIZE) {
        // Restart the file with some of the end
        char* pch = new char[LOG_BUFFERSIZE];
        if (nullptr != pch) {
            fseek(file, -LOG_BUFFERSIZE, SEEK_END);
            int nBytes = fread(pch, 1, LOG_BUFFERSIZE, file);
            fclose(file);
            file = nullptr;

            file = fopen(pathLog.string().c_str(), "w");
            if (file) {
                fwrite(pch, 1, nBytes, file);
                fclose(file);
                file = nullptr;
            }
            delete[] pch;
        }
    } else if (nullptr != file) {
        fclose(file);
        file = nullptr;
    }
}
