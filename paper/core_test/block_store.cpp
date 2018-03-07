#include <gtest/gtest.h>
#include <paper/node/common.hpp>
#include <paper/node/node.hpp>
#include <paper/versioning.hpp>

#include <fstream>

TEST (block_store, construction)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	auto now (paper::seconds_since_epoch ());
	ASSERT_GT (now, 1408074640);
}

TEST (block_store, add_item)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::open_block block (0, 1, 0, paper::keypair ().prv, 0, 0);
	paper::uint256_union hash1 (block.hash ());
	paper::transaction transaction (store.environment, nullptr, true);
	auto latest1 (store.block_get (transaction, hash1));
	ASSERT_EQ (nullptr, latest1);
	ASSERT_FALSE (store.block_exists (transaction, hash1));
	store.block_put (transaction, hash1, block);
	auto latest2 (store.block_get (transaction, hash1));
	ASSERT_NE (nullptr, latest2);
	ASSERT_EQ (block, *latest2);
	ASSERT_TRUE (store.block_exists (transaction, hash1));
	ASSERT_FALSE (store.block_exists (transaction, hash1.number () - 1));
	store.block_del (transaction, hash1);
	auto latest3 (store.block_get (transaction, hash1));
	ASSERT_EQ (nullptr, latest3);
}

TEST (block_store, add_nonempty_block)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::keypair key1;
	paper::open_block block (0, 1, 0, paper::keypair ().prv, 0, 0);
	paper::uint256_union hash1 (block.hash ());
	block.signature = paper::sign_message (key1.prv, key1.pub, hash1);
	paper::transaction transaction (store.environment, nullptr, true);
	auto latest1 (store.block_get (transaction, hash1));
	ASSERT_EQ (nullptr, latest1);
	store.block_put (transaction, hash1, block);
	auto latest2 (store.block_get (transaction, hash1));
	ASSERT_NE (nullptr, latest2);
	ASSERT_EQ (block, *latest2);
}

TEST (block_store, add_two_items)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::keypair key1;
	paper::open_block block (0, 1, 1, paper::keypair ().prv, 0, 0);
	paper::uint256_union hash1 (block.hash ());
	block.signature = paper::sign_message (key1.prv, key1.pub, hash1);
	paper::transaction transaction (store.environment, nullptr, true);
	auto latest1 (store.block_get (transaction, hash1));
	ASSERT_EQ (nullptr, latest1);
	paper::open_block block2 (0, 1, 3, paper::keypair ().prv, 0, 0);
	block2.hashables.account = 3;
	paper::uint256_union hash2 (block2.hash ());
	block2.signature = paper::sign_message (key1.prv, key1.pub, hash2);
	auto latest2 (store.block_get (transaction, hash2));
	ASSERT_EQ (nullptr, latest2);
	store.block_put (transaction, hash1, block);
	store.block_put (transaction, hash2, block2);
	auto latest3 (store.block_get (transaction, hash1));
	ASSERT_NE (nullptr, latest3);
	ASSERT_EQ (block, *latest3);
	auto latest4 (store.block_get (transaction, hash2));
	ASSERT_NE (nullptr, latest4);
	ASSERT_EQ (block2, *latest4);
	ASSERT_FALSE (*latest3 == *latest4);
}

TEST (block_store, add_receive)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::keypair key1;
	paper::keypair key2;
	paper::open_block block1 (0, 1, 0, paper::keypair ().prv, 0, 0);
	paper::transaction transaction (store.environment, nullptr, true);
	store.block_put (transaction, block1.hash (), block1);
	paper::receive_block block (block1.hash (), 1, paper::keypair ().prv, 2, 3);
	paper::block_hash hash1 (block.hash ());
	auto latest1 (store.block_get (transaction, hash1));
	ASSERT_EQ (nullptr, latest1);
	store.block_put (transaction, hash1, block);
	auto latest2 (store.block_get (transaction, hash1));
	ASSERT_NE (nullptr, latest2);
	ASSERT_EQ (block, *latest2);
}

TEST (block_store, add_pending)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::keypair key1;
	paper::pending_key key2 (0, 0);
	paper::pending_info pending1;
	paper::transaction transaction (store.environment, nullptr, true);
	ASSERT_TRUE (store.pending_get (transaction, key2, pending1));
	store.pending_put (transaction, key2, pending1);
	paper::pending_info pending2;
	ASSERT_FALSE (store.pending_get (transaction, key2, pending2));
	ASSERT_EQ (pending1, pending2);
	store.pending_del (transaction, key2);
	ASSERT_TRUE (store.pending_get (transaction, key2, pending2));
}

TEST (block_store, pending_iterator)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::transaction transaction (store.environment, nullptr, true);
	ASSERT_EQ (store.pending_end (), store.pending_begin (transaction));
	store.pending_put (transaction, paper::pending_key (1, 2), { 2, 3 });
	auto current (store.pending_begin (transaction));
	ASSERT_NE (store.pending_end (), current);
	paper::pending_key key1 (current->first);
	ASSERT_EQ (paper::account (1), key1.account);
	ASSERT_EQ (paper::block_hash (2), key1.hash);
	paper::pending_info pending (current->second);
	ASSERT_EQ (paper::account (2), pending.source);
	ASSERT_EQ (paper::amount (3), pending.amount);
}

TEST (block_store, genesis)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::genesis genesis;
	auto hash (genesis.hash ());
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::account_info info;
	ASSERT_FALSE (store.account_get (transaction, paper::genesis_account, info));
	ASSERT_EQ (hash, info.head);
	auto block1 (store.block_get (transaction, info.head));
	ASSERT_NE (nullptr, block1);
	auto receive1 (dynamic_cast<paper::open_block *> (block1.get ()));
	ASSERT_NE (nullptr, receive1);
	ASSERT_LE (info.modified, paper::seconds_since_epoch ());
	auto test_pub_text (paper::test_genesis_key.pub.to_string ());
	auto test_pub_account (paper::test_genesis_key.pub.to_account ());
	auto test_prv_text (paper::test_genesis_key.prv.data.to_string ());
	ASSERT_EQ (paper::genesis_account, paper::test_genesis_key.pub);
}

TEST (representation, changes)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::keypair key1;
	paper::transaction transaction (store.environment, nullptr, true);
	ASSERT_EQ (0, store.representation_get (transaction, key1.pub));
	store.representation_put (transaction, key1.pub, 1);
	ASSERT_EQ (1, store.representation_get (transaction, key1.pub));
	store.representation_put (transaction, key1.pub, 2);
	ASSERT_EQ (2, store.representation_get (transaction, key1.pub));
}

TEST (bootstrap, simple)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	auto block1 (std::make_shared<paper::send_block> (0, 1, 2, paper::keypair ().prv, 4, 5));
	paper::transaction transaction (store.environment, nullptr, true);
	auto block2 (store.unchecked_get (transaction, block1->previous ()));
	ASSERT_TRUE (block2.empty ());
	store.unchecked_put (transaction, block1->previous (), block1);
	auto block3 (store.unchecked_get (transaction, block1->previous ()));
	ASSERT_FALSE (block3.empty ());
	ASSERT_EQ (*block1, *block3[0]);
	store.unchecked_del (transaction, block1->previous (), *block1);
	auto block4 (store.unchecked_get (transaction, block1->previous ()));
	ASSERT_TRUE (block4.empty ());
}

TEST (unchecked, multiple)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	auto block1 (std::make_shared<paper::send_block> (4, 1, 2, paper::keypair ().prv, 4, 5));
	paper::transaction transaction (store.environment, nullptr, true);
	auto block2 (store.unchecked_get (transaction, block1->previous ()));
	ASSERT_TRUE (block2.empty ());
	store.unchecked_put (transaction, block1->previous (), block1);
	store.unchecked_put (transaction, block1->source (), block1);
	auto block3 (store.unchecked_get (transaction, block1->previous ()));
	ASSERT_FALSE (block3.empty ());
	auto block4 (store.unchecked_get (transaction, block1->source ()));
	ASSERT_FALSE (block4.empty ());
}

TEST (unchecked, double_put)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	auto block1 (std::make_shared<paper::send_block> (4, 1, 2, paper::keypair ().prv, 4, 5));
	paper::transaction transaction (store.environment, nullptr, true);
	auto block2 (store.unchecked_get (transaction, block1->previous ()));
	ASSERT_TRUE (block2.empty ());
	store.unchecked_put (transaction, block1->previous (), block1);
	store.unchecked_put (transaction, block1->previous (), block1);
	auto block3 (store.unchecked_get (transaction, block1->previous ()));
	ASSERT_EQ (block3.size (), 1);
}

TEST (checksum, simple)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::block_hash hash0 (0);
	paper::transaction transaction (store.environment, nullptr, true);
	ASSERT_TRUE (store.checksum_get (transaction, 0x100, 0x10, hash0));
	paper::block_hash hash1 (0);
	store.checksum_put (transaction, 0x100, 0x10, hash1);
	paper::block_hash hash2;
	ASSERT_FALSE (store.checksum_get (transaction, 0x100, 0x10, hash2));
	ASSERT_EQ (hash1, hash2);
	store.checksum_del (transaction, 0x100, 0x10);
	paper::block_hash hash3;
	ASSERT_TRUE (store.checksum_get (transaction, 0x100, 0x10, hash3));
}

TEST (block_store, empty_accounts)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::transaction transaction (store.environment, nullptr, false);
	auto begin (store.latest_begin (transaction));
	auto end (store.latest_end ());
	ASSERT_EQ (end, begin);
}

TEST (block_store, one_block)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::open_block block1 (0, 1, 0, paper::keypair ().prv, 0, 0);
	paper::transaction transaction (store.environment, nullptr, true);
	store.block_put (transaction, block1.hash (), block1);
	ASSERT_TRUE (store.block_exists (transaction, block1.hash ()));
}

TEST (block_store, empty_bootstrap)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::transaction transaction (store.environment, nullptr, false);
	auto begin (store.unchecked_begin (transaction));
	auto end (store.unchecked_end ());
	ASSERT_EQ (end, begin);
}

TEST (block_store, one_bootstrap)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	auto block1 (std::make_shared<paper::send_block> (0, 1, 2, paper::keypair ().prv, 4, 5));
	paper::transaction transaction (store.environment, nullptr, true);
	store.unchecked_put (transaction, block1->hash (), block1);
	store.flush (transaction);
	auto begin (store.unchecked_begin (transaction));
	auto end (store.unchecked_end ());
	ASSERT_NE (end, begin);
	auto hash1 (begin->first.uint256 ());
	ASSERT_EQ (block1->hash (), hash1);
	auto block2 (paper::deserialize_block (begin->second));
	ASSERT_EQ (*block1, *block2);
	++begin;
	ASSERT_EQ (end, begin);
}

TEST (block_store, unchecked_begin_search)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::keypair key0;
	paper::send_block block1 (0, 1, 2, key0.prv, key0.pub, 3);
	paper::send_block block2 (5, 6, 7, key0.prv, key0.pub, 8);
}

TEST (block_store, frontier_retrieval)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::account account1 (0);
	paper::account_info info1 (0, 0, 0, 0, 0, 0);
	paper::transaction transaction (store.environment, nullptr, true);
	store.account_put (transaction, account1, info1);
	paper::account_info info2;
	store.account_get (transaction, account1, info2);
	ASSERT_EQ (info1, info2);
}

TEST (block_store, one_account)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::account account (0);
	paper::block_hash hash (0);
	paper::transaction transaction (store.environment, nullptr, true);
	store.account_put (transaction, account, { hash, account, hash, 42, 100, 200 });
	auto begin (store.latest_begin (transaction));
	auto end (store.latest_end ());
	ASSERT_NE (end, begin);
	ASSERT_EQ (account, begin->first.uint256 ());
	paper::account_info info (begin->second);
	ASSERT_EQ (hash, info.head);
	ASSERT_EQ (42, info.balance.number ());
	ASSERT_EQ (100, info.modified);
	ASSERT_EQ (200, info.block_count);
	++begin;
	ASSERT_EQ (end, begin);
}

TEST (block_store, two_block)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::open_block block1 (0, 1, 1, paper::keypair ().prv, 0, 0);
	block1.hashables.account = 1;
	std::vector<paper::block_hash> hashes;
	std::vector<paper::open_block> blocks;
	hashes.push_back (block1.hash ());
	blocks.push_back (block1);
	paper::transaction transaction (store.environment, nullptr, true);
	store.block_put (transaction, hashes[0], block1);
	paper::open_block block2 (0, 1, 2, paper::keypair ().prv, 0, 0);
	hashes.push_back (block2.hash ());
	blocks.push_back (block2);
	store.block_put (transaction, hashes[1], block2);
	ASSERT_TRUE (store.block_exists (transaction, block1.hash ()));
	ASSERT_TRUE (store.block_exists (transaction, block2.hash ()));
}

TEST (block_store, two_account)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::account account1 (1);
	paper::block_hash hash1 (2);
	paper::account account2 (3);
	paper::block_hash hash2 (4);
	paper::transaction transaction (store.environment, nullptr, true);
	store.account_put (transaction, account1, { hash1, account1, hash1, 42, 100, 300 });
	store.account_put (transaction, account2, { hash2, account2, hash2, 84, 200, 400 });
	auto begin (store.latest_begin (transaction));
	auto end (store.latest_end ());
	ASSERT_NE (end, begin);
	ASSERT_EQ (account1, begin->first.uint256 ());
	paper::account_info info1 (begin->second);
	ASSERT_EQ (hash1, info1.head);
	ASSERT_EQ (42, info1.balance.number ());
	ASSERT_EQ (100, info1.modified);
	ASSERT_EQ (300, info1.block_count);
	++begin;
	ASSERT_NE (end, begin);
	ASSERT_EQ (account2, begin->first.uint256 ());
	paper::account_info info2 (begin->second);
	ASSERT_EQ (hash2, info2.head);
	ASSERT_EQ (84, info2.balance.number ());
	ASSERT_EQ (200, info2.modified);
	ASSERT_EQ (400, info2.block_count);
	++begin;
	ASSERT_EQ (end, begin);
}

TEST (block_store, latest_find)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::account account1 (1);
	paper::block_hash hash1 (2);
	paper::account account2 (3);
	paper::block_hash hash2 (4);
	paper::transaction transaction (store.environment, nullptr, true);
	store.account_put (transaction, account1, { hash1, account1, hash1, 100, 0, 300 });
	store.account_put (transaction, account2, { hash2, account2, hash2, 200, 0, 400 });
	auto first (store.latest_begin (transaction));
	auto second (store.latest_begin (transaction));
	++second;
	auto find1 (store.latest_begin (transaction, 1));
	ASSERT_EQ (first, find1);
	auto find2 (store.latest_begin (transaction, 3));
	ASSERT_EQ (second, find2);
	auto find3 (store.latest_begin (transaction, 2));
	ASSERT_EQ (second, find3);
}

TEST (block_store, bad_path)
{
	bool init (false);
	paper::block_store store (init, boost::filesystem::path ("///"));
	ASSERT_TRUE (init);
}

TEST (block_store, DISABLED_already_open) // File can be shared
{
	auto path (paper::unique_path ());
	boost::filesystem::create_directories (path.parent_path ());
	std::ofstream file;
	file.open (path.string ().c_str ());
	ASSERT_TRUE (file.is_open ());
	bool init (false);
	paper::block_store store (init, path);
	ASSERT_TRUE (init);
}

TEST (block_store, roots)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::send_block send_block (0, 1, 2, paper::keypair ().prv, 4, 5);
	ASSERT_EQ (send_block.hashables.previous, send_block.root ());
	paper::change_block change_block (0, 1, paper::keypair ().prv, 3, 4);
	ASSERT_EQ (change_block.hashables.previous, change_block.root ());
	paper::receive_block receive_block (0, 1, paper::keypair ().prv, 3, 4);
	ASSERT_EQ (receive_block.hashables.previous, receive_block.root ());
	paper::open_block open_block (0, 1, 2, paper::keypair ().prv, 4, 5);
	ASSERT_EQ (open_block.hashables.account, open_block.root ());
}

TEST (block_store, pending_exists)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::pending_key two (2, 0);
	paper::pending_info pending;
	paper::transaction transaction (store.environment, nullptr, true);
	store.pending_put (transaction, two, pending);
	paper::pending_key one (1, 0);
	ASSERT_FALSE (store.pending_exists (transaction, one));
}

TEST (block_store, latest_exists)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::block_hash two (2);
	paper::account_info info;
	paper::transaction transaction (store.environment, nullptr, true);
	store.account_put (transaction, two, info);
	paper::block_hash one (1);
	ASSERT_FALSE (store.account_exists (transaction, one));
}

TEST (block_store, unsynced)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::transaction transaction (store.environment, nullptr, true);
	ASSERT_EQ (store.unsynced_end (), store.unsynced_begin (transaction));
	paper::block_hash hash1 (0);
	ASSERT_FALSE (store.unsynced_exists (transaction, hash1));
	store.unsynced_put (transaction, hash1);
	ASSERT_TRUE (store.unsynced_exists (transaction, hash1));
	ASSERT_NE (store.unsynced_end (), store.unsynced_begin (transaction));
	ASSERT_EQ (hash1, paper::uint256_union (store.unsynced_begin (transaction)->first.uint256 ()));
	store.unsynced_del (transaction, hash1);
	ASSERT_FALSE (store.unsynced_exists (transaction, hash1));
	ASSERT_EQ (store.unsynced_end (), store.unsynced_begin (transaction));
}

TEST (block_store, unsynced_iteration)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::transaction transaction (store.environment, nullptr, true);
	ASSERT_EQ (store.unsynced_end (), store.unsynced_begin (transaction));
	paper::block_hash hash1 (1);
	store.unsynced_put (transaction, hash1);
	paper::block_hash hash2 (2);
	store.unsynced_put (transaction, hash2);
	std::unordered_set<paper::block_hash> hashes;
	for (auto i (store.unsynced_begin (transaction)), n (store.unsynced_end ()); i != n; ++i)
	{
		hashes.insert (paper::uint256_union (i->first.uint256 ()));
	}
	ASSERT_EQ (2, hashes.size ());
	ASSERT_TRUE (hashes.find (hash1) != hashes.end ());
	ASSERT_TRUE (hashes.find (hash2) != hashes.end ());
}

TEST (block_store, large_iteration)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	std::unordered_set<paper::account> accounts1;
	for (auto i (0); i < 1000; ++i)
	{
		paper::transaction transaction (store.environment, nullptr, true);
		paper::account account;
		paper::random_pool.GenerateBlock (account.bytes.data (), account.bytes.size ());
		accounts1.insert (account);
		store.account_put (transaction, account, paper::account_info ());
	}
	std::unordered_set<paper::account> accounts2;
	paper::account previous (0);
	paper::transaction transaction (store.environment, nullptr, false);
	for (auto i (store.latest_begin (transaction, 0)), n (store.latest_end ()); i != n; ++i)
	{
		paper::account current (i->first.uint256 ());
		assert (current.number () > previous.number ());
		accounts2.insert (current);
		previous = current;
	}
	ASSERT_EQ (accounts1, accounts2);
}

TEST (block_store, frontier)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::transaction transaction (store.environment, nullptr, true);
	paper::block_hash hash (100);
	paper::account account (200);
	ASSERT_TRUE (store.frontier_get (transaction, hash).is_zero ());
	store.frontier_put (transaction, hash, account);
	ASSERT_EQ (account, store.frontier_get (transaction, hash));
	store.frontier_del (transaction, hash);
	ASSERT_TRUE (store.frontier_get (transaction, hash).is_zero ());
}

TEST (block_store, block_replace)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::send_block send1 (0, 0, 0, paper::keypair ().prv, 0, 1);
	paper::send_block send2 (0, 0, 0, paper::keypair ().prv, 0, 2);
	paper::transaction transaction (store.environment, nullptr, true);
	store.block_put (transaction, 0, send1);
	store.block_put (transaction, 0, send2);
	auto block3 (store.block_get (transaction, 0));
	ASSERT_NE (nullptr, block3);
	ASSERT_EQ (2, block3->block_work ());
}

TEST (block_store, block_count)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	ASSERT_EQ (0, store.block_count (paper::transaction (store.environment, nullptr, false)).sum ());
	paper::open_block block (0, 1, 0, paper::keypair ().prv, 0, 0);
	paper::uint256_union hash1 (block.hash ());
	store.block_put (paper::transaction (store.environment, nullptr, true), hash1, block);
	ASSERT_EQ (1, store.block_count (paper::transaction (store.environment, nullptr, false)).sum ());
}

TEST (block_store, frontier_count)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	ASSERT_EQ (0, store.frontier_count (paper::transaction (store.environment, nullptr, false)));
	paper::block_hash hash (100);
	paper::account account (200);
	store.frontier_put (paper::transaction (store.environment, nullptr, true), hash, account);
	ASSERT_EQ (1, store.frontier_count (paper::transaction (store.environment, nullptr, false)));
}

TEST (block_store, sequence_increment)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::keypair key1;
	paper::keypair key2;
	auto block1 (std::make_shared<paper::open_block> (0, 1, 0, paper::keypair ().prv, 0, 0));
	paper::transaction transaction (store.environment, nullptr, true);
	auto vote1 (store.vote_generate (transaction, key1.pub, key1.prv, block1));
	ASSERT_EQ (1, vote1->sequence);
	auto vote2 (store.vote_generate (transaction, key1.pub, key1.prv, block1));
	ASSERT_EQ (2, vote2->sequence);
	auto vote3 (store.vote_generate (transaction, key2.pub, key2.prv, block1));
	ASSERT_EQ (1, vote3->sequence);
	auto vote4 (store.vote_generate (transaction, key2.pub, key2.prv, block1));
	ASSERT_EQ (2, vote4->sequence);
	vote1->sequence = 20;
	auto seq5 (store.vote_max (transaction, vote1));
	ASSERT_EQ (20, seq5->sequence);
	vote3->sequence = 30;
	auto seq6 (store.vote_max (transaction, vote3));
	ASSERT_EQ (30, seq6->sequence);
	auto vote5 (store.vote_generate (transaction, key1.pub, key1.prv, block1));
	ASSERT_EQ (21, vote5->sequence);
	auto vote6 (store.vote_generate (transaction, key2.pub, key2.prv, block1));
	ASSERT_EQ (31, vote6->sequence);
}

TEST (block_store, upgrade_v2_v3)
{
	paper::keypair key1;
	paper::keypair key2;
	paper::block_hash change_hash;
	auto path (paper::unique_path ());
	{
		bool init (false);
		paper::block_store store (init, path);
		ASSERT_TRUE (!init);
		paper::transaction transaction (store.environment, nullptr, true);
		paper::genesis genesis;
		auto hash (genesis.hash ());
		genesis.initialize (transaction, store);
		paper::ledger ledger (store);
		paper::change_block change (hash, key1.pub, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
		change_hash = change.hash ();
		ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, change).code);
		ASSERT_EQ (0, ledger.weight (transaction, paper::test_genesis_key.pub));
		ASSERT_EQ (paper::genesis_amount, ledger.weight (transaction, key1.pub));
		store.version_put (transaction, 2);
		store.representation_put (transaction, key1.pub, 7);
		ASSERT_EQ (7, ledger.weight (transaction, key1.pub));
		ASSERT_EQ (2, store.version_get (transaction));
		store.representation_put (transaction, key2.pub, 6);
		ASSERT_EQ (6, ledger.weight (transaction, key2.pub));
		paper::account_info info;
		ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info));
		info.rep_block = 42;
		paper::account_info_v5 info_old (info.head, info.rep_block, info.open_block, info.balance, info.modified);
		auto status (mdb_put (transaction, store.accounts, paper::mdb_val (paper::test_genesis_key.pub), info_old.val (), 0));
		assert (status == 0);
	}
	bool init (false);
	paper::block_store store (init, path);
	paper::ledger ledger (store);
	paper::transaction transaction (store.environment, nullptr, true);
	ASSERT_TRUE (!init);
	ASSERT_LT (2, store.version_get (transaction));
	ASSERT_EQ (paper::genesis_amount, ledger.weight (transaction, key1.pub));
	ASSERT_EQ (0, ledger.weight (transaction, key2.pub));
	paper::account_info info;
	ASSERT_FALSE (store.account_get (transaction, paper::test_genesis_key.pub, info));
	ASSERT_EQ (change_hash, info.rep_block);
}

TEST (block_store, upgrade_v3_v4)
{
	paper::keypair key1;
	paper::keypair key2;
	paper::keypair key3;
	auto path (paper::unique_path ());
	{
		bool init (false);
		paper::block_store store (init, path);
		ASSERT_FALSE (init);
		paper::transaction transaction (store.environment, nullptr, true);
		store.version_put (transaction, 3);
		paper::pending_info_v3 info (key1.pub, 100, key2.pub);
		auto status (mdb_put (transaction, store.pending, paper::mdb_val (key3.pub), info.val (), 0));
		ASSERT_EQ (0, status);
	}
	bool init (false);
	paper::block_store store (init, path);
	paper::ledger ledger (store);
	paper::transaction transaction (store.environment, nullptr, true);
	ASSERT_FALSE (init);
	ASSERT_LT (3, store.version_get (transaction));
	paper::pending_key key (key2.pub, key3.pub);
	paper::pending_info info;
	auto error (store.pending_get (transaction, key, info));
	ASSERT_FALSE (error);
	ASSERT_EQ (key1.pub, info.source);
	ASSERT_EQ (paper::amount (100), info.amount);
}

TEST (block_store, upgrade_v4_v5)
{
	paper::block_hash genesis_hash (0);
	paper::block_hash hash (0);
	auto path (paper::unique_path ());
	{
		bool init (false);
		paper::block_store store (init, path);
		ASSERT_FALSE (init);
		paper::transaction transaction (store.environment, nullptr, true);
		paper::genesis genesis;
		genesis.initialize (transaction, store);
		paper::ledger ledger (store);
		store.version_put (transaction, 4);
		paper::account_info info;
		store.account_get (transaction, paper::test_genesis_key.pub, info);
		paper::keypair key0;
		paper::send_block block0 (info.head, key0.pub, paper::genesis_amount - paper::Gppr_ratio, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
		ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block0).code);
		hash = block0.hash ();
		auto original (store.block_get (transaction, info.head));
		genesis_hash = info.head;
		store.block_successor_clear (transaction, info.head);
		ASSERT_TRUE (store.block_successor (transaction, genesis_hash).is_zero ());
		paper::account_info info2;
		store.account_get (transaction, paper::test_genesis_key.pub, info2);
		paper::account_info_v5 info_old (info2.head, info2.rep_block, info2.open_block, info2.balance, info2.modified);
		auto status (mdb_put (transaction, store.accounts, paper::mdb_val (paper::test_genesis_key.pub), info_old.val (), 0));
		assert (status == 0);
	}
	bool init (false);
	paper::block_store store (init, path);
	ASSERT_FALSE (init);
	paper::transaction transaction (store.environment, nullptr, false);
	ASSERT_EQ (hash, store.block_successor (transaction, genesis_hash));
}

TEST (block_store, block_random)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	auto block (store.block_random (transaction));
	ASSERT_NE (nullptr, block);
	ASSERT_EQ (*block, *genesis.open);
}

TEST (vote, validate)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_TRUE (!init);
	paper::keypair key1;
	auto send1 (std::make_shared<paper::send_block> (0, key1.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0));
	auto vote1 (std::make_shared<paper::vote> (key1.pub, key1.prv, 2, send1));
	paper::transaction transaction (store.environment, nullptr, true);
	auto vote_result1 (store.vote_validate (transaction, vote1));
	ASSERT_EQ (paper::vote_code::vote, vote_result1.code);
	ASSERT_EQ (*vote1, *vote_result1.vote);
	vote1->signature.bytes[8] ^= 1;
	auto vote_result2 (store.vote_validate (transaction, vote1));
	ASSERT_EQ (paper::vote_code::invalid, vote_result2.code);
	// If the signature is invalid, we don't need to take the overhead of checking the current sequence value
	ASSERT_EQ (nullptr, vote_result2.vote);
	auto vote2 (std::make_shared<paper::vote> (key1.pub, key1.prv, 1, send1));
	auto vote_result3 (store.vote_validate (transaction, vote2));
	ASSERT_EQ (paper::vote_code::replay, vote_result3.code);
	ASSERT_EQ (*vote1, *vote_result3.vote);
}

TEST (block_store, upgrade_v5_v6)
{
	auto path (paper::unique_path ());
	{
		bool init (false);
		paper::block_store store (init, path);
		ASSERT_FALSE (init);
		paper::transaction transaction (store.environment, nullptr, true);
		paper::genesis genesis;
		genesis.initialize (transaction, store);
		store.version_put (transaction, 5);
		paper::account_info info;
		store.account_get (transaction, paper::test_genesis_key.pub, info);
		paper::account_info_v5 info_old (info.head, info.rep_block, info.open_block, info.balance, info.modified);
		auto status (mdb_put (transaction, store.accounts, paper::mdb_val (paper::test_genesis_key.pub), info_old.val (), 0));
		assert (status == 0);
	}
	bool init (false);
	paper::block_store store (init, path);
	ASSERT_FALSE (init);
	paper::transaction transaction (store.environment, nullptr, false);
	paper::account_info info;
	store.account_get (transaction, paper::test_genesis_key.pub, info);
	ASSERT_EQ (1, info.block_count);
}

TEST (block_store, upgrade_v6_v7)
{
	auto path (paper::unique_path ());
	{
		bool init (false);
		paper::block_store store (init, path);
		ASSERT_FALSE (init);
		paper::transaction transaction (store.environment, nullptr, true);
		paper::genesis genesis;
		genesis.initialize (transaction, store);
		store.version_put (transaction, 6);
		auto send1 (std::make_shared<paper::send_block> (0, 0, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0));
		store.unchecked_put (transaction, send1->hash (), send1);
		store.flush (transaction);
		ASSERT_NE (store.unchecked_end (), store.unchecked_begin (transaction));
	}
	bool init (false);
	paper::block_store store (init, path);
	ASSERT_FALSE (init);
	paper::transaction transaction (store.environment, nullptr, false);
	ASSERT_EQ (store.unchecked_end (), store.unchecked_begin (transaction));
}

// Databases need to be dropped in order to convert to dupsort compatible
TEST (block_store, change_dupsort)
{
	auto path (paper::unique_path ());
	bool init (false);
	paper::block_store store (init, path);
	paper::transaction transaction (store.environment, nullptr, true);
	ASSERT_EQ (0, mdb_drop (transaction, store.unchecked, 1));
	ASSERT_EQ (0, mdb_dbi_open (transaction, "unchecked", MDB_CREATE, &store.unchecked));
	auto send1 (std::make_shared<paper::send_block> (0, 0, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0));
	auto send2 (std::make_shared<paper::send_block> (1, 0, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0));
	ASSERT_NE (send1->hash (), send2->hash ());
	store.unchecked_put (transaction, send1->hash (), send1);
	store.unchecked_put (transaction, send1->hash (), send2);
	store.flush (transaction);
	{
		auto iterator1 (store.unchecked_begin (transaction));
		++iterator1;
		ASSERT_EQ (store.unchecked_end (), iterator1);
	}
	ASSERT_EQ (0, mdb_drop (transaction, store.unchecked, 0));
	mdb_dbi_close (store.environment, store.unchecked);
	ASSERT_EQ (0, mdb_dbi_open (transaction, "unchecked", MDB_CREATE | MDB_DUPSORT, &store.unchecked));
	store.unchecked_put (transaction, send1->hash (), send1);
	store.unchecked_put (transaction, send1->hash (), send2);
	store.flush (transaction);
	{
		auto iterator1 (store.unchecked_begin (transaction));
		++iterator1;
		ASSERT_EQ (store.unchecked_end (), iterator1);
	}
	ASSERT_EQ (0, mdb_drop (transaction, store.unchecked, 1));
	ASSERT_EQ (0, mdb_dbi_open (transaction, "unchecked", MDB_CREATE | MDB_DUPSORT, &store.unchecked));
	store.unchecked_put (transaction, send1->hash (), send1);
	store.unchecked_put (transaction, send1->hash (), send2);
	store.flush (transaction);
	{
		auto iterator1 (store.unchecked_begin (transaction));
		++iterator1;
		ASSERT_NE (store.unchecked_end (), iterator1);
		++iterator1;
		ASSERT_EQ (store.unchecked_end (), iterator1);
	}
}

TEST (block_store, upgrade_v7_v8)
{
	auto path (paper::unique_path ());
	{
		bool init (false);
		paper::block_store store (init, path);
		paper::transaction transaction (store.environment, nullptr, true);
		ASSERT_EQ (0, mdb_drop (transaction, store.unchecked, 1));
		ASSERT_EQ (0, mdb_dbi_open (transaction, "unchecked", MDB_CREATE, &store.unchecked));
		store.version_put (transaction, 7);
	}
	bool init (false);
	paper::block_store store (init, path);
	ASSERT_FALSE (init);
	paper::transaction transaction (store.environment, nullptr, true);
	auto send1 (std::make_shared<paper::send_block> (0, 0, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0));
	auto send2 (std::make_shared<paper::send_block> (1, 0, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0));
	store.unchecked_put (transaction, send1->hash (), send1);
	store.unchecked_put (transaction, send1->hash (), send2);
	store.flush (transaction);
	{
		auto iterator1 (store.unchecked_begin (transaction));
		++iterator1;
		ASSERT_NE (store.unchecked_end (), iterator1);
		++iterator1;
		ASSERT_EQ (store.unchecked_end (), iterator1);
	}
}

TEST (block_store, sequence_flush)
{
	auto path (paper::unique_path ());
	bool init (false);
	paper::block_store store (init, path);
	ASSERT_FALSE (init);
	paper::transaction transaction (store.environment, nullptr, true);
	paper::keypair key1;
	auto send1 (std::make_shared<paper::send_block> (0, 0, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0));
	auto vote1 (store.vote_generate (transaction, key1.pub, key1.prv, send1));
	auto seq2 (store.vote_get (transaction, vote1->account));
	ASSERT_EQ (nullptr, seq2);
	store.flush (transaction);
	auto seq3 (store.vote_get (transaction, vote1->account));
	ASSERT_EQ (*seq3, *vote1);
}

// Upgrading tracking block sequence numbers to whole vote.
TEST (block_store, upgrade_v8_v9)
{
	auto path (paper::unique_path ());
	paper::keypair key;
	{
		bool init (false);
		paper::block_store store (init, path);
		paper::transaction transaction (store.environment, nullptr, true);
		ASSERT_EQ (0, mdb_drop (transaction, store.vote, 1));
		ASSERT_EQ (0, mdb_dbi_open (transaction, "sequence", MDB_CREATE, &store.vote));
		uint64_t sequence (10);
		ASSERT_EQ (0, mdb_put (transaction, store.vote, paper::mdb_val (key.pub), paper::mdb_val (sizeof (sequence), &sequence), 0));
		store.version_put (transaction, 8);
	}
	bool init (false);
	paper::block_store store (init, path);
	ASSERT_FALSE (init);
	paper::transaction transaction (store.environment, nullptr, false);
	ASSERT_LT (8, store.version_get (transaction));
	auto vote (store.vote_get (transaction, key.pub));
	ASSERT_NE (nullptr, vote);
	ASSERT_EQ (10, vote->sequence);
}

TEST (block_store, upgrade_v9_v10)
{
	auto path (paper::unique_path ());
	paper::block_hash hash (0);
	{
		bool init (false);
		paper::block_store store (init, path);
		ASSERT_FALSE (init);
		paper::transaction transaction (store.environment, nullptr, true);
		paper::genesis genesis;
		genesis.initialize (transaction, store);
		paper::ledger ledger (store);
		store.version_put (transaction, 9);
		paper::account_info info;
		store.account_get (transaction, paper::test_genesis_key.pub, info);
		paper::keypair key0;
		paper::uint128_t balance (paper::genesis_amount);
		hash = info.head;
		for (auto i (1); i < 32; ++i) // Making 31 send blocks (+ 1 open = 32 total)
		{
			balance = balance - paper::Gppr_ratio;
			paper::send_block block0 (hash, key0.pub, balance, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
			ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, block0).code);
			hash = block0.hash ();
		}
		paper::block_info block_info_auto; // Checking automatic block_info creation for block 32
		store.block_info_get (transaction, hash, block_info_auto);
		ASSERT_EQ (block_info_auto.account, paper::test_genesis_key.pub);
		ASSERT_EQ (block_info_auto.balance.number (), balance);
		ASSERT_EQ (0, mdb_drop (transaction, store.blocks_info, 0)); // Cleaning blocks_info subdatabase
		bool block_info_exists (store.block_info_exists (transaction, hash));
		ASSERT_EQ (block_info_exists, 0); // Checking if automatic block_info is deleted
	}
	bool init (false);
	paper::block_store store (init, path);
	ASSERT_FALSE (init);
	paper::transaction transaction (store.environment, nullptr, false);
	ASSERT_LT (9, store.version_get (transaction));
	paper::block_info block_info;
	store.block_info_get (transaction, hash, block_info);
	ASSERT_EQ (block_info.account, paper::test_genesis_key.pub);
	ASSERT_EQ (block_info.balance.number (), paper::genesis_amount - paper::Gppr_ratio * 31);
}
