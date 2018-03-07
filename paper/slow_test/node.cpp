#include <gtest/gtest.h>
#include <paper/node/testing.hpp>

#include <thread>

TEST (system, generate_mass_activity)
{
	paper::system system (24000, 1);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	size_t count (20);
	system.generate_mass_activity (count, *system.nodes[0]);
	size_t accounts (0);
	paper::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	for (auto i (system.nodes[0]->store.latest_begin (transaction)), n (system.nodes[0]->store.latest_end ()); i != n; ++i)
	{
		++accounts;
	}
}

TEST (system, generate_mass_activity_long)
{
	std::vector<std::thread> threads;
	{
		paper::system system (24000, 1);
		paper::thread_runner runner (system.service, system.nodes[0]->config.io_threads);
		system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
		size_t count (1000000000);
		system.generate_mass_activity (count, *system.nodes[0]);
		size_t accounts (0);
		paper::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
		for (auto i (system.nodes[0]->store.latest_begin (transaction)), n (system.nodes[0]->store.latest_end ()); i != n; ++i)
		{
			++accounts;
		}
		system.stop ();
		runner.join ();
	}
	for (auto i (threads.begin ()), n (threads.end ()); i != n; ++i)
	{
		i->join ();
	}
}

TEST (system, receive_while_synchronizing)
{
	std::vector<std::thread> threads;
	{
		paper::system system (24000, 1);
		paper::thread_runner runner (system.service, system.nodes[0]->config.io_threads);
		system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
		size_t count (1000);
		system.generate_mass_activity (count, *system.nodes[0]);
		paper::keypair key;
		paper::node_init init1;
		auto node1 (std::make_shared<paper::node> (init1, system.service, 24001, paper::unique_path (), system.alarm, system.logging, system.work));
		ASSERT_FALSE (init1.error ());
		node1->network.send_keepalive (system.nodes[0]->network.endpoint ());
		auto wallet (node1->wallets.create (1));
		ASSERT_EQ (key.pub, wallet->insert_adhoc (key.prv));
		node1->start ();
		system.alarm.add (std::chrono::steady_clock::now () + std::chrono::milliseconds (200), ([&system, &key]() {
			auto hash (system.wallet (0)->send_sync (paper::test_genesis_key.pub, key.pub, system.nodes[0]->config.receive_minimum.number ()));
			auto block (system.nodes[0]->store.block_get (paper::transaction (system.nodes[0]->store.environment, nullptr, false), hash));
			std::string block_text;
			block->serialize_json (block_text);
		}));
		while (node1->balance (key.pub).is_zero ())
		{
			system.poll ();
		}
		node1->stop ();
		system.stop ();
		runner.join ();
	}
	for (auto i (threads.begin ()), n (threads.end ()); i != n; ++i)
	{
		i->join ();
	}
}

TEST (ledger, deep_account_compute)
{
	bool init (false);
	paper::block_store store (init, paper::unique_path ());
	ASSERT_FALSE (init);
	paper::ledger ledger (store);
	paper::genesis genesis;
	paper::transaction transaction (store.environment, nullptr, true);
	genesis.initialize (transaction, store);
	paper::keypair key;
	auto balance (paper::genesis_amount - 1);
	paper::send_block send (genesis.hash (), key.pub, balance, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, send).code);
	paper::open_block open (send.hash (), paper::test_genesis_key.pub, key.pub, key.prv, key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, open).code);
	auto sprevious (send.hash ());
	auto rprevious (open.hash ());
	for (auto i (0), n (100000); i != n; ++i)
	{
		balance -= 1;
		paper::send_block send (sprevious, key.pub, balance, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
		ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, send).code);
		sprevious = send.hash ();
		paper::receive_block receive (rprevious, send.hash (), key.prv, key.pub, 0);
		ASSERT_EQ (paper::process_result::progress, ledger.process (transaction, receive).code);
		rprevious = receive.hash ();
		if (i % 100 == 0)
		{
			std::cerr << i << ' ';
		}
		auto account (ledger.account (transaction, sprevious));
		(void)account;
		auto balance (ledger.balance (transaction, rprevious));
		(void)balance;
	}
}

TEST (wallet, multithreaded_send)
{
	std::vector<std::thread> threads;
	{
		paper::system system (24000, 1);
		paper::keypair key;
		auto wallet_l (system.wallet (0));
		wallet_l->insert_adhoc (paper::test_genesis_key.prv);
		for (auto i (0); i < 20; ++i)
		{
			threads.push_back (std::thread ([wallet_l, &key]() {
				for (auto i (0); i < 1000; ++i)
				{
					wallet_l->send_action (paper::test_genesis_key.pub, key.pub, 1000);
				}
			}));
		}
		while (system.nodes[0]->balance (paper::test_genesis_key.pub) != (paper::genesis_amount - 20 * 1000 * 1000))
		{
			system.poll ();
		}
	}
	for (auto i (threads.begin ()), n (threads.end ()); i != n; ++i)
	{
		i->join ();
	}
}

TEST (store, load)
{
	paper::system system (24000, 1);
	std::vector<std::thread> threads;
	for (auto i (0); i < 100; ++i)
	{
		threads.push_back (std::thread ([&system]() {
			for (auto i (0); i != 1000; ++i)
			{
				paper::transaction transaction (system.nodes[0]->store.environment, nullptr, true);
				for (auto j (0); j != 10; ++j)
				{
					paper::block_hash hash;
					paper::random_pool.GenerateBlock (hash.bytes.data (), hash.bytes.size ());
					system.nodes[0]->store.account_put (transaction, hash, paper::account_info ());
				}
			}
		}));
	}
	for (auto & i : threads)
	{
		i.join ();
	}
}

TEST (node, fork_storm)
{
	paper::system system (24000, 64);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	auto previous (system.nodes[0]->latest (paper::test_genesis_key.pub));
	auto balance (system.nodes[0]->balance (paper::test_genesis_key.pub));
	ASSERT_FALSE (previous.is_zero ());
	for (auto j (0); j != system.nodes.size (); ++j)
	{
		balance -= 1;
		paper::keypair key;
		paper::send_block send (previous, key.pub, balance, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
		previous = send.hash ();
		for (auto i (0); i != system.nodes.size (); ++i)
		{
			auto send_result (system.nodes[i]->process (send));
			ASSERT_EQ (paper::process_result::progress, send_result.code);
			paper::keypair rep;
			auto open (std::make_shared<paper::open_block> (previous, rep.pub, key.pub, key.prv, key.pub, 0));
			system.nodes[i]->generate_work (*open);
			auto open_result (system.nodes[i]->process (*open));
			ASSERT_EQ (paper::process_result::progress, open_result.code);
			paper::transaction transaction (system.nodes[i]->store.environment, nullptr, false);
			system.nodes[i]->network.republish_block (transaction, open);
		}
	}
	auto again (true);

	int empty (0);
	int single (0);
	int iteration (0);
	while (again)
	{
		empty = 0;
		single = 0;
		std::for_each (system.nodes.begin (), system.nodes.end (), [&](std::shared_ptr<paper::node> const & node_a) {
			if (node_a->active.roots.empty ())
			{
				++empty;
			}
			else
			{
				if (node_a->active.roots.begin ()->election->votes.rep_votes.size () == 1)
				{
					++single;
				}
			}
		});
		system.poll ();
		if ((iteration & 0xff) == 0)
		{
			std::cerr << "Empty: " << empty << " single: " << single << std::endl;
		}
		again = empty != 0 || single != 0;
		++iteration;
	}
	ASSERT_TRUE (true);
}

namespace
{
size_t heard_count (std::vector<uint8_t> const & nodes)
{
	auto result (0);
	for (auto i (nodes.begin ()), n (nodes.end ()); i != n; ++i)
	{
		switch (*i)
		{
			case 0:
				break;
			case 1:
				++result;
				break;
			case 2:
				++result;
				break;
		}
	}
	return result;
}
}

TEST (broadcast, world_broadcast_simulate)
{
	auto node_count (10000);
	// 0 = starting state
	// 1 = heard transaction
	// 2 = repeated transaction
	std::vector<uint8_t> nodes;
	nodes.resize (node_count, 0);
	nodes[0] = 1;
	auto any_changed (true);
	auto message_count (0);
	while (any_changed)
	{
		any_changed = false;
		for (auto i (nodes.begin ()), n (nodes.end ()); i != n; ++i)
		{
			switch (*i)
			{
				case 0:
					break;
				case 1:
					for (auto j (nodes.begin ()), m (nodes.end ()); j != m; ++j)
					{
						++message_count;
						switch (*j)
						{
							case 0:
								*j = 1;
								any_changed = true;
								break;
							case 1:
								break;
							case 2:
								break;
						}
					}
					*i = 2;
					any_changed = true;
					break;
				case 2:
					break;
				default:
					assert (false);
					break;
			}
		}
	}
	auto count (heard_count (nodes));
	printf ("");
}

TEST (broadcast, sqrt_broadcast_simulate)
{
	auto node_count (200);
	auto broadcast_count (std::ceil (std::sqrt (node_count)));
	// 0 = starting state
	// 1 = heard transaction
	// 2 = repeated transaction
	std::vector<uint8_t> nodes;
	nodes.resize (node_count, 0);
	nodes[0] = 1;
	auto any_changed (true);
	uint64_t message_count (0);
	while (any_changed)
	{
		any_changed = false;
		for (auto i (nodes.begin ()), n (nodes.end ()); i != n; ++i)
		{
			switch (*i)
			{
				case 0:
					break;
				case 1:
					for (auto j (0); j != broadcast_count; ++j)
					{
						++message_count;
						auto entry (paper::random_pool.GenerateWord32 (0, node_count - 1));
						switch (nodes[entry])
						{
							case 0:
								nodes[entry] = 1;
								any_changed = true;
								break;
							case 1:
								break;
							case 2:
								break;
						}
					}
					*i = 2;
					any_changed = true;
					break;
				case 2:
					break;
				default:
					assert (false);
					break;
			}
		}
	}
	auto count (heard_count (nodes));
	printf ("");
}

TEST (peer_container, random_set)
{
	auto loopback (boost::asio::ip::address_v6::loopback ());
	paper::peer_container container (paper::endpoint (loopback, 24000));
	for (auto i (0); i < 200; ++i)
	{
		container.contacted (paper::endpoint (loopback, 24001 + i), 0);
	}
	auto old (std::chrono::steady_clock::now ());
	for (auto i (0); i < 10000; ++i)
	{
		auto list (container.list_sqrt ());
	}
	auto current (std::chrono::steady_clock::now ());
	for (auto i (0); i < 10000; ++i)
	{
		auto list (container.random_set (15));
	}
	auto end (std::chrono::steady_clock::now ());
	auto old_ms (std::chrono::duration_cast<std::chrono::milliseconds> (current - old));
	auto new_ms (std::chrono::duration_cast<std::chrono::milliseconds> (end - current));
}

TEST (store, unchecked_load)
{
	paper::system system (24000, 1);
	auto & node (*system.nodes[0]);
	auto block (std::make_shared<paper::send_block> (0, 0, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0));
	for (auto i (0); i < 1000000; ++i)
	{
		paper::transaction transaction (node.store.environment, nullptr, true);
		node.store.unchecked_put (transaction, i, block);
	}
	paper::transaction transaction (node.store.environment, nullptr, false);
	auto count (node.store.unchecked_count (transaction));
}

TEST (store, vote_load)
{
	paper::system system (24000, 1);
	auto & node (*system.nodes[0]);
	auto block (std::make_shared<paper::send_block> (0, 0, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0));
	for (auto i (0); i < 1000000; ++i)
	{
		paper::transaction transaction (node.store.environment, nullptr, true);
		auto vote (std::make_shared<paper::vote> (paper::test_genesis_key.pub, paper::test_genesis_key.prv, i, block));
		node.store.vote_validate (transaction, vote);
	}
}
