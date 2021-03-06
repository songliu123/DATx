/**
 *  @file
 *  @copyright defined in datx/LICENSE.txt
 */
#include <datxos/core_api_accessory/core_api_accessory.hpp>
#include <datxos/chain/exceptions.hpp>

#include <fc/io/json.hpp>

namespace datxos {

static appbase::abstract_accessory& _core_api_accessory = app().register_accessory<core_api_accessory>();

using namespace datxos;

class core_api_accessory_impl {
public:
   core_api_accessory_impl(controller& db)
      : db(db) {}

   controller& db;
};


core_api_accessory::core_api_accessory(){}
core_api_accessory::~core_api_accessory(){}

void core_api_accessory::set_program_options(options_description&, options_description&) {}
void core_api_accessory::accessory_initialize(const variables_map&) {}

struct async_result_visitor : public fc::visitor<std::string> {
   template<typename T>
   std::string operator()(const T& v) const {
      return fc::json::to_string(v);
   }
};

#define CALL(api_name, api_handle, api_namespace, call_name, http_response_code) \
{std::string("/v1/" #api_name "/" #call_name), \
   [this, api_handle](string, string body, url_response_callback cb) mutable { \
          api_handle.validate(); \
          try { \
             if (body.empty()) body = "{}"; \
             auto result = api_handle.call_name(fc::json::from_string(body).as<api_namespace::call_name ## _params>()); \
             cb(http_response_code, fc::json::to_string(result)); \
          } catch (...) { \
             http_accessory::handle_exception(#api_name, #call_name, body, cb); \
          } \
       }}

#define CALL_ASYNC(api_name, api_handle, api_namespace, call_name, call_result, http_response_code) \
{std::string("/v1/" #api_name "/" #call_name), \
   [this, api_handle](string, string body, url_response_callback cb) mutable { \
      if (body.empty()) body = "{}"; \
      api_handle.validate(); \
      api_handle.call_name(fc::json::from_string(body).as<api_namespace::call_name ## _params>(),\
         [cb, body](const fc::static_variant<fc::exception_ptr, call_result>& result){\
            if (result.contains<fc::exception_ptr>()) {\
               try {\
                  result.get<fc::exception_ptr>()->dynamic_rethrow_exception();\
               } catch (...) {\
                  http_accessory::handle_exception(#api_name, #call_name, body, cb);\
               }\
            } else {\
               cb(http_response_code, result.visit(async_result_visitor()));\
            }\
         });\
   }\
}

#define CHAIN_RO_CALL(call_name, http_response_code) CALL(chain, ro_api, chain_apis::read_only, call_name, http_response_code)
#define CHAIN_RW_CALL(call_name, http_response_code) CALL(chain, rw_api, chain_apis::read_write, call_name, http_response_code)
#define CHAIN_RO_CALL_ASYNC(call_name, call_result, http_response_code) CALL_ASYNC(chain, ro_api, chain_apis::read_only, call_name, call_result, http_response_code)
#define CHAIN_RW_CALL_ASYNC(call_name, call_result, http_response_code) CALL_ASYNC(chain, rw_api, chain_apis::read_write, call_name, call_result, http_response_code)

void core_api_accessory::accessory_startup() {
   ilog( "starting core_api_accessory" );
   my.reset(new core_api_accessory_impl(app().get_accessory<core_accessory>().chain()));
   auto ro_api = app().get_accessory<core_accessory>().get_read_only_api();
   auto rw_api = app().get_accessory<core_accessory>().get_read_write_api();

   app().get_accessory<http_accessory>().add_api({
      CHAIN_RO_CALL(get_info, 200l),
      CHAIN_RO_CALL(get_block, 200),
      CHAIN_RO_CALL(get_block_header_state, 200),
      CHAIN_RO_CALL(get_account, 200),
      CHAIN_RO_CALL(get_code, 200),
      CHAIN_RO_CALL(get_abi, 200),
      CHAIN_RO_CALL(get_raw_code_and_abi, 200),
      CHAIN_RO_CALL(get_table_rows, 200),
      CHAIN_RO_CALL(get_currency_balance, 200),
      CHAIN_RO_CALL(get_currency_stats, 200),
      CHAIN_RO_CALL(get_producers, 200),
      CHAIN_RO_CALL(get_producer_schedule, 200),
      CHAIN_RO_CALL(get_scheduled_transactions, 200),
      CHAIN_RO_CALL(abi_json_to_bin, 200),
      CHAIN_RO_CALL(abi_bin_to_json, 200),
      CHAIN_RO_CALL(get_required_keys, 200),
      CHAIN_RO_CALL(get_transaction_id, 200),
      CHAIN_RW_CALL_ASYNC(push_block, chain_apis::read_write::push_block_results, 202),
      CHAIN_RW_CALL_ASYNC(push_transaction, chain_apis::read_write::push_transaction_results, 202),
      CHAIN_RW_CALL_ASYNC(push_transactions, chain_apis::read_write::push_transactions_results, 202)
   });
}

void core_api_accessory::accessory_shutdown() {}

}
