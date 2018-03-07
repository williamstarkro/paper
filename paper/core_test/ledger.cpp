#include <cryptopp/filters.h>
#include <cryptopp/randpool.h>
#include <gtest/gtest.h>
#include <paper/node/testing.hpp>

// Init returns an error if it can't open files at the path
TEST (ledger, store_error)
{
	bool init (false);
	paper::block_store store (init, boost::filesystem::path ("///"));
	ASSERT_FALSE (!init);
	paper::ledger ledger (store);
}

// Ledger can be initialized and returns a basic query for an empty account
TEST (ledger, empty)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store);
	paper::account account;
	paper::transaction transaction (store.environment, nullptr, false);
	auto balance (ledger.account_balance (transaction, account));
	ASSERT_TRUE (balance.is_zero ());
}

// Genesis account should have the max balance on empty initialization
TEST (ledger, genesis_balance)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	auto balance (ledger.account_balance (transaction, paper::genesis_account));
	ASSERT_EQ (paper::genesis_amount, balance);
	auto amount (ledger.amount (transaction, paper::genesis_account));
	ASSERT_EQ (paper::genesis_amount, amount);
	paper::account_info info;
	ASSERT_FALSE (store.account_get (transaction, paper::genesis_account, info));
	// Frontier time should have been updated when genesis balance was added
	ASSERT_GE (paper::seconds_since_epoch (), info.modified);
	ASSERT_LT (paper::seconds_since_epoch () - info.modified, 10);
}

// Make sure the checksum is the same when ledger reloaded
TEST (ledger, checksum_persistence)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::uint256_union checksum1;
	paper::uint256_union max;
	max.qwords[0] = 0;
	max.qwords[0] = ~max.qwords[0];
	max.qwords[1] = 0;
	max.qwords[1] = ~max.qwords[1];
	max.qwords[2] = 0;
	max.qwords[2] = ~max.qwords[2];
	max.qwords[3] = 0;
	max.qwords[3] = ~max.qwords[3];
	paper::transaction transaction (store.environment, nullptr, true);
	{
		paper::ledger ledger (store);
		paper::genesis genesis;
		genesis.initialize (transaction, store);
		checksum1 = ledger.checksum (transaction, 0, max);
	}
	paper::ledger ledger (store);
	ASSERT_EQ (checksum1, ledger.checksum (transaction, 0, max));
}

// All nodes in the system should agree on the genesis balance
TEST (system, system_genesis)
{
	paper::system system (24000, 2);
	for (auto & i : system.nodes)
	{
		paper::transaction transaction (i->store.environment, nullptr, false);
		ASSERT_EQ (paper::genesis_amount, i->ledger.account_balance (transaction, paper::genesis_account));
	}
}

// Create a send block and publish it.
TEST (ledger, process_send)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store);
	paper::transaction transaction (store.environment, nullptr, true);
	paper::genesis genesis;
	genesis.initialize (transaction, store);
	paper::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info1));
	paper::keypair key2;
	paper::send_block send (info1.head, key2.pub, 50, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	paper::block_hash hash1 (send.hash ());
	ASSERT_EQ (paper::test_genesis_key.pub, store.frontier_get (transaction, info1.head));
	ASSERT_EQ (1, info1.block_count);
	// This was a valid block, it should progress.
	auto return1 (ledger.process (transaction, send));
	ASSERT_EQ (paper::genesis_amount - 50, ledger.amount (transaction, hash1));
	ASSERT_TRUE (store.frontier_get (transaction, info1.head).is_zero ());
	ASSERT_EQ (paper::test_genesis_key.pub, store.frontier_get (transaction, hash1));
	ASSERT_EQ (paper::process_result::progress, return1.code);
	ASSERT_EQ (paper::test_genesis_key.pub, return1.account);
	ASSERT_EQ (paper::genesis_amount - 50, return1.amount.number ());
	ASSERT_EQ (50, ledger.account_balance (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (paper::genesis_amount - 50, ledger.account_pending (transaction, key2.pub));
	paper::account_info info2;
	ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info2));
	ASSERT_EQ (2, info2.block_count);
	auto latest6 (store.block_get (transaction, info2.head));
	ASSERT_NE (nullptr, latest6);
	auto latest7 (dynamic_cast<paper::send_block *> (latest6.get ()));
	ASSERT_NE (nullptr, latest7);
	ASSERT_EQ (send, *latest7);
	// Create an open block opening an account accepting the send we just created
	paper::open_block open (hash1, key2.pub, key2.pub, key2.prv, key2.pub, 0);
	paper::block_hash hash2 (open.hash ());
	// This was a valid block, it should progress.
	auto return2 (ledger.process (transaction, open));
	ASSERT_EQ (paper::genesis_amount - 50, ledger.amount (transaction, hash2));
	ASSERT_EQ (paper::process_result::progress, return2.code);
	ASSERT_EQ (key2.pub, return2.account);
	ASSERT_EQ (paper::genesis_amount - 50, return2.amount.number ());
	ASSERT_EQ (key2.pub, store.frontier_get (transaction, hash2));
	ASSERT_EQ (paper::genesis_amount - 50, ledger.account_balance (transaction, key2.pub));
	ASSERT_EQ (0, ledger.account_pending (transaction, key2.pub));
	ASSERT_EQ (50, ledger.weight (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (paper::genesis_amount - 50, ledger.weight (transaction, key2.pub));
	paper::account_info info3;
	ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info3));
	auto latest2 (store.block_get (transaction, info3.head));
	ASSERT_NE (nullptr, latest2);
	auto latest3 (dynamic_cast<paper::send_block *> (latest2.get ()));
	ASSERT_NE (nullptr, latest3);
	ASSERT_EQ (send, *latest3);
	paper::account_info info4;
	ASSERT_FALSE (store.account_get (transaction, key2.pub, info4));
	auto latest4 (store.block_get (transaction, info4.head));
	ASSERT_NE (nullptr, latest4);
	auto latest5 (dynamic_cast<paper::open_block *> (latest4.get ()));
	ASSERT_NE (nullptr, latest5);
	ASSERT_EQ (open, *latest5);
	ledger.rollback (transaction, hash2);
	ASSERT_TRUE (store.frontier_get (transaction, hash2).is_zero ());
	paper::account_info info5;
	ASSERT_TRUE (ledger.store.account_get (transaction, key2.pub, info5));
	paper::pending_info pending1;
	ASSERT_FALSE (ledger.store.pending_get (transaction, paper::pending_key (key2.pub, hash1), pending1));
	ASSERT_EQ (paper::test_genesis_key.pub, pending1.source);
	ASSERT_EQ (paper::genesis_amount - 50, pending1.amount.number ());
	ASSERT_EQ (0, ledger.account_balance (transaction, key2.pub));
	ASSERT_EQ (paper::genesis_amount - 50, ledger.account_pending (transaction, key2.pub));
	ASSERT_EQ (50, ledger.account_balance (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (50, ledger.weight (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	paper::account_info info6;
	ASSERT_FALSE (ledger.store.account_get (transaction, paper::test_genesis_key.pub, info6));
	ASSERT_EQ (hash1, info6.head);
	ledger.rollback (transaction, info6.head);
	ASSERT_EQ (paper::genesis_amount, ledger.weight (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (paper::test_genesis_key.pub, store.frontier_get (transaction, info1.head));
	ASSERT_TRUE (store.frontier_get (transaction, hash1).is_zero ());
	paper::account_info info7;
	ASSERT_FALSE (ledger.store.account_get (transaction, paper::test_genesis_key.pub, info7));
	ASSERT_EQ (1, info7.block_count);
	ASSERT_EQ (info1.head, info7.head);
	paper::pending_info pending2;
	ASSERT_TRUE (ledger.store.pending_get (transaction, paper::pending_key (key2.pub, hash1), pending2));
	ASSERT_EQ (paper::genesis_amount, ledger.account_balance (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.account_pending (transaction, key2.pub));
}

TEST (ledger, process_receive)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info1));
	paper::keypair key2;
	paper::send_block send (info1.head, key2.pub, 50, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	paper::block_hash hash1 (send.hash ());
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, send).code);
	paper::keypair key3;
	paper::open_block open (hash1, key3.pub, key2.pub, key2.prv, key2.pub, 0);
	paper::block_hash hash2 (open.hash ());
	auto return1 (ledger.process (transaction, open));
	ASSERT_EQ (paper::process_result::progress, return1.code);
	ASSERT_EQ (key2.pub, return1.account);
	ASSERT_EQ (paper::genesis_amount - 50, return1.amount.number ());
	ASSERT_EQ (paper::genesis_amount - 50, ledger.weight (transaction, key3.pub));
	paper::send_block send2 (hash1, key2.pub, 25, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	paper::block_hash hash3 (send2.hash ());
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, send2).code);
	paper::receive_block receive (hash2, hash3, key2.prv, key2.pub, 0);
	auto hash4 (receive.hash ());
	ASSERT_EQ (key2.pub, store.frontier_get (transaction, hash2));
	auto return2 (ledger.process (transaction, receive));
	ASSERT_EQ (25, ledger.amount (transaction, hash4));
	ASSERT_TRUE (store.frontier_get (transaction, hash2).is_zero ());
	ASSERT_EQ (key2.pub, store.frontier_get (transaction, hash4));
	ASSERT_EQ (paper::process_result::progress, return2.code);
	ASSERT_EQ (key2.pub, return2.account);
	ASSERT_EQ (25, return2.amount.number ());
	ASSERT_EQ (hash4, ledger.latest (transaction, key2.pub));
	ASSERT_EQ (25, ledger.account_balance (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.account_pending (transaction, key2.pub));
	ASSERT_EQ (paper::genesis_amount - 25, ledger.account_balance (transaction, key2.pub));
	ASSERT_EQ (paper::genesis_amount - 25, ledger.weight (transaction, key3.pub));
	ledger.rollback (transaction, hash4);
	ASSERT_TRUE (store.block_successor (transaction, hash2).is_zero ());
	ASSERT_EQ (key2.pub, store.frontier_get (transaction, hash2));
	ASSERT_TRUE (store.frontier_get (transaction, hash4).is_zero ());
	ASSERT_EQ (25, ledger.account_balance (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (25, ledger.account_pending (transaction, key2.pub));
	ASSERT_EQ (paper::genesis_amount - 50, ledger.account_balance (transaction, key2.pub));
	ASSERT_EQ (paper::genesis_amount - 50, ledger.weight (transaction, key3.pub));
	ASSERT_EQ (hash2, ledger.latest (transaction, key2.pub));
	paper::pending_info pending1;
	ASSERT_FALSE (ledger.store.pending_get (transaction, paper::pending_key (key2.pub, hash3), pending1));
	ASSERT_EQ (paper::test_genesis_key.pub, pending1.source);
	ASSERT_EQ (25, pending1.amount.number ());
}

TEST (ledger, rollback_receiver)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info1));
	paper::keypair key2;
	paper::send_block send (info1.head, key2.pub, 50, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	paper::block_hash hash1 (send.hash ());
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, send).code);
	paper::keypair key3;
	paper::open_block open (hash1, key3.pub, key2.pub, key2.prv, key2.pub, 0);
	paper::block_hash hash2 (open.hash ());
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, open).code);
	ASSERT_EQ (hash2, ledger.latest (transaction, key2.pub));
	ASSERT_EQ (50, ledger.account_balance (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (paper::genesis_amount - 50, ledger.account_balance (transaction, key2.pub));
	ASSERT_EQ (50, ledger.weight (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (paper::genesis_amount - 50, ledger.weight (transaction, key3.pub));
	ledger.rollback (transaction, hash1);
	ASSERT_EQ (paper::genesis_amount, ledger.account_balance (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.account_balance (transaction, key2.pub));
	ASSERT_EQ (paper::genesis_amount, ledger.weight (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key3.pub));
	paper::account_info info2;
	ASSERT_TRUE (ledger.store.account_get (transaction, key2.pub, info2));
	paper::pending_info pending1;
	ASSERT_TRUE (ledger.store.pending_get (transaction, paper::pending_key (key2.pub, info2.head), pending1));
}

TEST (ledger, rollback_representation)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key5;
	paper::change_block change1 (genesis.hash (), key5.pub, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, change1).code);
	paper::keypair key3;
	paper::change_block change2 (change1.hash (), key3.pub, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, change2).code);
	paper::keypair key2;
	paper::send_block send1 (change2.hash (), key2.pub, 50, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, send1).code);
	paper::keypair key4;
	paper::open_block open (send1.hash (), key4.pub, key2.pub, key2.prv, key2.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, open).code);
	paper::send_block send2 (send1.hash (), key2.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, send2).code);
	paper::receive_block receive1 (open.hash (), send2.hash (), key2.prv, key2.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, receive1).code);
	ASSERT_EQ (1, ledger.weight (transaction, key3.pub));
	ASSERT_EQ (paper::genesis_amount - 1, ledger.weight (transaction, key4.pub));
	paper::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, key2.pub, info1));
	ASSERT_EQ (open.hash (), info1.rep_block);
	ledger.rollback (transaction, receive1.hash ());
	paper::account_info info2;
	ASSERT_FALSE (store.account_get (transaction, key2.pub, info2));
	ASSERT_EQ (open.hash (), info2.rep_block);
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (paper::genesis_amount - 50, ledger.weight (transaction, key4.pub));
	ledger.rollback (transaction, open.hash ());
	ASSERT_EQ (1, ledger.weight (transaction, key3.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key4.pub));
	ledger.rollback (transaction, send1.hash ());
	ASSERT_EQ (paper::genesis_amount, ledger.weight (transaction, key3.pub));
	paper::account_info info3;
	ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info3));
	ASSERT_EQ (change2.hash (), info3.rep_block);
	ledger.rollback (transaction, change2.hash ());
	paper::account_info info4;
	ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info4));
	ASSERT_EQ (change1.hash (), info4.rep_block);
	ASSERT_EQ (paper::genesis_amount, ledger.weight (transaction, key5.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key3.pub));
}

TEST (ledger, process_duplicate)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info1));
	paper::keypair key2;
	paper::send_block send (info1.head, key2.pub, 50, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	paper::block_hash hash1 (send.hash ());
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, send).code);
	ASSERT_EQ (paper::process_result::old, ledger.process (transaction, send).code);
	paper::open_block open (hash1, 1, key2.pub, key2.prv, key2.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, open).code);
	ASSERT_EQ (paper::process_result::old, ledger.process (transaction, open).code);
}

TEST (ledger, representative_genesis)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	auto latest (ledger.latest (transaction, paper::test_genesis_key.pub));
	ASSERT_FALSE (latest.is_zero ());
	ASSERT_EQ (genesis.open->hash (), ledger.representative (transaction, latest));
}

TEST (ledger, weight)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	ASSERT_EQ (paper::genesis_amount, ledger.weight (transaction, paper::genesis_account));
}

TEST (ledger, representative_change)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store);
	paper::keypair key2;
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	ASSERT_EQ (paper::genesis_amount, ledger.weight (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	paper::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info1));
	paper::change_block block (info1.head, key2.pub, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::test_genesis_key.pub, store.frontier_get (transaction, info1.head));
	auto return1 (ledger.process (transaction, block));
	ASSERT_EQ (0, ledger.amount (transaction, block.hash ()));
	ASSERT_TRUE (store.frontier_get (transaction, info1.head).is_zero ());
	ASSERT_EQ (paper::test_genesis_key.pub, store.frontier_get (transaction, block.hash ()));
	ASSERT_EQ (paper::process_result::progress, return1.code);
	ASSERT_EQ (paper::test_genesis_key.pub, return1.account);
	ASSERT_EQ (0, ledger.weight (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (paper::genesis_amount, ledger.weight (transaction, key2.pub));
	paper::account_info info2;
	ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info2));
	ASSERT_EQ (block.hash (), info2.head);
	ledger.rollback (transaction, info2.head);
	ASSERT_EQ (paper::test_genesis_key.pub, store.frontier_get (transaction, info1.head));
	ASSERT_TRUE (store.frontier_get (transaction, block.hash ()).is_zero ());
	paper::account_info info3;
	ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info3));
	ASSERT_EQ (info1.head, info3.head);
	ASSERT_EQ (paper::genesis_amount, ledger.weight (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
}

TEST (ledger, send_fork)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store);
	paper::keypair key2;
	paper::keypair key3;
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info1));
	paper::send_block block (info1.head, key2.pub, 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block).code);
	paper::send_block block2 (info1.head, key3.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::fork, ledger.process (transaction, block2).code);
}

TEST (ledger, receive_fork)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store);
	paper::keypair key2;
	paper::keypair key3;
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info1));
	paper::send_block block (info1.head, key2.pub, 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block).code);
	paper::open_block block2 (block.hash (), key2.pub, key2.pub, key2.prv, key2.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block2).code);
	paper::change_block block3 (block2.hash (), key3.pub, key2.prv, key2.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block3).code);
	paper::send_block block4 (block.hash (), key2.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block4).code);
	paper::receive_block block5 (block2.hash (), block4.hash (), key2.prv, key2.pub, 0);
	ASSERT_EQ (paper::process_result::fork, ledger.process (transaction, block5).code);
}

TEST (ledger, open_fork)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store);
	paper::keypair key2;
	paper::keypair key3;
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info1));
	paper::send_block block (info1.head, key2.pub, 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block).code);
	paper::open_block block2 (block.hash (), key2.pub, key2.pub, key2.prv, key2.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block2).code);
	paper::open_block block3 (block.hash (), key3.pub, key2.pub, key2.prv, key2.pub, 0);
	ASSERT_EQ (paper::process_result::fork, ledger.process (transaction, block3).code);
}

TEST (ledger, checksum_single)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::ledger ledger (store);
	store.checksum_put (transaction, 0, 0, genesis.hash ());
	ASSERT_EQ (genesis.hash (), ledger.checksum (transaction, 0, std::numeric_limits<paper::uint256_t>::max ()));
	paper::change_block block1 (ledger.latest (transaction, paper::test_genesis_key.pub), paper::account (1), paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	paper::checksum check1 (ledger.checksum (transaction, 0, std::numeric_limits<paper::uint256_t>::max ()));
	ASSERT_EQ (genesis.hash (), check1);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block1).code);
	paper::checksum check2 (ledger.checksum (transaction, 0, std::numeric_limits<paper::uint256_t>::max ()));
	ASSERT_EQ (block1.hash (), check2);
}

TEST (ledger, checksum_two)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::ledger ledger (store);
	store.checksum_put (transaction, 0, 0, genesis.hash ());
	paper::keypair key2;
	paper::send_block block1 (ledger.latest (transaction, paper::test_genesis_key.pub), key2.pub, 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block1).code);
	paper::checksum check1 (ledger.checksum (transaction, 0, std::numeric_limits<paper::uint256_t>::max ()));
	paper::open_block block2 (block1.hash (), 1, key2.pub, key2.prv, key2.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block2).code);
	paper::checksum check2 (ledger.checksum (transaction, 0, std::numeric_limits<paper::uint256_t>::max ()));
	ASSERT_EQ (check1, check2 ^ block2.hash ());
}

TEST (ledger, DISABLED_checksum_range)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store);
	paper::transaction transaction (store.environment, nullptr, false);
	paper::checksum check1 (ledger.checksum (transaction, 0, std::numeric_limits<paper::uint256_t>::max ()));
	ASSERT_TRUE (check1.is_zero ());
	paper::block_hash hash1 (42);
	paper::checksum check2 (ledger.checksum (transaction, 0, 42));
	ASSERT_TRUE (check2.is_zero ());
	paper::checksum check3 (ledger.checksum (transaction, 42, std::numeric_limits<paper::uint256_t>::max ()));
	ASSERT_EQ (hash1, check3);
}

TEST (system, generate_send_existing)
{
	paper::system system (24000, 1);
	paper::thread_runner runner (system.service, system.nodes[0]->config.io_threads);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	paper::account_info info1;
	{
		paper::transaction transaction (system.wallet (0)->store.environment, nullptr, false);
		ASSERT_FALSE (system.nodes[0]->store.account_get (transaction, paper::test_genesis_key.pub, info1));
	}
	std::vector<paper::account> accounts;
	accounts.push_back (paper::test_genesis_key.pub);
	system.generate_send_existing (*system.nodes[0], accounts);
	paper::account_info info2;
	{
		paper::transaction transaction (system.wallet (0)->store.environment, nullptr, false);
		ASSERT_FALSE (system.nodes[0]->store.account_get (transaction, paper::test_genesis_key.pub, info2));
	}
	ASSERT_NE (info1.head, info2.head);
	auto iterations1 (0);
	while (system.nodes[0]->balance (paper::test_genesis_key.pub) == paper::genesis_amount)
	{
		system.poll ();
		++iterations1;
		ASSERT_LT (iterations1, 20);
	}
	auto iterations2 (0);
	while (system.nodes[0]->balance (paper::test_genesis_key.pub) != paper::genesis_amount)
	{
		system.poll ();
		++iterations2;
		ASSERT_LT (iterations2, 20);
	}
	system.stop ();
	runner.join ();
}

TEST (system, generate_send_new)
{
	paper::system system (24000, 1);
	paper::thread_runner runner (system.service, system.nodes[0]->config.io_threads);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	{
		paper::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
		auto iterator1 (system.nodes[0]->store.latest_begin (transaction));
		ASSERT_NE (system.nodes[0]->store.latest_end (), iterator1);
		++iterator1;
		ASSERT_EQ (system.nodes[0]->store.latest_end (), iterator1);
	}
	std::vector<paper::account> accounts;
	accounts.push_back (paper::test_genesis_key.pub);
	system.generate_send_new (*system.nodes[0], accounts);
	paper::account new_account (0);
	{
		paper::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
		auto iterator2 (system.wallet (0)->store.begin (transaction));
		if (iterator2->first.uint256 () != paper::test_genesis_key.pub)
		{
			new_account = iterator2->first.uint256 ();
		}
		++iterator2;
		ASSERT_NE (system.wallet (0)->store.end (), iterator2);
		if (iterator2->first.uint256 () != paper::test_genesis_key.pub)
		{
			new_account = iterator2->first.uint256 ();
		}
		++iterator2;
		ASSERT_EQ (system.wallet (0)->store.end (), iterator2);
		ASSERT_FALSE (new_account.is_zero ());
	}
	auto iterations (0);
	while (system.nodes[0]->balance (new_account) == 0)
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	system.stop ();
	runner.join ();
}

TEST (ledger, representation)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	ASSERT_EQ (paper::genesis_amount, store.representation_get (transaction, paper::test_genesis_key.pub));
	paper::keypair key2;
	paper::send_block block1 (genesis.hash (), key2.pub, paper::genesis_amount - 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block1).code);
	ASSERT_EQ (paper::genesis_amount - 100, store.representation_get (transaction, paper::test_genesis_key.pub));
	paper::keypair key3;
	paper::open_block block2 (block1.hash (), key3.pub, key2.pub, key2.prv, key2.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block2).code);
	ASSERT_EQ (paper::genesis_amount - 100, store.representation_get (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key2.pub));
	ASSERT_EQ (100, store.representation_get (transaction, key3.pub));
	paper::send_block block3 (block1.hash (), key2.pub, paper::genesis_amount - 200, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block3).code);
	ASSERT_EQ (paper::genesis_amount - 200, store.representation_get (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key2.pub));
	ASSERT_EQ (100, store.representation_get (transaction, key3.pub));
	paper::receive_block block4 (block2.hash (), block3.hash (), key2.prv, key2.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block4).code);
	ASSERT_EQ (paper::genesis_amount - 200, store.representation_get (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key2.pub));
	ASSERT_EQ (200, store.representation_get (transaction, key3.pub));
	paper::keypair key4;
	paper::change_block block5 (block4.hash (), key4.pub, key2.prv, key2.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block5).code);
	ASSERT_EQ (paper::genesis_amount - 200, store.representation_get (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key2.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key3.pub));
	ASSERT_EQ (200, store.representation_get (transaction, key4.pub));
	paper::keypair key5;
	paper::send_block block6 (block5.hash (), key5.pub, 100, key2.prv, key2.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block6).code);
	ASSERT_EQ (paper::genesis_amount - 200, store.representation_get (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key2.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key3.pub));
	ASSERT_EQ (100, store.representation_get (transaction, key4.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key5.pub));
	paper::keypair key6;
	paper::open_block block7 (block6.hash (), key6.pub, key5.pub, key5.prv, key5.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block7).code);
	ASSERT_EQ (paper::genesis_amount - 200, store.representation_get (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key2.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key3.pub));
	ASSERT_EQ (100, store.representation_get (transaction, key4.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key5.pub));
	ASSERT_EQ (100, store.representation_get (transaction, key6.pub));
	paper::send_block block8 (block6.hash (), key5.pub, 0, key2.prv, key2.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block8).code);
	ASSERT_EQ (paper::genesis_amount - 200, store.representation_get (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key2.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key3.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key4.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key5.pub));
	ASSERT_EQ (100, store.representation_get (transaction, key6.pub));
	paper::receive_block block9 (block7.hash (), block8.hash (), key5.prv, key5.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block9).code);
	ASSERT_EQ (paper::genesis_amount - 200, store.representation_get (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key2.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key3.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key4.pub));
	ASSERT_EQ (0, store.representation_get (transaction, key5.pub));
	ASSERT_EQ (200, store.representation_get (transaction, key6.pub));
}

TEST (ledger, double_open)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key2;
	paper::send_block send1 (genesis.hash (), key2.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, send1).code);
	paper::open_block open1 (send1.hash (), key2.pub, key2.pub, key2.prv, key2.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, open1).code);
	paper::open_block open2 (send1.hash (), paper::test_genesis_key.pub, key2.pub, key2.prv, key2.pub, 0);
	ASSERT_EQ (paper::process_result::fork, ledger.process (transaction, open2).code);
}

TEST (ledegr, double_receive)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key2;
	paper::send_block send1 (genesis.hash (), key2.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, send1).code);
	paper::open_block open1 (send1.hash (), key2.pub, key2.pub, key2.prv, key2.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, open1).code);
	paper::receive_block receive1 (open1.hash (), send1.hash (), key2.prv, key2.pub, 0);
	ASSERT_EQ (paper::process_result::unreceivable, ledger.process (transaction, receive1).code);
}

TEST (votes, add_one)
{
	paper::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	paper::genesis genesis;
	paper::keypair key1;
	auto send1 (std::make_shared<paper::send_block> (genesis.hash (), key1.pub, paper::genesis_amount - 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0));
	{
		paper::transaction transaction (node1.store.environment, nullptr, true);
		ASSERT_EQ (paper::process_result::progress, node1.ledger.process (transaction, *send1).code);
	}
	auto node_l (system.nodes[0]);
	{
		paper::transaction transaction (node1.store.environment, nullptr, true);
		node1.active.start (transaction, send1);
	}
	auto votes1 (node1.active.roots.find (send1->root ())->election);
	ASSERT_EQ (1, votes1->votes.rep_votes.size ());
	auto vote1 (std::make_shared<paper::vote> (paper::test_genesis_key.pub, paper::test_genesis_key.prv, 1, send1));
	votes1->vote (vote1);
	auto vote2 (std::make_shared<paper::vote> (paper::test_genesis_key.pub, paper::test_genesis_key.prv, 2, send1));
	votes1->vote (vote1);
	ASSERT_EQ (2, votes1->votes.rep_votes.size ());
	auto existing1 (votes1->votes.rep_votes.find (paper::test_genesis_key.pub));
	ASSERT_NE (votes1->votes.rep_votes.end (), existing1);
	ASSERT_EQ (*send1, *existing1->second);
	paper::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	auto winner (node1.ledger.winner (transaction, votes1->votes));
	ASSERT_EQ (*send1, *winner.second);
	ASSERT_EQ (paper::genesis_amount - 100, winner.first);
}

TEST (votes, add_two)
{
	paper::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	paper::genesis genesis;
	paper::keypair key1;
	auto send1 (std::make_shared<paper::send_block> (genesis.hash (), key1.pub, paper::genesis_amount - 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0));
	{
		paper::transaction transaction (node1.store.environment, nullptr, true);
		ASSERT_EQ (paper::process_result::progress, node1.ledger.process (transaction, *send1).code);
	}
	auto node_l (system.nodes[0]);
	{
		paper::transaction transaction (node1.store.environment, nullptr, true);
		node1.active.start (transaction, send1);
	}
	auto votes1 (node1.active.roots.find (send1->root ())->election);
	auto vote1 (std::make_shared<paper::vote> (paper::test_genesis_key.pub, paper::test_genesis_key.prv, 1, send1));
	votes1->vote (vote1);
	paper::keypair key2;
	auto send2 (std::make_shared<paper::send_block> (genesis.hash (), key2.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0));
	auto vote2 (std::make_shared<paper::vote> (key2.pub, key2.prv, 1, send2));
	votes1->vote (vote2);
	ASSERT_EQ (3, votes1->votes.rep_votes.size ());
	ASSERT_NE (votes1->votes.rep_votes.end (), votes1->votes.rep_votes.find (paper::test_genesis_key.pub));
	ASSERT_EQ (*send1, *votes1->votes.rep_votes[paper::test_genesis_key.pub]);
	ASSERT_NE (votes1->votes.rep_votes.end (), votes1->votes.rep_votes.find (key2.pub));
	ASSERT_EQ (*send2, *votes1->votes.rep_votes[key2.pub]);
	paper::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	auto winner (node1.ledger.winner (transaction, votes1->votes));
	ASSERT_EQ (*send1, *winner.second);
}

// Higher sequence numbers change the vote
TEST (votes, add_existing)
{
	paper::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	paper::genesis genesis;
	paper::keypair key1;
	auto send1 (std::make_shared<paper::send_block> (genesis.hash (), key1.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0));
	{
		paper::transaction transaction (node1.store.environment, nullptr, true);
		ASSERT_EQ (paper::process_result::progress, node1.ledger.process (transaction, *send1).code);
	}
	auto node_l (system.nodes[0]);
	{
		paper::transaction transaction (node1.store.environment, nullptr, true);
		node1.active.start (transaction, send1);
	}
	auto votes1 (node1.active.roots.find (send1->root ())->election);
	auto vote1 (std::make_shared<paper::vote> (paper::test_genesis_key.pub, paper::test_genesis_key.prv, 1, send1));
	votes1->vote (vote1);
	paper::keypair key2;
	auto send2 (std::make_shared<paper::send_block> (genesis.hash (), key2.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0));
	auto vote2 (std::make_shared<paper::vote> (paper::test_genesis_key.pub, paper::test_genesis_key.prv, 2, send2));
	votes1->vote (vote2);
	ASSERT_EQ (2, votes1->votes.rep_votes.size ());
	ASSERT_NE (votes1->votes.rep_votes.end (), votes1->votes.rep_votes.find (paper::test_genesis_key.pub));
	ASSERT_EQ (*send2, *votes1->votes.rep_votes[paper::test_genesis_key.pub]);
	paper::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	auto winner (node1.ledger.winner (transaction, votes1->votes));
	ASSERT_EQ (*send2, *winner.second);
}

// Lower sequence numbers are ignored
TEST (votes, add_old)
{
	paper::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	paper::genesis genesis;
	paper::keypair key1;
	auto send1 (std::make_shared<paper::send_block> (genesis.hash (), key1.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0));
	{
		paper::transaction transaction (node1.store.environment, nullptr, true);
		ASSERT_EQ (paper::process_result::progress, node1.ledger.process (transaction, *send1).code);
	}
	auto node_l (system.nodes[0]);
	{
		paper::transaction transaction (node1.store.environment, nullptr, true);
		node1.active.start (transaction, send1);
	}
	auto votes1 (node1.active.roots.find (send1->root ())->election);
	auto vote1 (std::make_shared<paper::vote> (paper::test_genesis_key.pub, paper::test_genesis_key.prv, 2, send1));
	node1.vote_processor.vote (vote1, paper::endpoint ());
	paper::keypair key2;
	auto send2 (std::make_shared<paper::send_block> (genesis.hash (), key2.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0));
	auto vote2 (std::make_shared<paper::vote> (paper::test_genesis_key.pub, paper::test_genesis_key.prv, 1, send2));
	node1.vote_processor.vote (vote2, paper::endpoint ());
	ASSERT_EQ (2, votes1->votes.rep_votes.size ());
	ASSERT_NE (votes1->votes.rep_votes.end (), votes1->votes.rep_votes.find (paper::test_genesis_key.pub));
	ASSERT_EQ (*send1, *votes1->votes.rep_votes[paper::test_genesis_key.pub]);
	paper::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	auto winner (node1.ledger.winner (transaction, votes1->votes));
	ASSERT_EQ (*send1, *winner.second);
}

// Query for block successor
TEST (ledger, successor)
{
	paper::system system (24000, 1);
	paper::keypair key1;
	paper::genesis genesis;
	paper::send_block send1 (genesis.hash (), key1.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	paper::transaction transaction (system.nodes[0]->store.environment, nullptr, true);
	ASSERT_EQ (paper::process_result::progress, system.nodes[0]->ledger.process (transaction, send1).code);
	ASSERT_EQ (send1, *system.nodes[0]->ledger.successor (transaction, genesis.hash ()));
	ASSERT_EQ (*genesis.open, *system.nodes[0]->ledger.successor (transaction, genesis.open->root ()));
}

TEST (ledger, fail_change_old)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::change_block block (genesis.hash (), key1.pub, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result1 (ledger.process (transaction, block));
	ASSERT_EQ (paper::process_result::progress, result1.code);
	auto result2 (ledger.process (transaction, block));
	ASSERT_EQ (paper::process_result::old, result2.code);
}

TEST (ledger, fail_change_gap_previous)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::change_block block (1, key1.pub, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result1 (ledger.process (transaction, block));
	ASSERT_EQ (paper::process_result::gap_previous, result1.code);
}

TEST (ledger, fail_change_bad_signature)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::change_block block (genesis.hash (), key1.pub, paper::keypair ().prv, 0, 0);
	auto result1 (ledger.process (transaction, block));
	ASSERT_EQ (paper::process_result::bad_signature, result1.code);
}

TEST (ledger, fail_change_fork)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::change_block block1 (genesis.hash (), key1.pub, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result1 (ledger.process (transaction, block1));
	ASSERT_EQ (paper::process_result::progress, result1.code);
	paper::keypair key2;
	paper::change_block block2 (genesis.hash (), key2.pub, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result2 (ledger.process (transaction, block2));
	ASSERT_EQ (paper::process_result::fork, result2.code);
}

TEST (ledger, fail_send_old)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::send_block block (genesis.hash (), key1.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result1 (ledger.process (transaction, block));
	ASSERT_EQ (paper::process_result::progress, result1.code);
	auto result2 (ledger.process (transaction, block));
	ASSERT_EQ (paper::process_result::old, result2.code);
}

TEST (ledger, fail_send_gap_previous)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::send_block block (1, key1.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result1 (ledger.process (transaction, block));
	ASSERT_EQ (paper::process_result::gap_previous, result1.code);
}

TEST (ledger, fail_send_bad_signature)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::send_block block (genesis.hash (), key1.pub, 1, paper::keypair ().prv, 0, 0);
	auto result1 (ledger.process (transaction, block));
	ASSERT_EQ (paper::process_result::bad_signature, result1.code);
}

TEST (ledger, fail_send_negative_spend)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::send_block block1 (genesis.hash (), key1.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block1).code);
	paper::keypair key2;
	paper::send_block block2 (block1.hash (), key2.pub, 2, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::negative_spend, ledger.process (transaction, block2).code);
}

TEST (ledger, fail_send_fork)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::send_block block1 (genesis.hash (), key1.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block1).code);
	paper::keypair key2;
	paper::send_block block2 (genesis.hash (), key2.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::fork, ledger.process (transaction, block2).code);
}

TEST (ledger, fail_open_old)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::send_block block1 (genesis.hash (), key1.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block1).code);
	paper::open_block block2 (block1.hash (), 1, key1.pub, key1.prv, key1.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block2).code);
	ASSERT_EQ (paper::process_result::old, ledger.process (transaction, block2).code);
}

TEST (ledger, fail_open_gap_source)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::open_block block2 (1, 1, key1.pub, key1.prv, key1.pub, 0);
	auto result2 (ledger.process (transaction, block2));
	ASSERT_EQ (paper::process_result::gap_source, result2.code);
}

TEST (ledger, fail_open_bad_signature)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::send_block block1 (genesis.hash (), key1.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block1).code);
	paper::open_block block2 (block1.hash (), 1, key1.pub, key1.prv, key1.pub, 0);
	block2.signature.clear ();
	ASSERT_EQ (paper::process_result::bad_signature, ledger.process (transaction, block2).code);
}

TEST (ledger, fail_open_fork_previous)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::send_block block1 (genesis.hash (), key1.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block1).code);
	paper::send_block block2 (block1.hash (), key1.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block2).code);
	paper::open_block block3 (block1.hash (), 1, key1.pub, key1.prv, key1.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block3).code);
	paper::open_block block4 (block2.hash (), 1, key1.pub, key1.prv, key1.pub, 0);
	ASSERT_EQ (paper::process_result::fork, ledger.process (transaction, block4).code);
}

TEST (ledger, fail_open_account_mismatch)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::send_block block1 (genesis.hash (), key1.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block1).code);
	paper::keypair badkey;
	paper::open_block block2 (block1.hash (), 1, badkey.pub, badkey.prv, badkey.pub, 0);
	ASSERT_NE (paper::process_result::progress, ledger.process (transaction, block2).code);
}

TEST (ledger, fail_receive_old)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::send_block block1 (genesis.hash (), key1.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block1).code);
	paper::send_block block2 (block1.hash (), key1.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block2).code);
	paper::open_block block3 (block1.hash (), 1, key1.pub, key1.prv, key1.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block3).code);
	paper::receive_block block4 (block3.hash (), block2.hash (), key1.prv, key1.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block4).code);
	ASSERT_EQ (paper::process_result::old, ledger.process (transaction, block4).code);
}

TEST (ledger, fail_receive_gap_source)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::send_block block1 (genesis.hash (), key1.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result1 (ledger.process (transaction, block1));
	ASSERT_EQ (paper::process_result::progress, result1.code);
	paper::send_block block2 (block1.hash (), key1.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result2 (ledger.process (transaction, block2));
	ASSERT_EQ (paper::process_result::progress, result2.code);
	paper::open_block block3 (block1.hash (), 1, key1.pub, key1.prv, key1.pub, 0);
	auto result3 (ledger.process (transaction, block3));
	ASSERT_EQ (paper::process_result::progress, result3.code);
	paper::receive_block block4 (block3.hash (), 1, key1.prv, key1.pub, 0);
	auto result4 (ledger.process (transaction, block4));
	ASSERT_EQ (paper::process_result::gap_source, result4.code);
}

TEST (ledger, fail_receive_overreceive)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::send_block block1 (genesis.hash (), key1.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result1 (ledger.process (transaction, block1));
	ASSERT_EQ (paper::process_result::progress, result1.code);
	paper::open_block block2 (block1.hash (), 1, key1.pub, key1.prv, key1.pub, 0);
	auto result3 (ledger.process (transaction, block2));
	ASSERT_EQ (paper::process_result::progress, result3.code);
	paper::receive_block block3 (block2.hash (), block1.hash (), key1.prv, key1.pub, 0);
	auto result4 (ledger.process (transaction, block3));
	ASSERT_EQ (paper::process_result::unreceivable, result4.code);
}

TEST (ledger, fail_receive_bad_signature)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::send_block block1 (genesis.hash (), key1.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result1 (ledger.process (transaction, block1));
	ASSERT_EQ (paper::process_result::progress, result1.code);
	paper::send_block block2 (block1.hash (), key1.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result2 (ledger.process (transaction, block2));
	ASSERT_EQ (paper::process_result::progress, result2.code);
	paper::open_block block3 (block1.hash (), 1, key1.pub, key1.prv, key1.pub, 0);
	auto result3 (ledger.process (transaction, block3));
	ASSERT_EQ (paper::process_result::progress, result3.code);
	paper::receive_block block4 (block3.hash (), block2.hash (), paper::keypair ().prv, 0, 0);
	auto result4 (ledger.process (transaction, block4));
	ASSERT_EQ (paper::process_result::bad_signature, result4.code);
}

TEST (ledger, fail_receive_gap_previous_opened)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::send_block block1 (genesis.hash (), key1.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result1 (ledger.process (transaction, block1));
	ASSERT_EQ (paper::process_result::progress, result1.code);
	paper::send_block block2 (block1.hash (), key1.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result2 (ledger.process (transaction, block2));
	ASSERT_EQ (paper::process_result::progress, result2.code);
	paper::open_block block3 (block1.hash (), 1, key1.pub, key1.prv, key1.pub, 0);
	auto result3 (ledger.process (transaction, block3));
	ASSERT_EQ (paper::process_result::progress, result3.code);
	paper::receive_block block4 (1, block2.hash (), key1.prv, key1.pub, 0);
	auto result4 (ledger.process (transaction, block4));
	ASSERT_EQ (paper::process_result::gap_previous, result4.code);
}

TEST (ledger, fail_receive_gap_previous_unopened)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::send_block block1 (genesis.hash (), key1.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result1 (ledger.process (transaction, block1));
	ASSERT_EQ (paper::process_result::progress, result1.code);
	paper::send_block block2 (block1.hash (), key1.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result2 (ledger.process (transaction, block2));
	ASSERT_EQ (paper::process_result::progress, result2.code);
	paper::receive_block block3 (1, block2.hash (), key1.prv, key1.pub, 0);
	auto result3 (ledger.process (transaction, block3));
	ASSERT_EQ (paper::process_result::gap_previous, result3.code);
}

TEST (ledger, fail_receive_fork_previous)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::send_block block1 (genesis.hash (), key1.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result1 (ledger.process (transaction, block1));
	ASSERT_EQ (paper::process_result::progress, result1.code);
	paper::send_block block2 (block1.hash (), key1.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result2 (ledger.process (transaction, block2));
	ASSERT_EQ (paper::process_result::progress, result2.code);
	paper::open_block block3 (block1.hash (), 1, key1.pub, key1.prv, key1.pub, 0);
	auto result3 (ledger.process (transaction, block3));
	ASSERT_EQ (paper::process_result::progress, result3.code);
	paper::keypair key2;
	paper::send_block block4 (block3.hash (), key1.pub, 1, key1.prv, key1.pub, 0);
	auto result4 (ledger.process (transaction, block4));
	ASSERT_EQ (paper::process_result::progress, result4.code);
	paper::receive_block block5 (block3.hash (), block2.hash (), key1.prv, key1.pub, 0);
	auto result5 (ledger.process (transaction, block5));
	ASSERT_EQ (paper::process_result::fork, result5.code);
}

TEST (ledger, fail_receive_received_source)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key1;
	paper::send_block block1 (genesis.hash (), key1.pub, 2, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result1 (ledger.process (transaction, block1));
	ASSERT_EQ (paper::process_result::progress, result1.code);
	paper::send_block block2 (block1.hash (), key1.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result2 (ledger.process (transaction, block2));
	ASSERT_EQ (paper::process_result::progress, result2.code);
	paper::send_block block6 (block2.hash (), key1.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto result6 (ledger.process (transaction, block6));
	ASSERT_EQ (paper::process_result::progress, result6.code);
	paper::open_block block3 (block1.hash (), 1, key1.pub, key1.prv, key1.pub, 0);
	auto result3 (ledger.process (transaction, block3));
	ASSERT_EQ (paper::process_result::progress, result3.code);
	paper::keypair key2;
	paper::send_block block4 (block3.hash (), key1.pub, 1, key1.prv, key1.pub, 0);
	auto result4 (ledger.process (transaction, block4));
	ASSERT_EQ (paper::process_result::progress, result4.code);
	paper::receive_block block5 (block4.hash (), block2.hash (), key1.prv, key1.pub, 0);
	auto result5 (ledger.process (transaction, block5));
	ASSERT_EQ (paper::process_result::progress, result5.code);
	paper::receive_block block7 (block3.hash (), block2.hash (), key1.prv, key1.pub, 0);
	auto result7 (ledger.process (transaction, block7));
	ASSERT_EQ (paper::process_result::fork, result7.code);
}

TEST (ledger, latest_empty)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::keypair key;
	paper::transaction transaction (store.environment, nullptr, false);
	auto latest (ledger.latest (transaction, key.pub));
	ASSERT_TRUE (latest.is_zero ());
}

TEST (ledger, latest_root)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key;
	ASSERT_EQ (key.pub, ledger.latest_root (transaction, key.pub));
	auto hash1 (ledger.latest (transaction, paper::test_genesis_key.pub));
	paper::send_block send (hash1, 0, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, send).code);
	ASSERT_EQ (send.hash (), ledger.latest_root (transaction, paper::test_genesis_key.pub));
}

TEST (ledger, inactive_supply)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store, 40);
	{
		paper::transaction transaction (store.environment, nullptr, true);
		paper::genesis genesis;
		genesis.initialize (transaction, store);
		paper::keypair key2;
		paper::account_info info1;
		ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info1));
		paper::send_block send (info1.head, key2.pub, std::numeric_limits<paper::uint128_t>::max () - 50, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
		ledger.process (transaction, send);
	}
	paper::transaction transaction (store.environment, nullptr, false);
	ASSERT_EQ (10, ledger.supply (transaction));
	ledger.inactive_supply = 60;
	ASSERT_EQ (0, ledger.supply (transaction));
	ledger.inactive_supply = 0;
	ASSERT_EQ (50, ledger.supply (transaction));
}

TEST (ledger, change_representative_move_representation)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store);
	paper::keypair key1;
	paper::transaction transaction (store.environment, nullptr, true);
	paper::genesis genesis;
	genesis.initialize (transaction, store);
	auto hash1 (genesis.hash ());
	ASSERT_EQ (paper::genesis_amount, ledger.weight (transaction, paper::test_genesis_key.pub));
	paper::send_block send (hash1, key1.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, send).code);
	ASSERT_EQ (0, ledger.weight (transaction, paper::test_genesis_key.pub));
	paper::keypair key2;
	paper::change_block change (send.hash (), key2.pub, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, change).code);
	paper::keypair key3;
	paper::open_block open (send.hash (), key3.pub, key1.pub, key1.prv, key1.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, open).code);
	ASSERT_EQ (paper::genesis_amount, ledger.weight (transaction, key3.pub));
}

TEST (ledger, send_open_receive_rollback)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store, 0);
	paper::transaction transaction (store.environment, nullptr, true);
	paper::genesis genesis;
	genesis.initialize (transaction, store);
	paper::account_info info1;
	ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info1));
	paper::keypair key1;
	paper::send_block send1 (info1.head, key1.pub, paper::genesis_amount - 50, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto return1 (ledger.process (transaction, send1));
	ASSERT_EQ (paper::process_result::progress, return1.code);
	paper::send_block send2 (send1.hash (), key1.pub, paper::genesis_amount - 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto return2 (ledger.process (transaction, send2));
	ASSERT_EQ (paper::process_result::progress, return2.code);
	paper::keypair key2;
	paper::open_block open (send2.hash (), key2.pub, key1.pub, key1.prv, key1.pub, 0);
	auto return4 (ledger.process (transaction, open));
	ASSERT_EQ (paper::process_result::progress, return4.code);
	paper::receive_block receive (open.hash (), send1.hash (), key1.prv, key1.pub, 0);
	auto return5 (ledger.process (transaction, receive));
	ASSERT_EQ (paper::process_result::progress, return5.code);
	paper::keypair key3;
	ASSERT_EQ (100, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (paper::genesis_amount - 100, ledger.weight (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key3.pub));
	paper::change_block change1 (send2.hash (), key3.pub, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	auto return6 (ledger.process (transaction, change1));
	ASSERT_EQ (paper::process_result::progress, return6.code);
	ASSERT_EQ (100, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (0, ledger.weight (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (paper::genesis_amount - 100, ledger.weight (transaction, key3.pub));
	ledger.rollback (transaction, receive.hash ());
	ASSERT_EQ (50, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (0, ledger.weight (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (paper::genesis_amount - 100, ledger.weight (transaction, key3.pub));
	ledger.rollback (transaction, open.hash ());
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (0, ledger.weight (transaction, paper::test_genesis_key.pub));
	ASSERT_EQ (paper::genesis_amount - 100, ledger.weight (transaction, key3.pub));
	ledger.rollback (transaction, change1.hash ());
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key3.pub));
	ASSERT_EQ (paper::genesis_amount - 100, ledger.weight (transaction, paper::test_genesis_key.pub));
	ledger.rollback (transaction, send2.hash ());
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key3.pub));
	ASSERT_EQ (paper::genesis_amount - 50, ledger.weight (transaction, paper::test_genesis_key.pub));
	ledger.rollback (transaction, send1.hash ());
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key3.pub));
	ASSERT_EQ (paper::genesis_amount - 0, ledger.weight (transaction, paper::test_genesis_key.pub));
}

TEST (ledger, bootstrap_rep_weight)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::ledger ledger (store, 40);
	paper::account_info info1;
	paper::keypair key2;
	paper::genesis genesis;
	{
		paper::transaction transaction (store.environment, nullptr, true);
		genesis.initialize (transaction, store);
		ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info1));
		paper::send_block send (info1.head, key2.pub, std::numeric_limits<paper::uint128_t>::max () - 50, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
		ledger.process (transaction, send);
	}
	{
		paper::transaction transaction (store.environment, nullptr, false);
		ledger.bootstrap_weight_max_blocks = 3;
		ledger.bootstrap_weights[key2.pub] = 1000;
		ASSERT_EQ (1000, ledger.weight (transaction, key2.pub));
	}
	{
		paper::transaction transaction (store.environment, nullptr, true);
		ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info1));
		paper::send_block send (info1.head, key2.pub, std::numeric_limits<paper::uint128_t>::max () - 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
		ledger.process (transaction, send);
	}
	{
		paper::transaction transaction (store.environment, nullptr, false);
		ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	}
}
