#include <gtest/gtest.h>
#include <paper/node/testing.hpp>

TEST (gap_cache, add_new)
{
	paper::system system (24000, 1);
	paper::gap_cache cache (*system.nodes[0]);
	auto block1 (std::make_shared<paper::send_block> (0, 1, 2, paper::keypair ().prv, 4, 5));
	paper::transaction transaction (system.nodes[0]->store.environment, nullptr, true);
	cache.add (transaction, block1);
}

TEST (gap_cache, add_existing)
{
	paper::system system (24000, 1);
	paper::gap_cache cache (*system.nodes[0]);
	auto block1 (std::make_shared<paper::send_block> (0, 1, 2, paper::keypair ().prv, 4, 5));
	paper::transaction transaction (system.nodes[0]->store.environment, nullptr, true);
	cache.add (transaction, block1);
	auto existing1 (cache.blocks.get<1> ().find (block1->hash ()));
	ASSERT_NE (cache.blocks.get<1> ().end (), existing1);
	auto arrival (existing1->arrival);
	while (arrival == std::chrono::steady_clock::now ())
		;
	cache.add (transaction, block1);
	ASSERT_EQ (1, cache.blocks.size ());
	auto existing2 (cache.blocks.get<1> ().find (block1->hash ()));
	ASSERT_NE (cache.blocks.get<1> ().end (), existing2);
	ASSERT_GT (existing2->arrival, arrival);
}

TEST (gap_cache, comparison)
{
	paper::system system (24000, 1);
	paper::gap_cache cache (*system.nodes[0]);
	auto block1 (std::make_shared<paper::send_block> (1, 0, 2, paper::keypair ().prv, 4, 5));
	paper::transaction transaction (system.nodes[0]->store.environment, nullptr, true);
	cache.add (transaction, block1);
	auto existing1 (cache.blocks.get<1> ().find (block1->hash ()));
	ASSERT_NE (cache.blocks.get<1> ().end (), existing1);
	auto arrival (existing1->arrival);
	while (std::chrono::steady_clock::now () == arrival)
		;
	auto block3 (std::make_shared<paper::send_block> (0, 42, 1, paper::keypair ().prv, 3, 4));
	cache.add (transaction, block3);
	ASSERT_EQ (2, cache.blocks.size ());
	auto existing2 (cache.blocks.get<1> ().find (block3->hash ()));
	ASSERT_NE (cache.blocks.get<1> ().end (), existing2);
	ASSERT_GT (existing2->arrival, arrival);
	ASSERT_EQ (arrival, cache.blocks.get<1> ().begin ()->arrival);
}

TEST (gap_cache, gap_bootstrap)
{
	paper::system system (24000, 2);
	paper::block_hash latest (system.nodes[0]->latest (paper::test_genesis_key.pub));
	paper::keypair key;
	auto send (std::make_shared<paper::send_block> (latest, key.pub, paper::genesis_amount - 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, system.work.generate (latest)));
	{
		paper::transaction transaction (system.nodes[0]->store.environment, nullptr, true);
		ASSERT_EQ (paper::process_result::progress, system.nodes[0]->block_processor.process_receive_one (transaction, send).code);
	}
	ASSERT_EQ (paper::genesis_amount - 100, system.nodes[0]->balance (paper::genesis_account));
	ASSERT_EQ (paper::genesis_amount, system.nodes[1]->balance (paper::genesis_account));
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key.prv);
	system.wallet (0)->send_action (paper::test_genesis_key.pub, key.pub, 100);
	ASSERT_EQ (paper::genesis_amount - 200, system.nodes[0]->balance (paper::genesis_account));
	ASSERT_EQ (paper::genesis_amount, system.nodes[1]->balance (paper::genesis_account));
	auto iterations2 (0);
	while (system.nodes[1]->balance (paper::genesis_account) != paper::genesis_amount - 200)
	{
		system.poll ();
		++iterations2;
		ASSERT_LT (iterations2, 200);
	}
}

TEST (gap_cache, two_dependencies)
{
	paper::system system (24000, 1);
	paper::keypair key;
	paper::genesis genesis;
	auto send1 (std::make_shared<paper::send_block> (genesis.hash (), key.pub, 1, paper::test_genesis_key.prv, paper::test_genesis_key.pub, system.work.generate (genesis.hash ())));
	auto send2 (std::make_shared<paper::send_block> (send1->hash (), key.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, system.work.generate (send1->hash ())));
	auto open (std::make_shared<paper::open_block> (send1->hash (), key.pub, key.pub, key.prv, key.pub, system.work.generate (key.pub)));
	ASSERT_EQ (0, system.nodes[0]->gap_cache.blocks.size ());
	system.nodes[0]->block_processor.process_receive_many (paper::block_processor_item (send2));
	ASSERT_EQ (1, system.nodes[0]->gap_cache.blocks.size ());
	system.nodes[0]->block_processor.process_receive_many (paper::block_processor_item (open));
	ASSERT_EQ (2, system.nodes[0]->gap_cache.blocks.size ());
	system.nodes[0]->block_processor.process_receive_many (paper::block_processor_item (send1));
	ASSERT_EQ (0, system.nodes[0]->gap_cache.blocks.size ());
	paper::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	ASSERT_TRUE (system.nodes[0]->store.block_exists (transaction, send1->hash ()));
	ASSERT_TRUE (system.nodes[0]->store.block_exists (transaction, send2->hash ()));
	ASSERT_TRUE (system.nodes[0]->store.block_exists (transaction, open->hash ()));
}
