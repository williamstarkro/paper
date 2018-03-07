#include <gtest/gtest.h>
#include <Paper/core/core.hpp>
#include <cryptopp/filters.h>
#include <cryptopp/randpool.h>

TEST (ledger, store_error)
{
    leveldb::Status init;
    Paper::block_store store (init, boost::filesystem::path {});
    ASSERT_FALSE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_TRUE (init1);
}

TEST (ledger, empty)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::address address;
    auto balance (ledger.account_balance (address));
    ASSERT_TRUE (balance.is_zero ());
}

TEST (ledger, genesis_balance)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    auto balance (ledger.account_balance (Paper::genesis_address));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), balance);
    Paper::frontier frontier;
    ASSERT_FALSE (store.latest_get (Paper::genesis_address, frontier));
    ASSERT_GE (store.now (), frontier.time);
    ASSERT_LT (store.now () - frontier.time, 10);
}

TEST (ledger, checksum_persistence)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    Paper::uint256_union checksum1;
    Paper::uint256_union max;
    max.qwords [0] = 0;
    max.qwords [0] = ~max.qwords [0];
    max.qwords [1] = 0;
    max.qwords [1] = ~max.qwords [1];
    max.qwords [2] = 0;
    max.qwords [2] = ~max.qwords [2];
    max.qwords [3] = 0;
    max.qwords [3] = ~max.qwords [3];
    {
        bool init1;
        Paper::ledger ledger (init1, init, store);
        ASSERT_FALSE (init1);
        Paper::genesis genesis;
        genesis.initialize (store);
        checksum1 = ledger.checksum (0, max);
    }
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    ASSERT_EQ (checksum1, ledger.checksum (0, max));
}

TEST (system, system_genesis)
{
    Paper::system system (24000, 2);
    for (auto & i: system.clients)
    {
        ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), i->ledger.account_balance (Paper::genesis_address));
    }
}

TEST (ledger, process_send)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::frontier frontier1;
    ASSERT_FALSE (store.latest_get (Paper::test_genesis_key.pub, frontier1));
    Paper::send_block send;
    Paper::keypair key2;
    send.hashables.balance = 50;
    send.hashables.previous = frontier1.hash;
    send.hashables.destination = key2.pub;
    Paper::block_hash hash1 (send.hash ());
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, hash1, send.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (send));
    ASSERT_EQ (50, ledger.account_balance (Paper::test_genesis_key.pub));
    Paper::frontier frontier2;
    ASSERT_FALSE (store.latest_get (Paper::test_genesis_key.pub, frontier2));
    auto latest6 (store.block_get (frontier2.hash));
    ASSERT_NE (nullptr, latest6);
    auto latest7 (dynamic_cast <Paper::send_block *> (latest6.get ()));
    ASSERT_NE (nullptr, latest7);
    ASSERT_EQ (send, *latest7);
    Paper::open_block open;
    open.hashables.source = hash1;
    open.hashables.representative = key2.pub;
    Paper::block_hash hash2 (open.hash ());
    Paper::sign_message (key2.prv, key2.pub, hash2, open.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (open));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 50, ledger.account_balance (key2.pub));
    ASSERT_EQ (50, ledger.weight (Paper::test_genesis_key.pub));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 50, ledger.weight (key2.pub));
    Paper::frontier frontier3;
    ASSERT_FALSE (store.latest_get (Paper::test_genesis_key.pub, frontier3));
    auto latest2 (store.block_get (frontier3.hash));
    ASSERT_NE (nullptr, latest2);
    auto latest3 (dynamic_cast <Paper::send_block *> (latest2.get ()));
    ASSERT_NE (nullptr, latest3);
    ASSERT_EQ (send, *latest3);
    Paper::frontier frontier4;
    ASSERT_FALSE (store.latest_get (key2.pub, frontier4));
    auto latest4 (store.block_get (frontier4.hash));
    ASSERT_NE (nullptr, latest4);
    auto latest5 (dynamic_cast <Paper::open_block *> (latest4.get ()));
    ASSERT_NE (nullptr, latest5);
    ASSERT_EQ (open, *latest5);
	ledger.rollback (hash2);
	Paper::frontier frontier5;
	ASSERT_TRUE (ledger.store.latest_get (key2.pub, frontier5));
    Paper::address sender1;
    Paper::amount amount1;
    Paper::address destination1;
	ASSERT_FALSE (ledger.store.pending_get (hash1, sender1, amount1, destination1));
    ASSERT_EQ (Paper::test_genesis_key.pub, sender1);
    ASSERT_EQ (key2.pub, destination1);
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 50, amount1.number ());
	ASSERT_EQ (0, ledger.account_balance (key2.pub));
	ASSERT_EQ (50, ledger.account_balance (Paper::test_genesis_key.pub));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), ledger.weight (Paper::test_genesis_key.pub));
    ASSERT_EQ (0, ledger.weight (key2.pub));
    Paper::frontier frontier6;
	ASSERT_FALSE (ledger.store.latest_get (Paper::test_genesis_key.pub, frontier6));
	ASSERT_EQ (hash1, frontier6.hash);
	ledger.rollback (frontier6.hash);
    Paper::frontier frontier7;
	ASSERT_FALSE (ledger.store.latest_get (Paper::test_genesis_key.pub, frontier7));
	ASSERT_EQ (frontier1.hash, frontier7.hash);
    Paper::address sender2;
    Paper::amount amount2;
    Paper::address destination2;
	ASSERT_TRUE (ledger.store.pending_get (hash1, sender2, amount2, destination2));
	ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), ledger.account_balance (Paper::test_genesis_key.pub));
}

TEST (ledger, process_receive)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::frontier frontier1;
    ASSERT_FALSE (store.latest_get (Paper::test_genesis_key.pub, frontier1));
    Paper::send_block send;
    Paper::keypair key2;
    send.hashables.balance = 50;
    send.hashables.previous = frontier1.hash;
    send.hashables.destination = key2.pub;
    Paper::block_hash hash1 (send.hash ());
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, hash1, send.signature);
	ASSERT_EQ (Paper::process_result::progress, ledger.process (send));
    Paper::keypair key3;
    Paper::open_block open;
    open.hashables.source = hash1;
    open.hashables.representative = key3.pub;
    Paper::block_hash hash2 (open.hash ());
    Paper::sign_message (key2.prv, key2.pub, hash2, open.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (open));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 50, ledger.weight (key3.pub));
	Paper::send_block send2;
	send2.hashables.balance = 25;
	send2.hashables.previous = hash1;
	send2.hashables.destination = key2.pub;
    Paper::block_hash hash3 (send2.hash ());
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, hash3, send2.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (send2));
	Paper::receive_block receive;
	receive.hashables.previous = hash2;
	receive.hashables.source = hash3;
	auto hash4 (receive.hash ());
	Paper::sign_message (key2.prv, key2.pub, hash4, receive.signature);
	ASSERT_EQ (Paper::process_result::progress, ledger.process (receive));
	ASSERT_EQ (hash4, ledger.latest (key2.pub));
	ASSERT_EQ (25, ledger.account_balance (Paper::test_genesis_key.pub));
	ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 25, ledger.account_balance (key2.pub));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 25, ledger.weight (key3.pub));
	ledger.rollback (hash4);
	ASSERT_EQ (25, ledger.account_balance (Paper::test_genesis_key.pub));
	ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 50, ledger.account_balance (key2.pub));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 50, ledger.weight (key3.pub));
	ASSERT_EQ (hash2, ledger.latest (key2.pub));
    Paper::address sender1;
    Paper::amount amount1;
    Paper::address destination1;
	ASSERT_FALSE (ledger.store.pending_get (hash3, sender1, amount1, destination1));
    ASSERT_EQ (Paper::test_genesis_key.pub, sender1);
    ASSERT_EQ (25, amount1.number ());
}

TEST (ledger, rollback_receiver)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::frontier frontier1;
    ASSERT_FALSE (store.latest_get (Paper::test_genesis_key.pub, frontier1));
    Paper::send_block send;
    Paper::keypair key2;
    send.hashables.balance = 50;
    send.hashables.previous = frontier1.hash;
    send.hashables.destination = key2.pub;
    Paper::block_hash hash1 (send.hash ());
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, hash1, send.signature);
	ASSERT_EQ (Paper::process_result::progress, ledger.process (send));
    Paper::keypair key3;
    Paper::open_block open;
    open.hashables.source = hash1;
    open.hashables.representative = key3.pub;
    Paper::block_hash hash2 (open.hash ());
    Paper::sign_message (key2.prv, key2.pub, hash2, open.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (open));
	ASSERT_EQ (hash2, ledger.latest (key2.pub));
	ASSERT_EQ (50, ledger.account_balance (Paper::test_genesis_key.pub));
	ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 50, ledger.account_balance (key2.pub));
    ASSERT_EQ (50, ledger.weight (Paper::test_genesis_key.pub));
    ASSERT_EQ (0, ledger.weight (key2.pub));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 50, ledger.weight (key3.pub));
	ledger.rollback (hash1);
	ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), ledger.account_balance (Paper::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.account_balance (key2.pub));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), ledger.weight (Paper::test_genesis_key.pub));
    ASSERT_EQ (0, ledger.weight (key2.pub));
    ASSERT_EQ (0, ledger.weight (key3.pub));
	Paper::frontier frontier2;
	ASSERT_TRUE (ledger.store.latest_get (key2.pub, frontier2));
    Paper::address sender1;
    Paper::amount amount1;
    Paper::address destination1;
	ASSERT_TRUE (ledger.store.pending_get (frontier2.hash, sender1, amount1, destination1));
}

TEST (ledger, rollback_representation)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::keypair key5;
    Paper::change_block change1 (key5.pub, genesis.hash (), Paper::test_genesis_key.prv, Paper::test_genesis_key.pub);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (change1));
    Paper::keypair key3;
    Paper::change_block change2 (key3.pub, change1.hash (), Paper::test_genesis_key.prv, Paper::test_genesis_key.pub);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (change2));
    Paper::send_block send1;
    Paper::keypair key2;
    send1.hashables.balance = 50;
    send1.hashables.previous = change2.hash ();
    send1.hashables.destination = key2.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send1.hash (), send1.signature);
	ASSERT_EQ (Paper::process_result::progress, ledger.process (send1));
    Paper::keypair key4;
    Paper::open_block open;
    open.hashables.source = send1.hash ();
    open.hashables.representative = key4.pub;
    Paper::block_hash hash2 (open.hash ());
    Paper::sign_message (key2.prv, key2.pub, hash2, open.signature);
    Paper::sign_message(key2.prv, key2.pub, open.hash (), open.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (open));
    Paper::send_block send2;
    send2.hashables.balance = 1;
    send2.hashables.previous = send1.hash ();
    send2.hashables.destination = key2.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send2.hash (), send2.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (send2));
    Paper::receive_block receive1;
    receive1.hashables.previous = open.hash ();
    receive1.hashables.source = send2.hash ();
    Paper::sign_message (key2.prv, key2.pub, receive1.hash (), receive1.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (receive1));
    ASSERT_EQ (1, ledger.weight (key3.pub));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 1, ledger.weight (key4.pub));
    ledger.rollback (receive1.hash ());
    ASSERT_EQ (50, ledger.weight (key3.pub));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 50, ledger.weight (key4.pub));
    ledger.rollback (open.hash ());
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), ledger.weight (key3.pub));
    ASSERT_EQ (0, ledger.weight (key4.pub));
    ledger.rollback (change2.hash ());
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), ledger.weight (key5.pub));
    ASSERT_EQ (0, ledger.weight (key3.pub));
}

TEST (ledger, process_duplicate)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::frontier frontier1;
    ASSERT_FALSE (store.latest_get (Paper::test_genesis_key.pub, frontier1));
    Paper::send_block send;
    Paper::keypair key2;
    send.hashables.balance = 50;
    send.hashables.previous = frontier1.hash;
    send.hashables.destination = key2.pub;
    Paper::block_hash hash1 (send.hash ());
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, hash1, send.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (send));
    ASSERT_EQ (Paper::process_result::old, ledger.process (send));
    Paper::open_block open;
    open.hashables.representative.clear ();
    open.hashables.source = hash1;
    Paper::block_hash hash2 (open.hash ());
    Paper::sign_message(key2.prv, key2.pub, hash2, open.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (open));
    ASSERT_EQ (Paper::process_result::old, ledger.process (open));
}

TEST (ledger, representative_genesis)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    ASSERT_EQ (Paper::test_genesis_key.pub, ledger.representative (ledger.latest (Paper::test_genesis_key.pub)));
}

TEST (ledger, weight)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), ledger.weight (Paper::test_genesis_key.pub));
}

TEST (ledger, representative_change)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::keypair key2;
    Paper::genesis genesis;
    genesis.initialize (store);
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), ledger.weight (Paper::test_genesis_key.pub));
    ASSERT_EQ (0, ledger.weight (key2.pub));
    Paper::frontier frontier1;
    ASSERT_FALSE (store.latest_get (Paper::test_genesis_key.pub, frontier1));
    Paper::change_block block (key2.pub, frontier1.hash, Paper::test_genesis_key.prv, Paper::test_genesis_key.pub);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (block));
    ASSERT_EQ (0, ledger.weight (Paper::test_genesis_key.pub));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), ledger.weight (key2.pub));
	Paper::frontier frontier2;
	ASSERT_FALSE (store.latest_get (Paper::test_genesis_key.pub, frontier2));
	ASSERT_EQ (block.hash (), frontier2.hash);
	ledger.rollback (frontier2.hash);
	Paper::frontier frontier3;
	ASSERT_FALSE (store.latest_get (Paper::test_genesis_key.pub, frontier3));
	ASSERT_EQ (frontier1.hash, frontier3.hash);
	ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), ledger.weight (Paper::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (key2.pub));
}

TEST (ledger, send_fork)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::keypair key2;
    Paper::keypair key3;
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::frontier frontier1;
    ASSERT_FALSE (store.latest_get (Paper::test_genesis_key.pub, frontier1));
    Paper::send_block block;
    block.hashables.destination = key2.pub;
    block.hashables.previous = frontier1.hash;
    block.hashables.balance = 100;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block.hash (), block.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (block));
    Paper::send_block block2;
    block2.hashables.destination = key3.pub;
    block2.hashables.previous = frontier1.hash;
    block2.hashables.balance.clear ();
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block2.hash (), block2.signature);
    ASSERT_EQ (Paper::process_result::fork_previous, ledger.process (block2));
}

TEST (ledger, receive_fork)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::keypair key2;
    Paper::keypair key3;
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::frontier frontier1;
    ASSERT_FALSE (store.latest_get (Paper::test_genesis_key.pub, frontier1));
    Paper::send_block block;
    block.hashables.destination = key2.pub;
    block.hashables.previous = frontier1.hash;
    block.hashables.balance = 100;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block.hash (), block.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (block));
    Paper::open_block block2;
    block2.hashables.representative = key2.pub;
    block2.hashables.source = block.hash ();
    Paper::sign_message(key2.prv, key2.pub, block2.hash (), block2.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (block2));
    Paper::change_block block3 (key3.pub, block2.hash (), key2.prv, key2.pub);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (block3));
    Paper::send_block block4;
    block4.hashables.destination = key2.pub;
    block4.hashables.previous = block.hash ();
    block4.hashables.balance.clear ();
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block4.hash (), block4.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (block4));
    Paper::receive_block block5;
    block5.hashables.previous = block2.hash ();
    block5.hashables.source = block4.hash ();
    Paper::sign_message (key2.prv, key2.pub, block5.hash (), block5.signature);
    ASSERT_EQ (Paper::process_result::fork_previous, ledger.process (block5));
}

TEST (ledger, checksum_single)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    Paper::genesis genesis;
    genesis.initialize (store);
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    store.checksum_put (0, 0, genesis.hash ());
	ASSERT_EQ (genesis.hash (), ledger.checksum (0, std::numeric_limits <Paper::uint256_t>::max ()));
    Paper::change_block block1 (Paper::address (0), ledger.latest (Paper::test_genesis_key.pub), Paper::test_genesis_key.prv, Paper::test_genesis_key.pub);
    Paper::checksum check1 (ledger.checksum (0, std::numeric_limits <Paper::uint256_t>::max ()));
	ASSERT_EQ (genesis.hash (), check1);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (block1));
    Paper::checksum check2 (ledger.checksum (0, std::numeric_limits <Paper::uint256_t>::max ()));
    ASSERT_EQ (block1.hash (), check2);
}

TEST (ledger, checksum_two)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    Paper::genesis genesis;
    genesis.initialize (store);
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    store.checksum_put (0, 0, genesis.hash ());
	Paper::keypair key2;
    Paper::send_block block1;
    block1.hashables.previous = ledger.latest (Paper::test_genesis_key.pub);
	block1.hashables.destination = key2.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block1.hash (), block1.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (block1));
	Paper::checksum check1 (ledger.checksum (0, std::numeric_limits <Paper::uint256_t>::max ()));
	Paper::open_block block2;
	block2.hashables.source = block1.hash ();
	Paper::sign_message (key2.prv, key2.pub, block2.hash (), block2.signature);
	ASSERT_EQ (Paper::process_result::progress, ledger.process (block2));
	Paper::checksum check2 (ledger.checksum (0, std::numeric_limits <Paper::uint256_t>::max ()));
	ASSERT_EQ (check1, check2 ^ block2.hash ());
}

TEST (ledger, DISABLED_checksum_range)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::checksum check1 (ledger.checksum (0, std::numeric_limits <Paper::uint256_t>::max ()));
    ASSERT_TRUE (check1.is_zero ());
    Paper::block_hash hash1 (42);
    Paper::checksum check2 (ledger.checksum (0, 42));
    ASSERT_TRUE (check2.is_zero ());
    Paper::checksum check3 (ledger.checksum (42, std::numeric_limits <Paper::uint256_t>::max ()));
    ASSERT_EQ (hash1, check3);
}

TEST (system, generate_send_existing)
{
    Paper::system system (24000, 1);
    system.clients [0]->wallet.insert (Paper::test_genesis_key.prv);
    Paper::frontier frontier1;
    ASSERT_FALSE (system.clients [0]->store.latest_get (Paper::test_genesis_key.pub, frontier1));
    system.generate_send_existing (*system.clients [0]);
    Paper::frontier frontier2;
    ASSERT_FALSE (system.clients [0]->store.latest_get (Paper::test_genesis_key.pub, frontier2));
    ASSERT_NE (frontier1.hash, frontier2.hash);
    auto iterations1 (0);
    while (system.clients [0]->ledger.account_balance (Paper::test_genesis_key.pub) == std::numeric_limits <Paper::uint128_t>::max ())
    {
        system.service->poll_one ();
        system.processor.poll_one ();
        ++iterations1;
        ASSERT_LT (iterations1, 20);
    }
    auto iterations2 (0);
    while (system.clients [0]->ledger.account_balance (Paper::test_genesis_key.pub) != std::numeric_limits <Paper::uint128_t>::max ())
    {
        system.service->poll_one ();
        system.processor.poll_one ();
        ++iterations2;
        ASSERT_LT (iterations2, 20);
    }
}

TEST (system, generate_send_new)
{
    Paper::system system (24000, 1);
    system.clients [0]->wallet.insert (Paper::test_genesis_key.prv);
    auto iterator1 (system.clients [0]->store.latest_begin ());
    ++iterator1;
    ASSERT_EQ (system.clients [0]->store.latest_end (), iterator1);
    system.generate_send_new (*system.clients [0]);
    Paper::address new_address;
    auto iterator2 (system.clients [0]->wallet.begin ());
    if (iterator2->first != Paper::test_genesis_key.pub)
    {
        new_address = iterator2->first;
    }
    ++iterator2;
    ASSERT_NE (system.clients [0]->wallet.end (), iterator2);
    if (iterator2->first != Paper::test_genesis_key.pub)
    {
        new_address = iterator2->first;
    }
    ++iterator2;
    ASSERT_EQ (system.clients [0]->wallet.end (), iterator2);
    while (system.clients [0]->ledger.account_balance (new_address) == 0)
    {
        system.service->poll_one ();
        system.processor.poll_one ();
    }
}

TEST (system, generate_mass_activity)
{
    Paper::system system (24000, 1);
    system.clients [0]->wallet.insert (Paper::test_genesis_key.prv);
    size_t count (20);
    system.generate_mass_activity (count, *system.clients [0]);
    size_t accounts (0);
    for (auto i (system.clients [0]->store.latest_begin ()), n (system.clients [0]->store.latest_end ()); i != n; ++i)
    {
        ++accounts;
    }
    ASSERT_GT (accounts, count / 10);
}

TEST (system, DISABLED_generate_mass_activity_long)
{
    Paper::system system (24000, 1);
    system.clients [0]->wallet.insert (Paper::test_genesis_key.prv);
    size_t count (10000);
    system.generate_mass_activity (count, *system.clients [0]);
    system.clients [0]->log.dump_cerr ();
    size_t accounts (0);
    for (auto i (system.clients [0]->store.latest_begin ()), n (system.clients [0]->store.latest_end ()); i != n; ++i)
    {
        ++accounts;
    }
    ASSERT_GT (accounts, count / 10);
}

TEST (ledger, representation)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), store.representation_get (Paper::test_genesis_key.pub));
    Paper::keypair key2;
    Paper::send_block block1;
    block1.hashables.balance = std::numeric_limits <Paper::uint128_t>::max () - 100;
    block1.hashables.destination = key2.pub;
    block1.hashables.previous = genesis.hash ();
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block1.hash (), block1.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (block1));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), store.representation_get (Paper::test_genesis_key.pub));
    Paper::keypair key3;
    Paper::open_block block2;
    block2.hashables.representative = key3.pub;
    block2.hashables.source = block1.hash ();
    Paper::sign_message (key2.prv, key2.pub, block2.hash (), block2.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (block2));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 100, store.representation_get (Paper::test_genesis_key.pub));
    ASSERT_EQ (0, store.representation_get (key2.pub));
    ASSERT_EQ (100, store.representation_get (key3.pub));
    Paper::send_block block3;
    block3.hashables.balance = std::numeric_limits <Paper::uint128_t>::max () - 200;
    block3.hashables.destination = key2.pub;
    block3.hashables.previous = block1.hash ();
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block3.hash (), block3.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (block3));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 100, store.representation_get (Paper::test_genesis_key.pub));
    ASSERT_EQ (0, store.representation_get (key2.pub));
    ASSERT_EQ (100, store.representation_get (key3.pub));
    Paper::receive_block block4;
    block4.hashables.previous = block2.hash ();
    block4.hashables.source = block3.hash ();
    Paper::sign_message (key2.prv, key2.pub, block4.hash (), block4.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (block4));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 200, store.representation_get (Paper::test_genesis_key.pub));
    ASSERT_EQ (0, store.representation_get (key2.pub));
    ASSERT_EQ (200, store.representation_get (key3.pub));
    Paper::keypair key4;
    Paper::change_block block5 (key4.pub, block4.hash (), key2.prv, key2.pub);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (block5));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 200, store.representation_get (Paper::test_genesis_key.pub));
    ASSERT_EQ (0, store.representation_get (key2.pub));
    ASSERT_EQ (0, store.representation_get (key3.pub));
    ASSERT_EQ (200, store.representation_get (key4.pub));
    Paper::keypair key5;
    Paper::send_block block6;
    block6.hashables.balance = 100;
    block6.hashables.destination = key5.pub;
    block6.hashables.previous = block5.hash ();
    Paper::sign_message (key2.prv, key2.pub, block6.hash (), block6.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (block6));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 200, store.representation_get (Paper::test_genesis_key.pub));
    ASSERT_EQ (0, store.representation_get (key2.pub));
    ASSERT_EQ (0, store.representation_get (key3.pub));
    ASSERT_EQ (200, store.representation_get (key4.pub));
    ASSERT_EQ (0, store.representation_get (key5.pub));
    Paper::keypair key6;
    Paper::open_block block7;
    block7.hashables.representative = key6.pub;
    block7.hashables.source = block6.hash ();
    Paper::sign_message (key5.prv, key5.pub, block7.hash (), block7.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (block7));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 200, store.representation_get (Paper::test_genesis_key.pub));
    ASSERT_EQ (0, store.representation_get (key2.pub));
    ASSERT_EQ (0, store.representation_get (key3.pub));
    ASSERT_EQ (100, store.representation_get (key4.pub));
    ASSERT_EQ (0, store.representation_get (key5.pub));
    ASSERT_EQ (100, store.representation_get (key6.pub));
    Paper::send_block block8;
    block8.hashables.balance.clear ();
    block8.hashables.destination = key5.pub;
    block8.hashables.previous = block6.hash ();
    Paper::sign_message (key2.prv, key2.pub, block8.hash (), block8.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (block8));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 200, store.representation_get (Paper::test_genesis_key.pub));
    ASSERT_EQ (0, store.representation_get (key2.pub));
    ASSERT_EQ (0, store.representation_get (key3.pub));
    ASSERT_EQ (100, store.representation_get (key4.pub));
    ASSERT_EQ (0, store.representation_get (key5.pub));
    ASSERT_EQ (100, store.representation_get (key6.pub));
    Paper::receive_block block9;
    block9.hashables.previous = block7.hash ();
    block9.hashables.source = block8.hash ();
    Paper::sign_message (key5.prv, key5.pub, block9.hash (), block9.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (block9));
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max () - 200, store.representation_get (Paper::test_genesis_key.pub));
    ASSERT_EQ (0, store.representation_get (key2.pub));
    ASSERT_EQ (0, store.representation_get (key3.pub));
    ASSERT_EQ (0, store.representation_get (key4.pub));
    ASSERT_EQ (0, store.representation_get (key5.pub));
    ASSERT_EQ (200, store.representation_get (key6.pub));
}

TEST (ledger, double_open)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::keypair key2;
    Paper::send_block send1;
    send1.hashables.balance = 1;
    send1.hashables.destination = key2.pub;
    send1.hashables.previous = genesis.hash ();
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send1.hash (), send1.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (send1));
    Paper::open_block open1;
    open1.hashables.representative = key2.pub;
    open1.hashables.source = send1.hash ();
    Paper::sign_message (key2.prv, key2.pub, open1.hash (), open1.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (open1));
    Paper::open_block open2;
    open2.hashables.representative = Paper::test_genesis_key.pub;
    open2.hashables.source = send1.hash ();
    Paper::sign_message (key2.prv, key2.pub, open2.hash (), open2.signature);
    ASSERT_EQ (Paper::process_result::fork_source, ledger.process (open2));
}

TEST (ledegr, double_receive)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::keypair key2;
    Paper::send_block send1;
    send1.hashables.balance = 1;
    send1.hashables.destination = key2.pub;
    send1.hashables.previous = genesis.hash ();
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send1.hash (), send1.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (send1));
    Paper::open_block open1;
    open1.hashables.representative = key2.pub;
    open1.hashables.source = send1.hash ();
    Paper::sign_message (key2.prv, key2.pub, open1.hash (), open1.signature);
    ASSERT_EQ (Paper::process_result::progress, ledger.process (open1));
    Paper::receive_block receive1;
    receive1.hashables.previous = open1.hash ();
    receive1.hashables.source = send1.hash ();
    Paper::sign_message (key2.prv, key2.pub, receive1.hash (), receive1.signature);
    ASSERT_EQ (Paper::process_result::overreceive, ledger.process (receive1));
}

TEST (votes, add_unsigned)
{
    Paper::system system (24000, 1);
    auto & client1 (*system.clients [0]);
    Paper::genesis genesis;
    Paper::send_block send1;
    Paper::keypair key1;
    send1.hashables.previous = genesis.hash ();
    send1.hashables.balance.clear ();
    send1.hashables.destination = key1.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send1.hash (), send1.signature);
    ASSERT_EQ (Paper::process_result::progress, client1.ledger.process (send1));
    client1.conflicts.start (send1, client1.create_work (send1), false);
    auto votes1 (client1.conflicts.roots.find (client1.store.root (send1))->second);
    ASSERT_NE (nullptr, votes1);
    ASSERT_EQ (1, votes1->votes.rep_votes.size ());
    Paper::vote vote1;
    vote1.sequence = 1;
    vote1.block = send1.clone ();
    vote1.address = key1.pub;
    votes1->vote (vote1);
    ASSERT_EQ (1, votes1->votes.rep_votes.size ());
}

TEST (votes, add_one)
{
    Paper::system system (24000, 1);
    auto & client1 (*system.clients [0]);
    Paper::genesis genesis;
    Paper::send_block send1;
    Paper::keypair key1;
    send1.hashables.previous = genesis.hash ();
    send1.hashables.balance.clear ();
    send1.hashables.destination = key1.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send1.hash (), send1.signature);
    ASSERT_EQ (Paper::process_result::progress, client1.ledger.process (send1));
    client1.conflicts.start (send1, client1.create_work (send1), false);
    auto votes1 (client1.conflicts.roots.find (client1.store.root (send1))->second);
    ASSERT_EQ (1, votes1->votes.rep_votes.size ());
    Paper::vote vote1;
    vote1.sequence = 1;
    vote1.block = send1.clone ();
    vote1.address = Paper::test_genesis_key.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, vote1.hash (), vote1.signature);
    votes1->vote (vote1);
    ASSERT_EQ (2, votes1->votes.rep_votes.size ());
    auto existing1 (votes1->votes.rep_votes.find (Paper::test_genesis_key.pub));
    ASSERT_NE (votes1->votes.rep_votes.end (), existing1);
    ASSERT_EQ (send1, *existing1->second.second);
    auto winner (votes1->votes.winner ());
    ASSERT_EQ (send1, *winner.first);
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), winner.second);
}

TEST (votes, add_two)
{
    Paper::system system (24000, 1);
    auto & client1 (*system.clients [0]);
    Paper::genesis genesis;
    Paper::send_block send1;
    Paper::keypair key1;
    send1.hashables.previous = genesis.hash ();
    send1.hashables.balance.clear ();
    send1.hashables.destination = key1.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send1.hash (), send1.signature);
    ASSERT_EQ (Paper::process_result::progress, client1.ledger.process (send1));
    client1.conflicts.start (send1, client1.create_work (send1), false);
    auto votes1 (client1.conflicts.roots.find (client1.store.root (send1))->second);
    Paper::vote vote1;
    vote1.sequence = 1;
    vote1.block = send1.clone ();
    vote1.address = Paper::test_genesis_key.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, vote1.hash (), vote1.signature);
    votes1->vote (vote1);
    Paper::send_block send2;
    Paper::keypair key2;
    send2.hashables.previous = genesis.hash ();
    send2.hashables.balance.clear ();
    send2.hashables.destination = key2.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send2.hash (), send2.signature);
    Paper::vote vote2;
    vote2.address = key2.pub;
    vote2.sequence = 1;
    vote2.block = send2.clone ();
    Paper::sign_message (key2.prv, key2.pub, vote2.hash (), vote2.signature);
    votes1->vote (vote2);
    ASSERT_EQ (3, votes1->votes.rep_votes.size ());
    ASSERT_NE (votes1->votes.rep_votes.end (), votes1->votes.rep_votes.find (Paper::test_genesis_key.pub));
    ASSERT_EQ (send1, *votes1->votes.rep_votes [Paper::test_genesis_key.pub].second);
    ASSERT_NE (votes1->votes.rep_votes.end (), votes1->votes.rep_votes.find (key2.pub));
    ASSERT_EQ (send2, *votes1->votes.rep_votes [key2.pub].second);
    auto winner (votes1->votes.winner ());
    ASSERT_EQ (send1, *winner.first);
}

TEST (votes, add_existing)
{
    Paper::system system (24000, 1);
    auto & client1 (*system.clients [0]);
    Paper::genesis genesis;
    Paper::send_block send1;
    Paper::keypair key1;
    send1.hashables.previous = genesis.hash ();
    send1.hashables.balance.clear ();
    send1.hashables.destination = key1.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send1.hash (), send1.signature);
    ASSERT_EQ (Paper::process_result::progress, client1.ledger.process (send1));
    client1.conflicts.start (send1, client1.create_work (send1), false);
    auto votes1 (client1.conflicts.roots.find (client1.store.root (send1))->second);
    Paper::vote vote1;
    vote1.sequence = 1;
    vote1.block = send1.clone ();
    vote1.address = Paper::test_genesis_key.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, vote1.hash (), vote1.signature);
    votes1->vote (vote1);
    Paper::send_block send2;
    Paper::keypair key2;
    send2.hashables.previous = genesis.hash ();
    send2.hashables.balance.clear ();
    send2.hashables.destination = key2.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send2.hash (), send2.signature);
    Paper::vote vote2;
    vote2.address = Paper::test_genesis_key.pub;
    vote2.sequence = 2;
    vote2.block = send2.clone ();
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, vote2.hash (), vote2.signature);
    votes1->vote (vote2);
    ASSERT_EQ (2, votes1->votes.rep_votes.size ());
    ASSERT_NE (votes1->votes.rep_votes.end (), votes1->votes.rep_votes.find (Paper::test_genesis_key.pub));
    ASSERT_EQ (send2, *votes1->votes.rep_votes [Paper::test_genesis_key.pub].second);
    auto winner (votes1->votes.winner ());
    ASSERT_EQ (send2, *winner.first);
}

TEST (votes, add_old)
{
    Paper::system system (24000, 1);
    auto & client1 (*system.clients [0]);
	Paper::genesis genesis;
    Paper::send_block send1;
    Paper::keypair key1;
    send1.hashables.previous = genesis.hash ();
    send1.hashables.balance.clear ();
    send1.hashables.destination = key1.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send1.hash (), send1.signature);
    ASSERT_EQ (Paper::process_result::progress, client1.ledger.process (send1));
    client1.conflicts.start (send1, client1.create_work (send1), false);
    auto votes1 (client1.conflicts.roots.find (client1.store.root (send1))->second);
    Paper::vote vote1;
    vote1.sequence = 2;
    vote1.block = send1.clone ();
    vote1.address = Paper::test_genesis_key.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, vote1.hash (), vote1.signature);
    votes1->vote (vote1);
    Paper::send_block send2;
    Paper::keypair key2;
    send2.hashables.previous = genesis.hash ();
    send2.hashables.balance.clear ();
    send2.hashables.destination = key2.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send2.hash (), send2.signature);
    Paper::vote vote2;
    vote2.address = Paper::test_genesis_key.pub;
    vote2.sequence = 1;
    vote2.block = send2.clone ();
    Paper::sign_message (key2.prv, key2.pub, vote2.hash (), vote2.signature);
    votes1->vote (vote2);
    ASSERT_EQ (2, votes1->votes.rep_votes.size ());
    ASSERT_NE (votes1->votes.rep_votes.end (), votes1->votes.rep_votes.find (Paper::test_genesis_key.pub));
    ASSERT_EQ (send1, *votes1->votes.rep_votes [Paper::test_genesis_key.pub].second);
    auto winner (votes1->votes.winner ());
    ASSERT_EQ (send1, *winner.first);
}

TEST (conflicts, start_stop)
{
    Paper::system system (24000, 1);
    auto & client1 (*system.clients [0]);
	Paper::genesis genesis;
    Paper::send_block send1;
    Paper::keypair key1;
    send1.hashables.previous = genesis.hash ();
    send1.hashables.balance.clear ();
    send1.hashables.destination = key1.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send1.hash (), send1.signature);
    ASSERT_EQ (Paper::process_result::progress, client1.ledger.process (send1));
    ASSERT_EQ (0, client1.conflicts.roots.size ());
    client1.conflicts.start (send1, client1.create_work (send1), false);
    ASSERT_EQ (1, client1.conflicts.roots.size ());
    auto root1 (client1.store.root (send1));
    auto existing1 (client1.conflicts.roots.find (root1));
    ASSERT_NE (client1.conflicts.roots.end (), existing1);
    auto votes1 (existing1->second);
    ASSERT_NE (nullptr, votes1);
    ASSERT_EQ (1, votes1->votes.rep_votes.size ());
    client1.conflicts.stop (root1);
    ASSERT_EQ (0, client1.conflicts.roots.size ());
}

TEST (conflicts, add_existing)
{
    Paper::system system (24000, 1);
    auto & client1 (*system.clients [0]);
	Paper::genesis genesis;
    Paper::send_block send1;
    Paper::keypair key1;
    send1.hashables.previous = genesis.hash ();
    send1.hashables.balance.clear ();
    send1.hashables.destination = key1.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send1.hash (), send1.signature);
    ASSERT_EQ (Paper::process_result::progress, client1.ledger.process (send1));
    client1.conflicts.start (send1, client1.create_work (send1), false);
    Paper::send_block send2;
    Paper::keypair key2;
    send2.hashables.previous = genesis.hash ();
    send2.hashables.balance.clear ();
    send2.hashables.destination = key2.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send2.hash (), send2.signature);
    client1.conflicts.start (send2, client1.create_work (send2), false);
    ASSERT_EQ (1, client1.conflicts.roots.size ());
    Paper::vote vote1;
    vote1.address = key2.pub;
    vote1.sequence = 0;
    vote1.block = send2.clone ();
    Paper::sign_message (key2.prv, key2.pub, vote1.hash (), vote1.signature);
    client1.conflicts.update (vote1);
    ASSERT_EQ (1, client1.conflicts.roots.size ());
    auto votes1 (client1.conflicts.roots [client1.store.root (send2)]);
    ASSERT_NE (nullptr, votes1);
    ASSERT_EQ (2, votes1->votes.rep_votes.size ());
    ASSERT_NE (votes1->votes.rep_votes.end (), votes1->votes.rep_votes.find (key2.pub));
}

TEST (conflicts, add_two)
{
    Paper::system system (24000, 1);
    auto & client1 (*system.clients [0]);
	Paper::genesis genesis;
    Paper::send_block send1;
    Paper::keypair key1;
    send1.hashables.previous = genesis.hash ();
    send1.hashables.balance.clear ();
    send1.hashables.destination = key1.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send1.hash (), send1.signature);
    ASSERT_EQ (Paper::process_result::progress, client1.ledger.process (send1));
    client1.conflicts.start (send1, client1.create_work (send1), false);
    Paper::send_block send2;
    Paper::keypair key2;
    send2.hashables.previous = send1.hash ();
    send2.hashables.balance.clear ();
    send2.hashables.destination = key2.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send2.hash (), send2.signature);
    ASSERT_EQ (Paper::process_result::progress, client1.ledger.process (send2));
    client1.conflicts.start (send2, client1.create_work (send2), false);
    ASSERT_EQ (2, client1.conflicts.roots.size ());
}

TEST (ledger, successor)
{
    Paper::system system (24000, 1);
	Paper::keypair key1;
	Paper::genesis genesis;
	Paper::send_block send1;
	send1.hashables.previous = genesis.hash ();
	send1.hashables.balance.clear ();
	send1.hashables.destination = key1.pub;
	Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send1.hash (), send1.signature);
	ASSERT_EQ (Paper::process_result::progress, system.clients [0]->ledger.process (send1));
	ASSERT_EQ (send1, *system.clients [0]->ledger.successor (genesis.hash ()));
}

TEST (fork, publish)
{
    std::weak_ptr <Paper::client> client0;
    {
        Paper::system system (24000, 1);
        client0 = system.clients [0];
        auto & client1 (*system.clients [0]);
        client1.wallet.insert (Paper::test_genesis_key.prv);
        Paper::keypair key1;
		Paper::genesis genesis;
        std::unique_ptr <Paper::send_block> send1 (new Paper::send_block);
        send1->hashables.previous = genesis.hash ();
        send1->hashables.balance.clear ();
        send1->hashables.destination = key1.pub;
        Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send1->hash (), send1->signature);
        Paper::publish publish1;
        publish1.block = std::move (send1);
        Paper::keypair key2;
        std::unique_ptr <Paper::send_block> send2 (new Paper::send_block);
        send2->hashables.previous = genesis.hash ();
        send2->hashables.balance.clear ();
        send2->hashables.destination = key2.pub;
        Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send2->hash (), send2->signature);
        Paper::publish publish2;
        publish2.block = std::move (send2);
        client1.processor.process_message (publish1, Paper::endpoint {});
        ASSERT_EQ (0, client1.conflicts.roots.size ());
        client1.processor.process_message (publish2, Paper::endpoint {});
        ASSERT_EQ (1, client1.conflicts.roots.size ());
        auto conflict1 (client1.conflicts.roots.find (client1.store.root (*publish1.block)));
        ASSERT_NE (client1.conflicts.roots.end (), conflict1);
        auto votes1 (conflict1->second);
        ASSERT_NE (nullptr, votes1);
        ASSERT_EQ (1, votes1->votes.rep_votes.size ());
        while (votes1->votes.rep_votes.size () == 1)
        {
            system.service->poll_one ();
            system.processor.poll_one ();
        }
        ASSERT_EQ (2, votes1->votes.rep_votes.size ());
        auto existing1 (votes1->votes.rep_votes.find (Paper::test_genesis_key.pub));
        ASSERT_NE (votes1->votes.rep_votes.end (), existing1);
        ASSERT_EQ (*publish1.block, *existing1->second.second);
        auto winner (votes1->votes.winner ());
        ASSERT_EQ (*publish1.block, *winner.first);
        ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), winner.second);
    }
    ASSERT_TRUE (client0.expired ());
}

TEST (ledger, fork_keep)
{
    Paper::system system (24000, 2);
    auto & client1 (*system.clients [0]);
    auto & client2 (*system.clients [1]);
	ASSERT_EQ (1, client1.peers.size ());
	client1.wallet.insert (Paper::test_genesis_key.prv);
    Paper::keypair key1;
	Paper::genesis genesis;
    std::unique_ptr <Paper::send_block> send1 (new Paper::send_block);
    send1->hashables.previous = genesis.hash ();
    send1->hashables.balance.clear ();
    send1->hashables.destination = key1.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send1->hash (), send1->signature);
    Paper::publish publish1;
    publish1.block = std::move (send1);
    publish1.work = client1.create_work (*publish1.block);
    Paper::keypair key2;
    std::unique_ptr <Paper::send_block> send2 (new Paper::send_block);
    send2->hashables.previous = genesis.hash ();
    send2->hashables.balance.clear ();
    send2->hashables.destination = key2.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send2->hash (), send2->signature);
    Paper::publish publish2;
    publish2.block = std::move (send2);
    publish2.work = client1.create_work (*publish2.block);
    client1.processor.process_message (publish1, Paper::endpoint {});
	client2.processor.process_message (publish1, Paper::endpoint {});
    ASSERT_EQ (0, client1.conflicts.roots.size ());
    ASSERT_EQ (0, client2.conflicts.roots.size ());
    client1.processor.process_message (publish2, Paper::endpoint {});
	client2.processor.process_message (publish2, Paper::endpoint {});
    ASSERT_EQ (1, client1.conflicts.roots.size ());
    ASSERT_EQ (1, client2.conflicts.roots.size ());
    auto conflict (client2.conflicts.roots.find (genesis.hash ()));
    ASSERT_NE (client2.conflicts.roots.end (), conflict);
    auto votes1 (conflict->second);
    ASSERT_NE (nullptr, votes1);
    ASSERT_EQ (1, votes1->votes.rep_votes.size ());
	ASSERT_TRUE (system.clients [0]->store.block_exists (publish1.block->hash ()));
	ASSERT_TRUE (system.clients [1]->store.block_exists (publish1.block->hash ()));
    auto iterations (0);
    while (votes1->votes.rep_votes.size () == 1)
	{
		system.service->poll_one ();
		system.processor.poll_one ();
        ++iterations;
        ASSERT_LT (iterations, 200);
	}
    auto winner (votes1->votes.winner ());
    ASSERT_EQ (*publish1.block, *winner.first);
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), winner.second);
	ASSERT_TRUE (system.clients [0]->store.block_exists (publish1.block->hash ()));
	ASSERT_TRUE (system.clients [1]->store.block_exists (publish1.block->hash ()));
}

TEST (ledger, fork_flip)
{
    Paper::system system (24000, 2);
    auto & client1 (*system.clients [0]);
    auto & client2 (*system.clients [1]);
    ASSERT_EQ (1, client1.peers.size ());
    client1.wallet.insert (Paper::test_genesis_key.prv);
    Paper::keypair key1;
	Paper::genesis genesis;
    std::unique_ptr <Paper::send_block> send1 (new Paper::send_block);
    send1->hashables.previous = genesis.hash ();
    send1->hashables.balance.clear ();
    send1->hashables.destination = key1.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send1->hash (), send1->signature);
    Paper::publish publish1;
    publish1.block = std::move (send1);
    publish1.work = client1.create_work (*publish1.block);
    Paper::keypair key2;
    std::unique_ptr <Paper::send_block> send2 (new Paper::send_block);
    send2->hashables.previous = genesis.hash ();
    send2->hashables.balance.clear ();
    send2->hashables.destination = key2.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send2->hash (), send2->signature);
    Paper::publish publish2;
    publish2.block = std::move (send2);
    publish2.work = client1.create_work (*publish2.block);
    client1.processor.process_message (publish1, Paper::endpoint {});
    client2.processor.process_message (publish2, Paper::endpoint {});
    ASSERT_EQ (0, client1.conflicts.roots.size ());
    ASSERT_EQ (0, client2.conflicts.roots.size ());
    client1.processor.process_message (publish2, Paper::endpoint {});
    client2.processor.process_message (publish1, Paper::endpoint {});
    ASSERT_EQ (1, client1.conflicts.roots.size ());
    ASSERT_EQ (1, client2.conflicts.roots.size ());
    auto conflict (client2.conflicts.roots.find (genesis.hash ()));
    ASSERT_NE (client2.conflicts.roots.end (), conflict);
    auto votes1 (conflict->second);
    ASSERT_NE (nullptr, votes1);
    ASSERT_EQ (1, votes1->votes.rep_votes.size ());
    ASSERT_TRUE (client1.store.block_exists (publish1.block->hash ()));
    ASSERT_TRUE (client2.store.block_exists (publish2.block->hash ()));
    auto iterations (0);
    while (votes1->votes.rep_votes.size () == 1)
    {
        system.service->poll_one ();
        system.processor.poll_one ();
        ++iterations;
        ASSERT_LT (iterations, 200);
    }
    auto winner (votes1->votes.winner ());
    ASSERT_EQ (*publish1.block, *winner.first);
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), winner.second);
    ASSERT_TRUE (client1.store.block_exists (publish1.block->hash ()));
    ASSERT_TRUE (client2.store.block_exists (publish1.block->hash ()));
    ASSERT_FALSE (client2.store.block_exists (publish2.block->hash ()));
}

TEST (ledger, fork_multi_flip)
{
    Paper::system system (24000, 2);
    auto & client1 (*system.clients [0]);
    auto & client2 (*system.clients [1]);
	ASSERT_EQ (1, client1.peers.size ());
	client1.wallet.insert (Paper::test_genesis_key.prv);
    Paper::keypair key1;
	Paper::genesis genesis;
    std::unique_ptr <Paper::send_block> send1 (new Paper::send_block);
    send1->hashables.previous = genesis.hash ();
    send1->hashables.balance.clear ();
    send1->hashables.destination = key1.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send1->hash (), send1->signature);
    Paper::publish publish1;
    publish1.block = std::move (send1);
    publish1.work = client1.create_work (*publish1.block);
    Paper::keypair key2;
    std::unique_ptr <Paper::send_block> send2 (new Paper::send_block);
    send2->hashables.previous = genesis.hash ();
    send2->hashables.balance.clear ();
    send2->hashables.destination = key2.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send2->hash (), send2->signature);
    Paper::publish publish2;
    publish2.block = std::move (send2);
    publish2.work = client1.create_work (*publish2.block);
    std::unique_ptr <Paper::send_block> send3 (new Paper::send_block);
    send3->hashables.previous = publish2.block->hash ();
    send3->hashables.balance.clear ();
    send3->hashables.destination = key2.pub;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, send3->hash (), send3->signature);
    Paper::publish publish3;
    publish3.block = std::move (send3);
    client1.processor.process_message (publish1, Paper::endpoint {});
	client2.processor.process_message (publish2, Paper::endpoint {});
    client2.processor.process_message (publish3, Paper::endpoint {});
    ASSERT_EQ (0, client1.conflicts.roots.size ());
    ASSERT_EQ (0, client2.conflicts.roots.size ());
    client1.processor.process_message (publish2, Paper::endpoint {});
    client1.processor.process_message (publish3, Paper::endpoint {});
	client2.processor.process_message (publish1, Paper::endpoint {});
    ASSERT_EQ (1, client1.conflicts.roots.size ());
    ASSERT_EQ (1, client2.conflicts.roots.size ());
    auto conflict (client2.conflicts.roots.find (genesis.hash ()));
    ASSERT_NE (client2.conflicts.roots.end (), conflict);
    auto votes1 (conflict->second);
    ASSERT_NE (nullptr, votes1);
    ASSERT_EQ (1, votes1->votes.rep_votes.size ());
	ASSERT_TRUE (client1.store.block_exists (publish1.block->hash ()));
	ASSERT_TRUE (client2.store.block_exists (publish2.block->hash ()));
    ASSERT_TRUE (client2.store.block_exists (publish3.block->hash ()));
    auto iterations (0);
    while (votes1->votes.rep_votes.size () == 1)
	{
		system.service->poll_one ();
		system.processor.poll_one ();
        ++iterations;
        ASSERT_LT (iterations, 200);
	}
    auto winner (votes1->votes.winner ());
    ASSERT_EQ (*publish1.block, *winner.first);
    ASSERT_EQ (std::numeric_limits <Paper::uint128_t>::max (), winner.second);
	ASSERT_TRUE (client1.store.block_exists (publish1.block->hash ()));
	ASSERT_TRUE (client2.store.block_exists (publish1.block->hash ()));
	ASSERT_FALSE (client2.store.block_exists (publish2.block->hash ()));
    ASSERT_FALSE (client2.store.block_exists (publish3.block->hash ()));
}

TEST (ledger, fail_change_old)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::keypair key1;
    Paper::change_block block (key1.pub, genesis.hash (), Paper::test_genesis_key.prv, Paper::test_genesis_key.pub);
    auto result1 (ledger.process (block));
    ASSERT_EQ (Paper::process_result::progress, result1);
    auto result2 (ledger.process (block));
    ASSERT_EQ (Paper::process_result::old, result2);
}

TEST (ledger, fail_change_gap_previous)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::keypair key1;
    Paper::change_block block (key1.pub, 1, Paper::test_genesis_key.prv, Paper::test_genesis_key.pub);
    auto result1 (ledger.process (block));
    ASSERT_EQ (Paper::process_result::gap_previous, result1);
}

TEST (ledger, fail_change_bad_signature)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::keypair key1;
    Paper::change_block block (key1.pub, genesis.hash (), Paper::private_key (0), Paper::public_key (0));
    auto result1 (ledger.process (block));
    ASSERT_EQ (Paper::process_result::bad_signature, result1);
}

TEST (ledger, fail_change_fork)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::keypair key1;
    Paper::change_block block1 (key1.pub, genesis.hash (), Paper::test_genesis_key.prv, Paper::test_genesis_key.pub);
    auto result1 (ledger.process (block1));
    ASSERT_EQ (Paper::process_result::progress, result1);
    Paper::keypair key2;
    Paper::change_block block2 (key2.pub, genesis.hash (), Paper::test_genesis_key.prv, Paper::test_genesis_key.pub);
    auto result2 (ledger.process (block2));
    ASSERT_EQ (Paper::process_result::fork_previous, result2);
}

TEST (ledger, fail_send_old)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::send_block block;
    Paper::keypair key1;
    block.hashables.destination = key1.pub;
    block.hashables.previous = genesis.hash ();
    block.hashables.balance = 1;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block.hash (), block.signature);
    auto result1 (ledger.process (block));
    ASSERT_EQ (Paper::process_result::progress, result1);
    auto result2 (ledger.process (block));
    ASSERT_EQ (Paper::process_result::old, result2);
}

TEST (ledger, fail_send_gap_previous)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::send_block block;
    Paper::keypair key1;
    block.hashables.destination = key1.pub;
    block.hashables.previous = 1;
    block.hashables.balance = 1;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block.hash (), block.signature);
    auto result1 (ledger.process (block));
    ASSERT_EQ (Paper::process_result::gap_previous, result1);
}

TEST (ledger, fail_send_bad_signature)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::send_block block;
    Paper::keypair key1;
    block.hashables.destination = key1.pub;
    block.hashables.previous = genesis.hash ();
    block.hashables.balance = 1;
    block.signature.clear ();
    auto result1 (ledger.process (block));
    ASSERT_EQ (Paper::process_result::bad_signature, result1);
}

TEST (ledger, fail_send_overspend)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::send_block block1;
    Paper::keypair key1;
    block1.hashables.destination = key1.pub;
    block1.hashables.previous = genesis.hash ();
    block1.hashables.balance = 1;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block1.hash (), block1.signature);
    auto result1 (ledger.process (block1));
    ASSERT_EQ (Paper::process_result::progress, result1);
    Paper::send_block block2;
    Paper::keypair key2;
    block2.hashables.destination = key2.pub;
    block2.hashables.previous = block1.hash ();
    block2.hashables.balance = 2;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block2.hash (), block2.signature);
    auto result2 (ledger.process (block2));
    ASSERT_EQ (Paper::process_result::overspend, result2);
}

TEST (ledger, fail_send_fork)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::send_block block1;
    Paper::keypair key1;
    block1.hashables.destination = key1.pub;
    block1.hashables.previous = genesis.hash ();
    block1.hashables.balance = 1;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block1.hash (), block1.signature);
    auto result1 (ledger.process (block1));
    ASSERT_EQ (Paper::process_result::progress, result1);
    Paper::send_block block2;
    Paper::keypair key2;
    block2.hashables.destination = key2.pub;
    block2.hashables.previous = genesis.hash ();
    block2.hashables.balance = 1;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block2.hash (), block2.signature);
    auto result2 (ledger.process (block2));
    ASSERT_EQ (Paper::process_result::fork_previous, result2);
}

TEST (ledger, fail_open_old)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::send_block block1;
    Paper::keypair key1;
    block1.hashables.destination = key1.pub;
    block1.hashables.previous = genesis.hash ();
    block1.hashables.balance = 1;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block1.hash (), block1.signature);
    auto result1 (ledger.process (block1));
    ASSERT_EQ (Paper::process_result::progress, result1);
    Paper::open_block block2;
    block2.hashables.representative.clear ();
    block2.hashables.source = block1.hash ();
    Paper::sign_message (key1.prv, key1.pub, block2.hash (), block2.signature);
    auto result2 (ledger.process (block2));
    ASSERT_EQ (Paper::process_result::progress, result2);
    auto result3 (ledger.process (block2));
    ASSERT_EQ (Paper::process_result::old, result3);
}

TEST (ledger, fail_open_gap_source)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::keypair key1;
    Paper::open_block block2;
    block2.hashables.representative.clear ();
    block2.hashables.source = 1;
    Paper::sign_message (key1.prv, key1.pub, block2.hash (), block2.signature);
    auto result2 (ledger.process (block2));
    ASSERT_EQ (Paper::process_result::gap_source, result2);
}

TEST (ledger, fail_open_overreceive)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::send_block block1;
    Paper::keypair key1;
    block1.hashables.destination = key1.pub;
    block1.hashables.previous = genesis.hash ();
    block1.hashables.balance = 1;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block1.hash (), block1.signature);
    auto result1 (ledger.process (block1));
    ASSERT_EQ (Paper::process_result::progress, result1);
    Paper::open_block block2;
    block2.hashables.representative.clear ();
    block2.hashables.source = block1.hash ();
    Paper::sign_message (key1.prv, key1.pub, block2.hash (), block2.signature);
    auto result2 (ledger.process (block2));
    ASSERT_EQ (Paper::process_result::progress, result2);
    Paper::open_block block3;
    block3.hashables.representative = 1;
    block3.hashables.source = block1.hash ();;
    Paper::sign_message (key1.prv, key1.pub, block3.hash (), block3.signature);
    auto result3 (ledger.process (block3));
    ASSERT_EQ (Paper::process_result::fork_source, result3);
}

TEST (ledger, fail_open_bad_signature)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::send_block block1;
    Paper::keypair key1;
    block1.hashables.destination = key1.pub;
    block1.hashables.previous = genesis.hash ();
    block1.hashables.balance = 1;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block1.hash (), block1.signature);
    auto result1 (ledger.process (block1));
    ASSERT_EQ (Paper::process_result::progress, result1);
    Paper::open_block block2;
    block2.hashables.representative.clear ();
    block2.hashables.source = block1.hash ();
    block2.signature.clear ();
    auto result2 (ledger.process (block2));
    ASSERT_EQ (Paper::process_result::bad_signature, result2);
}

TEST (ledger, fail_open_fork_previous)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::send_block block1;
    Paper::keypair key1;
    block1.hashables.destination = key1.pub;
    block1.hashables.previous = genesis.hash ();
    block1.hashables.balance = 1;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block1.hash (), block1.signature);
    auto result1 (ledger.process (block1));
    ASSERT_EQ (Paper::process_result::progress, result1);
    Paper::send_block block2;
    block2.hashables.destination = key1.pub;
    block2.hashables.previous = block1.hash ();
    block2.hashables.balance.clear ();
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block2.hash (), block2.signature);
    auto result2 (ledger.process (block2));
    ASSERT_EQ (Paper::process_result::progress, result2);
    Paper::open_block block3;
    block3.hashables.representative.clear ();
    block3.hashables.source = block1.hash ();
    Paper::sign_message (key1.prv, key1.pub, block3.hash (), block3.signature);
    auto result3 (ledger.process (block3));
    ASSERT_EQ (Paper::process_result::progress, result3);
    Paper::open_block block4;
    block4.hashables.representative.clear ();
    block4.hashables.source = block2.hash ();
    Paper::sign_message (key1.prv, key1.pub, block4.hash (), block4.signature);
    auto result4 (ledger.process (block4));
    ASSERT_EQ (Paper::process_result::fork_previous, result4);
}

TEST (ledger, fail_receive_old)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::send_block block1;
    Paper::keypair key1;
    block1.hashables.destination = key1.pub;
    block1.hashables.previous = genesis.hash ();
    block1.hashables.balance = 1;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block1.hash (), block1.signature);
    auto result1 (ledger.process (block1));
    ASSERT_EQ (Paper::process_result::progress, result1);
    Paper::send_block block2;
    block2.hashables.destination = key1.pub;
    block2.hashables.previous = block1.hash ();
    block2.hashables.balance.clear ();
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block2.hash (), block2.signature);
    auto result2 (ledger.process (block2));
    ASSERT_EQ (Paper::process_result::progress, result2);
    Paper::open_block block3;
    block3.hashables.representative.clear ();
    block3.hashables.source = block1.hash ();
    Paper::sign_message (key1.prv, key1.pub, block3.hash (), block3.signature);
    auto result3 (ledger.process (block3));
    ASSERT_EQ (Paper::process_result::progress, result3);
    Paper::receive_block block4;
    block4.hashables.previous = block3.hash ();
    block4.hashables.source = block2.hash ();
    Paper::sign_message (key1.prv, key1.pub, block4.hash (), block4.signature);
    auto result4 (ledger.process (block4));
    ASSERT_EQ (Paper::process_result::progress, result4);
    auto result5 (ledger.process (block4));
    ASSERT_EQ (Paper::process_result::old, result5);
}

TEST (ledger, fail_receive_gap_source)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::send_block block1;
    Paper::keypair key1;
    block1.hashables.destination = key1.pub;
    block1.hashables.previous = genesis.hash ();
    block1.hashables.balance = 1;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block1.hash (), block1.signature);
    auto result1 (ledger.process (block1));
    ASSERT_EQ (Paper::process_result::progress, result1);
    Paper::send_block block2;
    block2.hashables.destination = key1.pub;
    block2.hashables.previous = block1.hash ();
    block2.hashables.balance.clear ();
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block2.hash (), block2.signature);
    auto result2 (ledger.process (block2));
    ASSERT_EQ (Paper::process_result::progress, result2);
    Paper::open_block block3;
    block3.hashables.representative.clear ();
    block3.hashables.source = block1.hash ();
    Paper::sign_message (key1.prv, key1.pub, block3.hash (), block3.signature);
    auto result3 (ledger.process (block3));
    ASSERT_EQ (Paper::process_result::progress, result3);
    Paper::receive_block block4;
    block4.hashables.previous = block3.hash ();
    block4.hashables.source = 1;
    Paper::sign_message (key1.prv, key1.pub, block4.hash (), block4.signature);
    auto result4 (ledger.process (block4));
    ASSERT_EQ (Paper::process_result::gap_source, result4);
}

TEST (ledger, fail_receive_overreceive)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::send_block block1;
    Paper::keypair key1;
    block1.hashables.destination = key1.pub;
    block1.hashables.previous = genesis.hash ();
    block1.hashables.balance = 1;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block1.hash (), block1.signature);
    auto result1 (ledger.process (block1));
    ASSERT_EQ (Paper::process_result::progress, result1);
    Paper::open_block block2;
    block2.hashables.representative.clear ();
    block2.hashables.source = block1.hash ();
    Paper::sign_message (key1.prv, key1.pub, block2.hash (), block2.signature);
    auto result3 (ledger.process (block2));
    ASSERT_EQ (Paper::process_result::progress, result3);
    Paper::receive_block block3;
    block3.hashables.previous = block2.hash ();
    block3.hashables.source = block1.hash ();
    Paper::sign_message (key1.prv, key1.pub, block3.hash (), block3.signature);
    auto result4 (ledger.process (block3));
    ASSERT_EQ (Paper::process_result::overreceive, result4);
}

TEST (ledger, fail_receive_bad_signature)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::send_block block1;
    Paper::keypair key1;
    block1.hashables.destination = key1.pub;
    block1.hashables.previous = genesis.hash ();
    block1.hashables.balance = 1;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block1.hash (), block1.signature);
    auto result1 (ledger.process (block1));
    ASSERT_EQ (Paper::process_result::progress, result1);
    Paper::send_block block2;
    block2.hashables.destination = key1.pub;
    block2.hashables.previous = block1.hash ();
    block2.hashables.balance.clear ();
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block2.hash (), block2.signature);
    auto result2 (ledger.process (block2));
    ASSERT_EQ (Paper::process_result::progress, result2);
    Paper::open_block block3;
    block3.hashables.representative.clear ();
    block3.hashables.source = block1.hash ();
    Paper::sign_message (key1.prv, key1.pub, block3.hash (), block3.signature);
    auto result3 (ledger.process (block3));
    ASSERT_EQ (Paper::process_result::progress, result3);
    Paper::receive_block block4;
    block4.hashables.previous = block3.hash ();
    block4.hashables.source = block2.hash ();
    block4.signature.clear ();
    auto result4 (ledger.process (block4));
    ASSERT_EQ (Paper::process_result::bad_signature, result4);
}

TEST (ledger, fail_receive_gap_previous_opened)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::send_block block1;
    Paper::keypair key1;
    block1.hashables.destination = key1.pub;
    block1.hashables.previous = genesis.hash ();
    block1.hashables.balance = 1;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block1.hash (), block1.signature);
    auto result1 (ledger.process (block1));
    ASSERT_EQ (Paper::process_result::progress, result1);
    Paper::send_block block2;
    block2.hashables.destination = key1.pub;
    block2.hashables.previous = block1.hash ();
    block2.hashables.balance.clear ();
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block2.hash (), block2.signature);
    auto result2 (ledger.process (block2));
    ASSERT_EQ (Paper::process_result::progress, result2);
    Paper::open_block block3;
    block3.hashables.representative.clear ();
    block3.hashables.source = block1.hash ();
    Paper::sign_message (key1.prv, key1.pub, block3.hash (), block3.signature);
    auto result3 (ledger.process (block3));
    ASSERT_EQ (Paper::process_result::progress, result3);
    Paper::receive_block block4;
    block4.hashables.previous = 1;
    block4.hashables.source = block2.hash ();
    Paper::sign_message (key1.prv, key1.pub, block4.hash (), block4.signature);
    auto result4 (ledger.process (block4));
    ASSERT_EQ (Paper::process_result::gap_previous, result4);
}

TEST (ledger, fail_receive_gap_previous_unopened)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::send_block block1;
    Paper::keypair key1;
    block1.hashables.destination = key1.pub;
    block1.hashables.previous = genesis.hash ();
    block1.hashables.balance = 1;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block1.hash (), block1.signature);
    auto result1 (ledger.process (block1));
    ASSERT_EQ (Paper::process_result::progress, result1);
    Paper::send_block block2;
    block2.hashables.destination = key1.pub;
    block2.hashables.previous = block1.hash ();
    block2.hashables.balance.clear ();
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block2.hash (), block2.signature);
    auto result2 (ledger.process (block2));
    ASSERT_EQ (Paper::process_result::progress, result2);
    Paper::receive_block block3;
    block3.hashables.previous = 1;
    block3.hashables.source = block2.hash ();
    Paper::sign_message (key1.prv, key1.pub, block3.hash (), block3.signature);
    auto result3 (ledger.process (block3));
    ASSERT_EQ (Paper::process_result::gap_previous, result3);
}

TEST (ledger, fail_receive_fork_previous)
{
    leveldb::Status init;
    Paper::block_store store (init, Paper::block_store_temp);
    ASSERT_TRUE (init.ok ());
    bool init1;
    Paper::ledger ledger (init1, init, store);
    ASSERT_FALSE (init1);
    Paper::genesis genesis;
    genesis.initialize (store);
    Paper::send_block block1;
    Paper::keypair key1;
    block1.hashables.destination = key1.pub;
    block1.hashables.previous = genesis.hash ();
    block1.hashables.balance = 1;
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block1.hash (), block1.signature);
    auto result1 (ledger.process (block1));
    ASSERT_EQ (Paper::process_result::progress, result1);
    Paper::send_block block2;
    block2.hashables.destination = key1.pub;
    block2.hashables.previous = block1.hash ();
    block2.hashables.balance.clear ();
    Paper::sign_message (Paper::test_genesis_key.prv, Paper::test_genesis_key.pub, block2.hash (), block2.signature);
    auto result2 (ledger.process (block2));
    ASSERT_EQ (Paper::process_result::progress, result2);
    Paper::open_block block3;
    block3.hashables.representative.clear ();
    block3.hashables.source = block1.hash ();
    Paper::sign_message (key1.prv, key1.pub, block3.hash (), block3.signature);
    auto result3 (ledger.process (block3));
    ASSERT_EQ (Paper::process_result::progress, result3);
    Paper::send_block block4;
    Paper::keypair key2;
    block4.hashables.destination = key1.pub;
    block4.hashables.previous = block3.hash ();
    block4.hashables.balance = 1;
    Paper::sign_message (key1.prv, key1.pub, block4.hash (), block4.signature);
    auto result4 (ledger.process (block4));
    ASSERT_EQ (Paper::process_result::progress, result4);
    Paper::receive_block block5;
    block5.hashables.previous = block3.hash ();
    block5.hashables.source = block2.hash ();
    Paper::sign_message (key1.prv, key1.pub, block5.hash (), block5.signature);
    auto result5 (ledger.process (block5));
    ASSERT_EQ (Paper::process_result::fork_previous, result5);
}