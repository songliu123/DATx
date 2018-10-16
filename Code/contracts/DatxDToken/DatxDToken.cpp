/**
 *  @file
 *  @copyright defined in datx/LICENSE.txt
 */

#include "DatxDToken.hpp"
#include <DatxioLib/transaction.hpp>
#include <DatxioLib/crypto.h>


namespace datxio {

void token::create( account_name issuer,
                    asset        maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    datxio_assert( sym.is_valid(), "invalid symbol name" );
    datxio_assert( maximum_supply.is_valid(), "invalid supply");
    datxio_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.name() );
    auto existing = statstable.find( sym.name() );
    datxio_assert( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}


void token::issue( account_name to, asset quantity, string memo )
{
    auto sym = quantity.symbol;
    datxio_assert( sym.is_valid(), "invalid symbol name" );
    datxio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    auto sym_name = sym.name();
    stats statstable( _self, sym_name );
    auto existing = statstable.find( sym_name );
    datxio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( st.issuer );
    datxio_assert( quantity.is_valid(), "invalid quantity" );
    datxio_assert( quantity.amount > 0, "must issue positive quantity" );

    datxio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    datxio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, 0, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st.issuer );

    if( to != st.issuer ) {
       SEND_INLINE_ACTION( *this, transfer, {st.issuer,N(active)}, {st.issuer, to, quantity, memo} );
    }
}

void token::transfer( account_name from,
                      account_name to,
                      asset        quantity,
                      string       memo )
{
    datxio_assert( from != to, "cannot transfer to self" );
    require_auth( from );
    datxio_assert( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.name();
    stats statstable( _self, sym );
    const auto& st = statstable.get( sym );

    require_recipient( from );
    require_recipient( to );

    datxio_assert( quantity.is_valid(), "invalid quantity" );
    datxio_assert( quantity.amount > 0, "must transfer positive quantity" );
    datxio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    datxio_assert( memo.size() <= 256, "memo has more than 256 bytes" );


    sub_balance( from, quantity );
    add_balance( to, quantity, from );
}

void token::extract( account_name from,
                      account_name to,
                      asset        quantity,
                      string       memo )
{
    datxio_assert( from != to, "cannot transfer to self" );
    require_auth( from );
    datxio_assert( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.name();
    stats statstable( _self, sym );
    const auto& st = statstable.get( sym );

    require_recipient( from );
    require_recipient( to );

    datxio_assert( quantity.is_valid(), "invalid quantity" );
    datxio_assert( quantity.amount > 0, "must transfer positive quantity" );
    datxio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    datxio_assert( memo.size() <= 256, "memo has more than 256 bytes" );


    sub_balance( from, quantity );
    add_balance( to, quantity, from );

    checksum256 h;
    auto size = transaction_size();
    char buf[size];
    uint32_t read = read_transaction( buf, size );
    datxio_assert( size == read, "read_transaction failed");
    sha256(buf, read, &h);


     transrecords trx_table(_self,_self);

        trx_table.emplace(_self, [&](auto &t) {
            t.id= trx_table.available_primary_key();
            t.trxid=h;
            t.category=quantity.symbol;
            t.account=from;
            t.memo=memo;
        });    


}


void token::sub_balance( account_name owner, asset value ) {
   accounts from_acnts( _self, owner );

   const auto& from = from_acnts.get( value.symbol.name(), "no balance object found" );
   datxio_assert( from.balance.amount >= value.amount, "overdrawn balance" );


   if( from.balance.amount == value.amount ) {
      from_acnts.erase( from );
   } else {
      from_acnts.modify( from, owner, [&]( auto& a ) {
          a.balance -= value;
      });
   }
}

void token::add_balance( account_name owner, asset value, account_name ram_payer )
{
   accounts to_acnts( _self, owner );
   auto to = to_acnts.find( value.symbol.name() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, 0, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

} /// namespace datxio

DATXIO_ABI( datxio::token, (create)(issue)(transfer)(extract))
