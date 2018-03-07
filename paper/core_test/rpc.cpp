#include <gtest/gtest.h>

#include <boost/beast.hpp>
#include <paper/node/common.hpp>
#include <paper/node/rpc.hpp>
#include <paper/node/testing.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/thread.hpp>

class test_response
{
public:
	test_response (boost::property_tree::ptree const & request_a, paper::rpc & rpc_a, boost::asio::io_service & service_a) :
	request (request_a),
	sock (service_a),
	status (0)
	{
		sock.async_connect (paper::tcp_endpoint (boost::asio::ip::address_v6::loopback (), rpc_a.config.port), [this](boost::system::error_code const & ec) {
			if (!ec)
			{
				std::stringstream ostream;
				boost::property_tree::write_json (ostream, request);
				req.method (boost::beast::http::verb::post);
				req.target ("/");
				req.version (11);
				ostream.flush ();
				req.body () = ostream.str ();
				req.prepare_payload ();
				boost::beast::http::async_write (sock, req, [this](boost::system::error_code const & ec, size_t bytes_transferred) {
					if (!ec)
					{
						boost::beast::http::async_read (sock, sb, resp, [this](boost::system::error_code const & ec, size_t bytes_transferred) {
							if (!ec)
							{
								std::stringstream body (resp.body ());
								try
								{
									boost::property_tree::read_json (body, json);
									status = 200;
								}
								catch (std::exception & e)
								{
									status = 500;
								}
							}
							else
							{
								status = 400;
							};
						});
					}
					else
					{
						status = 600;
					}
				});
			}
			else
			{
				status = 400;
			}
		});
	}
	boost::property_tree::ptree const & request;
	boost::asio::ip::tcp::socket sock;
	boost::property_tree::ptree json;
	boost::beast::flat_buffer sb;
	boost::beast::http::request<boost::beast::http::string_body> req;
	boost::beast::http::response<boost::beast::http::string_body> resp;
	int status;
};

TEST (rpc, account_balance)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "account_balance");
	request.put ("account", paper::test_genesis_key.pub.to_account ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string balance_text (response.json.get<std::string> ("balance"));
	ASSERT_EQ ("340282366920938463463374607431768211455", balance_text);
	std::string pending_text (response.json.get<std::string> ("pending"));
	ASSERT_EQ ("0", pending_text);
}

TEST (rpc, account_block_count)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "account_block_count");
	request.put ("account", paper::test_genesis_key.pub.to_account ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string block_count_text (response.json.get<std::string> ("block_count"));
	ASSERT_EQ ("1", block_count_text);
}

TEST (rpc, account_create)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "account_create");
	request.put ("wallet", system.nodes[0]->wallets.items.begin ()->first.to_string ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto account_text (response.json.get<std::string> ("account"));
	paper::uint256_union account;
	ASSERT_FALSE (account.decode_account (account_text));
	ASSERT_TRUE (system.wallet (0)->exists (account));
}

TEST (rpc, account_weight)
{
	paper::keypair key;
	paper::system system (24000, 1);
	paper::block_hash latest (system.nodes[0]->latest (paper::test_genesis_key.pub));
	auto & node1 (*system.nodes[0]);
	paper::change_block block (latest, key.pub, paper::test_genesis_key.prv, paper::test_genesis_key.pub, node1.generate_work (latest));
	ASSERT_EQ (paper::process_result::progress, node1.process (block).code);
	paper::rpc rpc (system.service, node1, paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "account_weight");
	request.put ("account", key.pub.to_account ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string balance_text (response.json.get<std::string> ("weight"));
	ASSERT_EQ ("340282366920938463463374607431768211455", balance_text);
}

TEST (rpc, wallet_contains)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	boost::property_tree::ptree request;
	std::string wallet;
	system.nodes[0]->wallets.items.begin ()->first.encode_hex (wallet);
	request.put ("wallet", wallet);
	request.put ("action", "wallet_contains");
	request.put ("account", paper::test_genesis_key.pub.to_account ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string exists_text (response.json.get<std::string> ("exists"));
	ASSERT_EQ ("1", exists_text);
}

TEST (rpc, wallet_doesnt_contain)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	std::string wallet;
	system.nodes[0]->wallets.items.begin ()->first.encode_hex (wallet);
	request.put ("wallet", wallet);
	request.put ("action", "wallet_contains");
	request.put ("account", paper::test_genesis_key.pub.to_account ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string exists_text (response.json.get<std::string> ("exists"));
	ASSERT_EQ ("0", exists_text);
}

TEST (rpc, validate_account_number)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	boost::property_tree::ptree request;
	request.put ("action", "validate_account_number");
	request.put ("account", paper::test_genesis_key.pub.to_account ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	std::string exists_text (response.json.get<std::string> ("valid"));
	ASSERT_EQ ("1", exists_text);
}

TEST (rpc, validate_account_invalid)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	std::string account;
	paper::test_genesis_key.pub.encode_account (account);
	account[0] ^= 0x1;
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	boost::property_tree::ptree request;
	request.put ("action", "validate_account_number");
	request.put ("account", account);
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string exists_text (response.json.get<std::string> ("valid"));
	ASSERT_EQ ("0", exists_text);
}

TEST (rpc, send)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	boost::property_tree::ptree request;
	std::string wallet;
	system.nodes[0]->wallets.items.begin ()->first.encode_hex (wallet);
	request.put ("wallet", wallet);
	request.put ("action", "send");
	request.put ("source", paper::test_genesis_key.pub.to_account ());
	request.put ("destination", paper::test_genesis_key.pub.to_account ());
	request.put ("amount", "100");
	std::thread thread2 ([&system]() {
		auto iterations (0);
		while (system.nodes[0]->balance (paper::test_genesis_key.pub) == paper::genesis_amount)
		{
			system.poll ();
			++iterations;
			ASSERT_LT (iterations, 200);
		}
	});
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string block_text (response.json.get<std::string> ("block"));
	paper::block_hash block;
	ASSERT_FALSE (block.decode_hex (block_text));
	ASSERT_TRUE (system.nodes[0]->ledger.block_exists (block));
	thread2.join ();
}

TEST (rpc, send_fail)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	std::string wallet;
	system.nodes[0]->wallets.items.begin ()->first.encode_hex (wallet);
	request.put ("wallet", wallet);
	request.put ("action", "send");
	request.put ("source", paper::test_genesis_key.pub.to_account ());
	request.put ("destination", paper::test_genesis_key.pub.to_account ());
	request.put ("amount", "100");
	std::atomic<bool> done (false);
	std::thread thread2 ([&system, &done]() {
		auto iterations (0);
		while (!done)
		{
			system.poll ();
			++iterations;
			ASSERT_LT (iterations, 200);
		}
	});
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	done = true;
	ASSERT_EQ (response.json.get<std::string> ("error"), "Error generating block");
	thread2.join ();
}

TEST (rpc, send_idempotent)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	boost::property_tree::ptree request;
	std::string wallet;
	system.nodes[0]->wallets.items.begin ()->first.encode_hex (wallet);
	request.put ("wallet", wallet);
	request.put ("action", "send");
	request.put ("source", paper::test_genesis_key.pub.to_account ());
	request.put ("destination", paper::test_genesis_key.pub.to_account ());
	request.put ("amount", "100");
	request.put ("id", "123abc");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string block_text (response.json.get<std::string> ("block"));
	paper::block_hash block;
	ASSERT_FALSE (block.decode_hex (block_text));
	ASSERT_TRUE (system.nodes[0]->ledger.block_exists (block));
	ASSERT_EQ (system.nodes[0]->balance (paper::test_genesis_key.pub), paper::genesis_amount - 100);
	test_response response2 (request, rpc, system.service);
	while (response2.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response2.status);
	ASSERT_EQ (response2.json.get<std::string> ("block"), block_text);
	ASSERT_EQ (system.nodes[0]->balance (paper::test_genesis_key.pub), paper::genesis_amount - 100);
	request.erase ("id");
	request.put ("id", "456def");
	test_response response3 (request, rpc, system.service);
	while (response3.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response3.status);
	std::string block2_text (response3.json.get<std::string> ("block"));
	paper::block_hash block2;
	ASSERT_NE (block2, block);
	ASSERT_FALSE (block2.decode_hex (block2_text));
	ASSERT_TRUE (system.nodes[0]->ledger.block_exists (block2));
	ASSERT_EQ (system.nodes[0]->balance (paper::test_genesis_key.pub), paper::genesis_amount - 200);
}

TEST (rpc, stop)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "stop");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	};
	ASSERT_FALSE (system.nodes[0]->network.on);
}

TEST (rpc, wallet_add)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	paper::keypair key1;
	std::string key_text;
	key1.prv.data.encode_hex (key_text);
	system.wallet (0)->insert_adhoc (key1.prv);
	boost::property_tree::ptree request;
	std::string wallet;
	system.nodes[0]->wallets.items.begin ()->first.encode_hex (wallet);
	request.put ("wallet", wallet);
	request.put ("action", "wallet_add");
	request.put ("key", key_text);
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string account_text1 (response.json.get<std::string> ("account"));
	ASSERT_EQ (account_text1, key1.pub.to_account ());
}

TEST (rpc, wallet_password_valid)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	std::string wallet;
	system.nodes[0]->wallets.items.begin ()->first.encode_hex (wallet);
	request.put ("wallet", wallet);
	request.put ("action", "password_valid");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string account_text1 (response.json.get<std::string> ("valid"));
	ASSERT_EQ (account_text1, "1");
}

TEST (rpc, wallet_password_change)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	std::string wallet;
	system.nodes[0]->wallets.items.begin ()->first.encode_hex (wallet);
	request.put ("wallet", wallet);
	request.put ("action", "password_change");
	request.put ("password", "test");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string account_text1 (response.json.get<std::string> ("changed"));
	ASSERT_EQ (account_text1, "1");
	ASSERT_TRUE (system.wallet (0)->valid_password ());
	ASSERT_TRUE (system.wallet (0)->enter_password (""));
	ASSERT_FALSE (system.wallet (0)->valid_password ());
	ASSERT_FALSE (system.wallet (0)->enter_password ("test"));
	ASSERT_TRUE (system.wallet (0)->valid_password ());
}

TEST (rpc, wallet_password_enter)
{
	paper::system system (24000, 1);
	auto iterations (0);
	paper::raw_key password_l;
	password_l.data.clear ();
	while (password_l.data == 0)
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
		system.wallet (0)->store.password.value (password_l);
	}
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	std::string wallet;
	system.nodes[0]->wallets.items.begin ()->first.encode_hex (wallet);
	request.put ("wallet", wallet);
	request.put ("action", "password_enter");
	request.put ("password", "");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string account_text1 (response.json.get<std::string> ("valid"));
	ASSERT_EQ (account_text1, "1");
}

TEST (rpc, wallet_representative)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	std::string wallet;
	system.nodes[0]->wallets.items.begin ()->first.encode_hex (wallet);
	request.put ("wallet", wallet);
	request.put ("action", "wallet_representative");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string account_text1 (response.json.get<std::string> ("representative"));
	ASSERT_EQ (account_text1, paper::genesis_account.to_account ());
}

TEST (rpc, wallet_representative_set)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	std::string wallet;
	system.nodes[0]->wallets.items.begin ()->first.encode_hex (wallet);
	request.put ("wallet", wallet);
	paper::keypair key;
	request.put ("action", "wallet_representative_set");
	request.put ("representative", key.pub.to_account ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	paper::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	ASSERT_EQ (key.pub, system.nodes[0]->wallets.items.begin ()->second->store.representative (transaction));
}

TEST (rpc, account_list)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	paper::keypair key2;
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key2.prv);
	boost::property_tree::ptree request;
	std::string wallet;
	system.nodes[0]->wallets.items.begin ()->first.encode_hex (wallet);
	request.put ("wallet", wallet);
	request.put ("action", "account_list");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto & accounts_node (response.json.get_child ("accounts"));
	std::vector<paper::uint256_union> accounts;
	for (auto i (accounts_node.begin ()), j (accounts_node.end ()); i != j; ++i)
	{
		auto account (i->second.get<std::string> (""));
		paper::uint256_union number;
		ASSERT_FALSE (number.decode_account (account));
		accounts.push_back (number);
	}
	ASSERT_EQ (2, accounts.size ());
	for (auto i (accounts.begin ()), j (accounts.end ()); i != j; ++i)
	{
		ASSERT_TRUE (system.wallet (0)->exists (*i));
	}
}

TEST (rpc, wallet_key_valid)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	boost::property_tree::ptree request;
	std::string wallet;
	system.nodes[0]->wallets.items.begin ()->first.encode_hex (wallet);
	request.put ("wallet", wallet);
	request.put ("action", "wallet_key_valid");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string exists_text (response.json.get<std::string> ("valid"));
	ASSERT_EQ ("1", exists_text);
}

TEST (rpc, wallet_create)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "wallet_create");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string wallet_text (response.json.get<std::string> ("wallet"));
	paper::uint256_union wallet_id;
	ASSERT_FALSE (wallet_id.decode_hex (wallet_text));
	ASSERT_NE (system.nodes[0]->wallets.items.end (), system.nodes[0]->wallets.items.find (wallet_id));
}

TEST (rpc, wallet_export)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	boost::property_tree::ptree request;
	request.put ("action", "wallet_export");
	request.put ("wallet", system.nodes[0]->wallets.items.begin ()->first.to_string ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string wallet_json (response.json.get<std::string> ("json"));
	bool error (false);
	paper::transaction transaction (system.nodes[0]->store.environment, nullptr, true);
	paper::kdf kdf;
	paper::wallet_store store (error, kdf, transaction, paper::genesis_account, 1, "0", wallet_json);
	ASSERT_FALSE (error);
	ASSERT_TRUE (store.exists (transaction, paper::test_genesis_key.pub));
}

TEST (rpc, wallet_destroy)
{
	paper::system system (24000, 1);
	auto wallet_id (system.nodes[0]->wallets.items.begin ()->first);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	boost::property_tree::ptree request;
	request.put ("action", "wallet_destroy");
	request.put ("wallet", wallet_id.to_string ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	ASSERT_EQ (system.nodes[0]->wallets.items.end (), system.nodes[0]->wallets.items.find (wallet_id));
}

TEST (rpc, account_move)
{
	paper::system system (24000, 1);
	auto wallet_id (system.nodes[0]->wallets.items.begin ()->first);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	auto destination (system.wallet (0));
	paper::keypair key;
	destination->insert_adhoc (paper::test_genesis_key.prv);
	paper::keypair source_id;
	auto source (system.nodes[0]->wallets.create (source_id.pub));
	source->insert_adhoc (key.prv);
	boost::property_tree::ptree request;
	request.put ("action", "account_move");
	request.put ("wallet", wallet_id.to_string ());
	request.put ("source", source_id.pub.to_string ());
	boost::property_tree::ptree keys;
	boost::property_tree::ptree entry;
	entry.put ("", key.pub.to_string ());
	keys.push_back (std::make_pair ("", entry));
	request.add_child ("accounts", keys);
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	ASSERT_EQ ("1", response.json.get<std::string> ("moved"));
	ASSERT_TRUE (destination->exists (key.pub));
	ASSERT_TRUE (destination->exists (paper::test_genesis_key.pub));
	paper::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	ASSERT_EQ (source->store.end (), source->store.begin (transaction));
}

TEST (rpc, block)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "block");
	request.put ("hash", system.nodes[0]->latest (paper::genesis_account).to_string ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto contents (response.json.get<std::string> ("contents"));
	ASSERT_FALSE (contents.empty ());
}

TEST (rpc, block_account)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	paper::genesis genesis;
	boost::property_tree::ptree request;
	request.put ("action", "block_account");
	request.put ("hash", genesis.hash ().to_string ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string account_text (response.json.get<std::string> ("account"));
	paper::account account;
	ASSERT_FALSE (account.decode_account (account_text));
}

TEST (rpc, chain)
{
	paper::system system (24000, 1);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	paper::keypair key;
	auto genesis (system.nodes[0]->latest (paper::test_genesis_key.pub));
	ASSERT_FALSE (genesis.is_zero ());
	auto block (system.wallet (0)->send_action (paper::test_genesis_key.pub, key.pub, 1));
	ASSERT_NE (nullptr, block);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "chain");
	request.put ("block", block->hash ().to_string ());
	request.put ("count", std::to_string (std::numeric_limits<uint64_t>::max ()));
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto & blocks_node (response.json.get_child ("blocks"));
	std::vector<paper::block_hash> blocks;
	for (auto i (blocks_node.begin ()), n (blocks_node.end ()); i != n; ++i)
	{
		blocks.push_back (paper::block_hash (i->second.get<std::string> ("")));
	}
	ASSERT_EQ (2, blocks.size ());
	ASSERT_EQ (block->hash (), blocks[0]);
	ASSERT_EQ (genesis, blocks[1]);
}

TEST (rpc, chain_limit)
{
	paper::system system (24000, 1);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	paper::keypair key;
	auto genesis (system.nodes[0]->latest (paper::test_genesis_key.pub));
	ASSERT_FALSE (genesis.is_zero ());
	auto block (system.wallet (0)->send_action (paper::test_genesis_key.pub, key.pub, 1));
	ASSERT_NE (nullptr, block);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "chain");
	request.put ("block", block->hash ().to_string ());
	request.put ("count", 1);
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto & blocks_node (response.json.get_child ("blocks"));
	std::vector<paper::block_hash> blocks;
	for (auto i (blocks_node.begin ()), n (blocks_node.end ()); i != n; ++i)
	{
		blocks.push_back (paper::block_hash (i->second.get<std::string> ("")));
	}
	ASSERT_EQ (1, blocks.size ());
	ASSERT_EQ (block->hash (), blocks[0]);
}

TEST (rpc, frontier)
{
	paper::system system (24000, 1);
	std::unordered_map<paper::account, paper::block_hash> source;
	{
		paper::transaction transaction (system.nodes[0]->store.environment, nullptr, true);
		for (auto i (0); i < 1000; ++i)
		{
			paper::keypair key;
			source[key.pub] = key.prv.data;
			system.nodes[0]->store.account_put (transaction, key.pub, paper::account_info (key.prv.data, 0, 0, 0, 0, 0));
		}
	}
	paper::keypair key;
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "frontiers");
	request.put ("account", paper::account (0).to_account ());
	request.put ("count", std::to_string (std::numeric_limits<uint64_t>::max ()));
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto & frontiers_node (response.json.get_child ("frontiers"));
	std::unordered_map<paper::account, paper::block_hash> frontiers;
	for (auto i (frontiers_node.begin ()), j (frontiers_node.end ()); i != j; ++i)
	{
		paper::account account;
		account.decode_account (i->first);
		paper::block_hash frontier;
		frontier.decode_hex (i->second.get<std::string> (""));
		frontiers[account] = frontier;
	}
	ASSERT_EQ (1, frontiers.erase (paper::test_genesis_key.pub));
	ASSERT_EQ (source, frontiers);
}

TEST (rpc, frontier_limited)
{
	paper::system system (24000, 1);
	std::unordered_map<paper::account, paper::block_hash> source;
	{
		paper::transaction transaction (system.nodes[0]->store.environment, nullptr, true);
		for (auto i (0); i < 1000; ++i)
		{
			paper::keypair key;
			source[key.pub] = key.prv.data;
			system.nodes[0]->store.account_put (transaction, key.pub, paper::account_info (key.prv.data, 0, 0, 0, 0, 0));
		}
	}
	paper::keypair key;
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "frontiers");
	request.put ("account", paper::account (0).to_account ());
	request.put ("count", std::to_string (100));
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto & frontiers_node (response.json.get_child ("frontiers"));
	ASSERT_EQ (100, frontiers_node.size ());
}

TEST (rpc, frontier_startpoint)
{
	paper::system system (24000, 1);
	std::unordered_map<paper::account, paper::block_hash> source;
	{
		paper::transaction transaction (system.nodes[0]->store.environment, nullptr, true);
		for (auto i (0); i < 1000; ++i)
		{
			paper::keypair key;
			source[key.pub] = key.prv.data;
			system.nodes[0]->store.account_put (transaction, key.pub, paper::account_info (key.prv.data, 0, 0, 0, 0, 0));
		}
	}
	paper::keypair key;
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "frontiers");
	request.put ("account", source.begin ()->first.to_account ());
	request.put ("count", std::to_string (1));
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto & frontiers_node (response.json.get_child ("frontiers"));
	ASSERT_EQ (1, frontiers_node.size ());
	ASSERT_EQ (source.begin ()->first.to_account (), frontiers_node.begin ()->first);
}

TEST (rpc, history)
{
	paper::system system (24000, 1);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	auto change (system.wallet (0)->change_action (paper::test_genesis_key.pub, paper::test_genesis_key.pub));
	ASSERT_NE (nullptr, change);
	auto send (system.wallet (0)->send_action (paper::test_genesis_key.pub, paper::test_genesis_key.pub, system.nodes[0]->config.receive_minimum.number ()));
	ASSERT_NE (nullptr, send);
	auto receive (system.wallet (0)->receive_action (static_cast<paper::send_block &> (*send), paper::test_genesis_key.pub, system.nodes[0]->config.receive_minimum.number ()));
	ASSERT_NE (nullptr, receive);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "history");
	request.put ("hash", receive->hash ().to_string ());
	request.put ("count", 100);
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::vector<std::tuple<std::string, std::string, std::string, std::string>> history_l;
	auto & history_node (response.json.get_child ("history"));
	for (auto i (history_node.begin ()), n (history_node.end ()); i != n; ++i)
	{
		history_l.push_back (std::make_tuple (i->second.get<std::string> ("type"), i->second.get<std::string> ("account"), i->second.get<std::string> ("amount"), i->second.get<std::string> ("hash")));
	}
	ASSERT_EQ (3, history_l.size ());
	ASSERT_EQ ("receive", std::get<0> (history_l[0]));
	ASSERT_EQ (paper::test_genesis_key.pub.to_account (), std::get<1> (history_l[0]));
	ASSERT_EQ (system.nodes[0]->config.receive_minimum.to_string_dec (), std::get<2> (history_l[0]));
	ASSERT_EQ (receive->hash ().to_string (), std::get<3> (history_l[0]));
	ASSERT_EQ ("send", std::get<0> (history_l[1]));
	ASSERT_EQ (paper::test_genesis_key.pub.to_account (), std::get<1> (history_l[1]));
	ASSERT_EQ (system.nodes[0]->config.receive_minimum.to_string_dec (), std::get<2> (history_l[1]));
	ASSERT_EQ (send->hash ().to_string (), std::get<3> (history_l[1]));
	paper::genesis genesis;
	ASSERT_EQ ("receive", std::get<0> (history_l[2]));
	ASSERT_EQ (paper::test_genesis_key.pub.to_account (), std::get<1> (history_l[2]));
	ASSERT_EQ (paper::genesis_amount.convert_to<std::string> (), std::get<2> (history_l[2]));
	ASSERT_EQ (genesis.hash ().to_string (), std::get<3> (history_l[2]));
}

TEST (rpc, history_count)
{
	paper::system system (24000, 1);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	auto change (system.wallet (0)->change_action (paper::test_genesis_key.pub, paper::test_genesis_key.pub));
	ASSERT_NE (nullptr, change);
	auto send (system.wallet (0)->send_action (paper::test_genesis_key.pub, paper::test_genesis_key.pub, system.nodes[0]->config.receive_minimum.number ()));
	ASSERT_NE (nullptr, send);
	auto receive (system.wallet (0)->receive_action (static_cast<paper::send_block &> (*send), paper::test_genesis_key.pub, system.nodes[0]->config.receive_minimum.number ()));
	ASSERT_NE (nullptr, receive);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "history");
	request.put ("hash", receive->hash ().to_string ());
	request.put ("count", 1);
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto & history_node (response.json.get_child ("history"));
	ASSERT_EQ (1, history_node.size ());
}

TEST (rpc, process_block)
{
	paper::system system (24000, 1);
	paper::keypair key;
	auto latest (system.nodes[0]->latest (paper::test_genesis_key.pub));
	auto & node1 (*system.nodes[0]);
	paper::send_block send (latest, key.pub, 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, node1.generate_work (latest));
	paper::rpc rpc (system.service, node1, paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "process");
	std::string json;
	send.serialize_json (json);
	request.put ("block", json);
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	ASSERT_EQ (send.hash (), system.nodes[0]->latest (paper::test_genesis_key.pub));
	std::string send_hash (response.json.get<std::string> ("hash"));
	ASSERT_EQ (send.hash ().to_string (), send_hash);
}

TEST (rpc, process_block_no_work)
{
	paper::system system (24000, 1);
	paper::keypair key;
	auto latest (system.nodes[0]->latest (paper::test_genesis_key.pub));
	auto & node1 (*system.nodes[0]);
	paper::send_block send (latest, key.pub, 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, node1.generate_work (latest));
	send.block_work_set (0);
	paper::rpc rpc (system.service, node1, paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "process");
	std::string json;
	send.serialize_json (json);
	request.put ("block", json);
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	ASSERT_FALSE (response.json.get<std::string> ("error").empty ());
}

TEST (rpc, keepalive)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto node1 (std::make_shared<paper::node> (init1, system.service, 24001, paper::unique_path (), system.alarm, system.logging, system.work));
	node1->start ();
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "keepalive");
	auto address (boost::str (boost::format ("%1%") % node1->network.endpoint ().address ()));
	auto port (boost::str (boost::format ("%1%") % node1->network.endpoint ().port ()));
	request.put ("address", address);
	request.put ("port", port);
	ASSERT_FALSE (system.nodes[0]->peers.known_peer (node1->network.endpoint ()));
	ASSERT_EQ (0, system.nodes[0]->peers.size ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto iterations (0);
	while (!system.nodes[0]->peers.known_peer (node1->network.endpoint ()))
	{
		ASSERT_EQ (0, system.nodes[0]->peers.size ());
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	node1->stop ();
}

TEST (rpc, payment_init)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto node1 (system.nodes[0]);
	paper::keypair wallet_id;
	auto wallet (node1->wallets.create (wallet_id.pub));
	ASSERT_TRUE (node1->wallets.items.find (wallet_id.pub) != node1->wallets.items.end ());
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "payment_init");
	request.put ("wallet", wallet_id.pub.to_string ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	ASSERT_EQ ("Ready", response.json.get<std::string> ("status"));
}

TEST (rpc, payment_begin_end)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto node1 (system.nodes[0]);
	paper::keypair wallet_id;
	auto wallet (node1->wallets.create (wallet_id.pub));
	ASSERT_TRUE (node1->wallets.items.find (wallet_id.pub) != node1->wallets.items.end ());
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request1;
	request1.put ("action", "payment_begin");
	request1.put ("wallet", wallet_id.pub.to_string ());
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	auto account_text (response1.json.get<std::string> ("account"));
	paper::uint256_union account;
	ASSERT_FALSE (account.decode_account (account_text));
	ASSERT_TRUE (wallet->exists (account));
	auto root1 (system.nodes[0]->ledger.latest_root (paper::transaction (wallet->store.environment, nullptr, false), account));
	uint64_t work (0);
	auto iteration (0);
	ASSERT_TRUE (paper::work_validate (root1, work));
	while (paper::work_validate (root1, work))
	{
		system.poll ();
		ASSERT_FALSE (wallet->store.work_get (paper::transaction (wallet->store.environment, nullptr, false), account, work));
		++iteration;
		ASSERT_LT (iteration, 200);
	}
	ASSERT_EQ (wallet->free_accounts.end (), wallet->free_accounts.find (account));
	boost::property_tree::ptree request2;
	request2.put ("action", "payment_end");
	request2.put ("wallet", wallet_id.pub.to_string ());
	request2.put ("account", account.to_account ());
	test_response response2 (request2, rpc, system.service);
	while (response2.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response2.status);
	ASSERT_TRUE (wallet->exists (account));
	ASSERT_NE (wallet->free_accounts.end (), wallet->free_accounts.find (account));
	rpc.stop ();
	system.stop ();
}

TEST (rpc, payment_end_nonempty)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto node1 (system.nodes[0]);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	system.wallet (0)->init_free_accounts (paper::transaction (node1->store.environment, nullptr, false));
	auto wallet_id (node1->wallets.items.begin ()->first);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request1;
	request1.put ("action", "payment_end");
	request1.put ("wallet", wallet_id.to_string ());
	request1.put ("account", paper::test_genesis_key.pub.to_account ());
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	ASSERT_FALSE (response1.json.get<std::string> ("error").empty ());
}

TEST (rpc, payment_zero_balance)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto node1 (system.nodes[0]);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	system.wallet (0)->init_free_accounts (paper::transaction (node1->store.environment, nullptr, false));
	auto wallet_id (node1->wallets.items.begin ()->first);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request1;
	request1.put ("action", "payment_begin");
	request1.put ("wallet", wallet_id.to_string ());
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	auto account_text (response1.json.get<std::string> ("account"));
	paper::uint256_union account;
	ASSERT_FALSE (account.decode_account (account_text));
	ASSERT_NE (paper::test_genesis_key.pub, account);
}

TEST (rpc, payment_begin_reuse)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto node1 (system.nodes[0]);
	paper::keypair wallet_id;
	auto wallet (node1->wallets.create (wallet_id.pub));
	ASSERT_TRUE (node1->wallets.items.find (wallet_id.pub) != node1->wallets.items.end ());
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request1;
	request1.put ("action", "payment_begin");
	request1.put ("wallet", wallet_id.pub.to_string ());
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	auto account_text (response1.json.get<std::string> ("account"));
	paper::uint256_union account;
	ASSERT_FALSE (account.decode_account (account_text));
	ASSERT_TRUE (wallet->exists (account));
	ASSERT_EQ (wallet->free_accounts.end (), wallet->free_accounts.find (account));
	boost::property_tree::ptree request2;
	request2.put ("action", "payment_end");
	request2.put ("wallet", wallet_id.pub.to_string ());
	request2.put ("account", account.to_account ());
	test_response response2 (request2, rpc, system.service);
	while (response2.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response2.status);
	ASSERT_TRUE (wallet->exists (account));
	ASSERT_NE (wallet->free_accounts.end (), wallet->free_accounts.find (account));
	test_response response3 (request1, rpc, system.service);
	while (response3.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response3.status);
	auto account2_text (response1.json.get<std::string> ("account"));
	paper::uint256_union account2;
	ASSERT_FALSE (account2.decode_account (account2_text));
	ASSERT_EQ (account, account2);
}

TEST (rpc, payment_begin_locked)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto node1 (system.nodes[0]);
	paper::keypair wallet_id;
	auto wallet (node1->wallets.create (wallet_id.pub));
	{
		paper::transaction transaction (wallet->store.environment, nullptr, true);
		wallet->store.rekey (transaction, "1");
		ASSERT_TRUE (wallet->store.attempt_password (transaction, ""));
	}
	ASSERT_TRUE (node1->wallets.items.find (wallet_id.pub) != node1->wallets.items.end ());
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request1;
	request1.put ("action", "payment_begin");
	request1.put ("wallet", wallet_id.pub.to_string ());
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	ASSERT_FALSE (response1.json.get<std::string> ("error").empty ());
}

TEST (rpc, payment_wait)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto node1 (system.nodes[0]);
	paper::keypair key;
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key.prv);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request1;
	request1.put ("action", "payment_wait");
	request1.put ("account", key.pub.to_account ());
	request1.put ("amount", paper::amount (paper::Mxrb_ratio).to_string_dec ());
	request1.put ("timeout", "100");
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	ASSERT_EQ ("nothing", response1.json.get<std::string> ("status"));
	request1.put ("timeout", "100000");
	system.wallet (0)->send_action (paper::test_genesis_key.pub, key.pub, paper::Mxrb_ratio);
	system.alarm.add (std::chrono::steady_clock::now () + std::chrono::milliseconds (500), [&]() {
		system.wallet (0)->send_action (paper::test_genesis_key.pub, key.pub, paper::Mxrb_ratio);
	});
	test_response response2 (request1, rpc, system.service);
	while (response2.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response2.status);
	ASSERT_EQ ("success", response2.json.get<std::string> ("status"));
	request1.put ("amount", paper::amount (paper::Mxrb_ratio * 2).to_string_dec ());
	test_response response3 (request1, rpc, system.service);
	while (response3.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response3.status);
	ASSERT_EQ ("success", response2.json.get<std::string> ("status"));
}

TEST (rpc, peers)
{
	paper::system system (24000, 2);
	system.nodes[0]->peers.insert (paper::endpoint (boost::asio::ip::address_v6::from_string ("::ffff:80.80.80.80"), 4000), 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "peers");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto & peers_node (response.json.get_child ("peers"));
	ASSERT_EQ (2, peers_node.size ());
}

TEST (rpc, pending)
{
	paper::system system (24000, 1);
	paper::keypair key1;
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	auto block1 (system.wallet (0)->send_action (paper::test_genesis_key.pub, key1.pub, 100));
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "pending");
	request.put ("account", key1.pub.to_account ());
	request.put ("count", "100");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto & blocks_node (response.json.get_child ("blocks"));
	ASSERT_EQ (1, blocks_node.size ());
	paper::block_hash hash1 (blocks_node.begin ()->second.get<std::string> (""));
	ASSERT_EQ (block1->hash (), hash1);
	request.put ("threshold", "100"); // Threshold test
	test_response response0 (request, rpc, system.service);
	while (response0.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response0.status);
	blocks_node = response0.json.get_child ("blocks");
	ASSERT_EQ (1, blocks_node.size ());
	std::unordered_map<paper::block_hash, paper::uint128_union> blocks;
	for (auto i (blocks_node.begin ()), j (blocks_node.end ()); i != j; ++i)
	{
		paper::block_hash hash;
		hash.decode_hex (i->first);
		paper::uint128_union amount;
		amount.decode_dec (i->second.get<std::string> (""));
		blocks[hash] = amount;
	}
	ASSERT_EQ (blocks[block1->hash ()], 100);
	request.put ("threshold", "101");
	test_response response1 (request, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	blocks_node = response1.json.get_child ("blocks");
	ASSERT_EQ (0, blocks_node.size ());
	request.put ("threshold", "0");
	request.put ("source", "true");
	test_response response2 (request, rpc, system.service);
	while (response2.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response2.status);
	blocks_node = response2.json.get_child ("blocks");
	ASSERT_EQ (1, blocks_node.size ());
	std::unordered_map<paper::block_hash, paper::uint128_union> amounts;
	std::unordered_map<paper::block_hash, paper::account> sources;
	for (auto i (blocks_node.begin ()), j (blocks_node.end ()); i != j; ++i)
	{
		paper::block_hash hash;
		hash.decode_hex (i->first);
		amounts[hash].decode_dec (i->second.get<std::string> ("amount"));
		sources[hash].decode_account (i->second.get<std::string> ("source"));
	}
	ASSERT_EQ (amounts[block1->hash ()], 100);
	ASSERT_EQ (sources[block1->hash ()], paper::test_genesis_key.pub);
}

TEST (rpc_config, serialization)
{
	paper::rpc_config config1;
	config1.address = boost::asio::ip::address_v6::any ();
	config1.port = 10;
	config1.enable_control = true;
	config1.frontier_request_limit = 8192;
	config1.chain_request_limit = 4096;
	boost::property_tree::ptree tree;
	config1.serialize_json (tree);
	paper::rpc_config config2;
	ASSERT_NE (config2.address, config1.address);
	ASSERT_NE (config2.port, config1.port);
	ASSERT_NE (config2.enable_control, config1.enable_control);
	ASSERT_NE (config2.frontier_request_limit, config1.frontier_request_limit);
	ASSERT_NE (config2.chain_request_limit, config1.chain_request_limit);
	config2.deserialize_json (tree);
	ASSERT_EQ (config2.address, config1.address);
	ASSERT_EQ (config2.port, config1.port);
	ASSERT_EQ (config2.enable_control, config1.enable_control);
	ASSERT_EQ (config2.frontier_request_limit, config1.frontier_request_limit);
	ASSERT_EQ (config2.chain_request_limit, config1.chain_request_limit);
}

TEST (rpc, search_pending)
{
	paper::system system (24000, 1);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	auto wallet (system.nodes[0]->wallets.items.begin ()->first.to_string ());
	paper::send_block block (system.nodes[0]->latest (paper::test_genesis_key.pub), paper::test_genesis_key.pub, paper::genesis_amount - system.nodes[0]->config.receive_minimum.number (), paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, system.nodes[0]->ledger.process (paper::transaction (system.nodes[0]->store.environment, nullptr, true), block).code);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "search_pending");
	request.put ("wallet", wallet);
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto iterations (0);
	while (system.nodes[0]->balance (paper::test_genesis_key.pub) != paper::genesis_amount)
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
}

TEST (rpc, version)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto node1 (system.nodes[0]);
	paper::keypair key;
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key.prv);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request1;
	request1.put ("action", "version");
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	ASSERT_EQ ("1", response1.json.get<std::string> ("rpc_version"));
	ASSERT_EQ (200, response1.status);
	ASSERT_EQ ("10", response1.json.get<std::string> ("store_version"));
	ASSERT_EQ (boost::str (boost::format ("Paper %1%.%2%") % PAPER_VERSION_MAJOR % PAPER_VERSION_MINOR), response1.json.get<std::string> ("node_vendor"));
	auto headers (response1.resp.base ());
	auto allowed_origin (headers.at ("Access-Control-Allow-Origin"));
	auto allowed_headers (headers.at ("Access-Control-Allow-Headers"));
	ASSERT_EQ ("*", allowed_origin);
	ASSERT_EQ ("Accept, Accept-Language, Content-Language, Content-Type", allowed_headers);
}

TEST (rpc, work_generate)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto node1 (system.nodes[0]);
	paper::keypair key;
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key.prv);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	paper::block_hash hash1 (1);
	boost::property_tree::ptree request1;
	request1.put ("action", "work_generate");
	request1.put ("hash", hash1.to_string ());
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	auto work1 (response1.json.get<std::string> ("work"));
	uint64_t work2;
	ASSERT_FALSE (paper::from_string_hex (work1, work2));
	ASSERT_FALSE (paper::work_validate (hash1, work2));
}

TEST (rpc, work_cancel)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto & node1 (*system.nodes[0]);
	paper::keypair key;
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key.prv);
	paper::rpc rpc (system.service, node1, paper::rpc_config (true));
	rpc.start ();
	paper::block_hash hash1 (1);
	boost::property_tree::ptree request1;
	request1.put ("action", "work_cancel");
	request1.put ("hash", hash1.to_string ());
	boost::optional<uint64_t> work;
	std::thread thread ([&]() {
		work = system.work.generate (hash1);
	});
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	thread.join ();
}

TEST (rpc, work_peer_bad)
{
	paper::system system (24000, 2);
	paper::node_init init1;
	auto & node1 (*system.nodes[0]);
	auto & node2 (*system.nodes[1]);
	paper::keypair key;
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key.prv);
	paper::rpc rpc (system.service, node1, paper::rpc_config (true));
	rpc.start ();
	node2.config.work_peers.push_back (std::make_pair (boost::asio::ip::address_v6::any (), 0));
	paper::block_hash hash1 (1);
	std::atomic<uint64_t> work (0);
	node2.generate_work (hash1, [&work](uint64_t work_a) {
		work = work_a;
	});
	while (paper::work_validate (hash1, work))
	{
		system.poll ();
	}
}

TEST (rpc, work_peer_one)
{
	paper::system system (24000, 2);
	paper::node_init init1;
	auto & node1 (*system.nodes[0]);
	auto & node2 (*system.nodes[1]);
	paper::keypair key;
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key.prv);
	paper::rpc rpc (system.service, node1, paper::rpc_config (true));
	rpc.start ();
	node2.config.work_peers.push_back (std::make_pair (node1.network.endpoint ().address (), rpc.config.port));
	paper::keypair key1;
	uint64_t work (0);
	node2.generate_work (key1.pub, [&work](uint64_t work_a) {
		work = work_a;
	});
	while (paper::work_validate (key1.pub, work))
	{
		system.poll ();
	}
}

TEST (rpc, work_peer_many)
{
	paper::system system1 (24000, 1);
	paper::system system2 (24001, 1);
	paper::system system3 (24002, 1);
	paper::system system4 (24003, 1);
	paper::node_init init1;
	auto & node1 (*system1.nodes[0]);
	auto & node2 (*system2.nodes[0]);
	auto & node3 (*system3.nodes[0]);
	auto & node4 (*system4.nodes[0]);
	paper::keypair key;
	paper::rpc_config config2 (true);
	config2.port += 0;
	paper::rpc rpc2 (system2.service, node2, config2);
	rpc2.start ();
	paper::rpc_config config3 (true);
	config3.port += 1;
	paper::rpc rpc3 (system3.service, node3, config3);
	rpc3.start ();
	paper::rpc_config config4 (true);
	config4.port += 2;
	paper::rpc rpc4 (system4.service, node4, config4);
	rpc4.start ();
	node1.config.work_peers.push_back (std::make_pair (node2.network.endpoint ().address (), rpc2.config.port));
	node1.config.work_peers.push_back (std::make_pair (node3.network.endpoint ().address (), rpc3.config.port));
	node1.config.work_peers.push_back (std::make_pair (node4.network.endpoint ().address (), rpc4.config.port));
	for (auto i (0); i < 10; ++i)
	{
		paper::keypair key1;
		uint64_t work (0);
		node1.generate_work (key1.pub, [&work](uint64_t work_a) {
			work = work_a;
		});
		while (paper::work_validate (key1.pub, work))
		{
			system1.poll ();
			system2.poll ();
			system3.poll ();
			system4.poll ();
		}
	}
}

TEST (rpc, block_count)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto & node1 (*system.nodes[0]);
	paper::rpc rpc (system.service, node1, paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request1;
	request1.put ("action", "block_count");
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	ASSERT_EQ ("1", response1.json.get<std::string> ("count"));
	ASSERT_EQ ("0", response1.json.get<std::string> ("unchecked"));
}

TEST (rpc, frontier_count)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto & node1 (*system.nodes[0]);
	paper::rpc rpc (system.service, node1, paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request1;
	request1.put ("action", "frontier_count");
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	ASSERT_EQ ("1", response1.json.get<std::string> ("count"));
}

TEST (rpc, available_supply)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto & node1 (*system.nodes[0]);
	paper::rpc rpc (system.service, node1, paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request1;
	request1.put ("action", "available_supply");
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	ASSERT_EQ ("0", response1.json.get<std::string> ("available"));
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	paper::keypair key;
	auto block (system.wallet (0)->send_action (paper::test_genesis_key.pub, key.pub, 1));
	test_response response2 (request1, rpc, system.service);
	while (response2.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response2.status);
	ASSERT_EQ ("1", response2.json.get<std::string> ("available"));
	auto block2 (system.wallet (0)->send_action (paper::test_genesis_key.pub, 0, 100)); // Sending to burning 0 account
	test_response response3 (request1, rpc, system.service);
	while (response3.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response3.status);
	ASSERT_EQ ("1", response3.json.get<std::string> ("available"));
}

TEST (rpc, mpaper_to_raw)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto & node1 (*system.nodes[0]);
	paper::rpc rpc (system.service, node1, paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request1;
	request1.put ("action", "mpaper_to_raw");
	request1.put ("amount", "1");
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	ASSERT_EQ (paper::Mxrb_ratio.convert_to<std::string> (), response1.json.get<std::string> ("amount"));
}

TEST (rpc, mpaper_from_raw)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto & node1 (*system.nodes[0]);
	paper::rpc rpc (system.service, node1, paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request1;
	request1.put ("action", "mpaper_from_raw");
	request1.put ("amount", paper::Mxrb_ratio.convert_to<std::string> ());
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	ASSERT_EQ ("1", response1.json.get<std::string> ("amount"));
}

TEST (rpc, kpaper_to_raw)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto & node1 (*system.nodes[0]);
	paper::rpc rpc (system.service, node1, paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request1;
	request1.put ("action", "kpaper_to_raw");
	request1.put ("amount", "1");
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	ASSERT_EQ (paper::kxrb_ratio.convert_to<std::string> (), response1.json.get<std::string> ("amount"));
}

TEST (rpc, kpaper_from_raw)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto & node1 (*system.nodes[0]);
	paper::rpc rpc (system.service, node1, paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request1;
	request1.put ("action", "kpaper_from_raw");
	request1.put ("amount", paper::kxrb_ratio.convert_to<std::string> ());
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	ASSERT_EQ ("1", response1.json.get<std::string> ("amount"));
}

TEST (rpc, paper_to_raw)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto & node1 (*system.nodes[0]);
	paper::rpc rpc (system.service, node1, paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request1;
	request1.put ("action", "paper_to_raw");
	request1.put ("amount", "1");
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	ASSERT_EQ (paper::xrb_ratio.convert_to<std::string> (), response1.json.get<std::string> ("amount"));
}

TEST (rpc, paper_from_raw)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto & node1 (*system.nodes[0]);
	paper::rpc rpc (system.service, node1, paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request1;
	request1.put ("action", "paper_from_raw");
	request1.put ("amount", paper::xrb_ratio.convert_to<std::string> ());
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	ASSERT_EQ ("1", response1.json.get<std::string> ("amount"));
}

TEST (rpc, account_representative)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	std::string wallet;
	request.put ("account", paper::genesis_account.to_account ());
	request.put ("action", "account_representative");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string account_text1 (response.json.get<std::string> ("representative"));
	ASSERT_EQ (account_text1, paper::genesis_account.to_account ());
}

TEST (rpc, account_representative_set)
{
	paper::system system (24000, 1);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	paper::keypair rep;
	request.put ("account", paper::genesis_account.to_account ());
	request.put ("representative", rep.pub.to_account ());
	request.put ("wallet", system.nodes[0]->wallets.items.begin ()->first.to_string ());
	request.put ("action", "account_representative_set");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string block_text1 (response.json.get<std::string> ("block"));
	paper::block_hash hash;
	ASSERT_FALSE (hash.decode_hex (block_text1));
	ASSERT_FALSE (hash.is_zero ());
	paper::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	ASSERT_TRUE (system.nodes[0]->store.block_exists (transaction, hash));
	ASSERT_EQ (rep.pub, system.nodes[0]->store.block_get (transaction, hash)->representative ());
}

TEST (rpc, bootstrap)
{
	paper::system system0 (24000, 1);
	paper::system system1 (24001, 1);
	auto latest (system1.nodes[0]->latest (paper::test_genesis_key.pub));
	paper::send_block send (latest, paper::genesis_account, 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, system1.nodes[0]->generate_work (latest));
	{
		paper::transaction transaction (system1.nodes[0]->store.environment, nullptr, true);
		ASSERT_EQ (paper::process_result::progress, system1.nodes[0]->ledger.process (transaction, send).code);
	}
	paper::rpc rpc (system0.service, *system0.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "bootstrap");
	request.put ("address", "::ffff:127.0.0.1");
	request.put ("port", system1.nodes[0]->network.endpoint ().port ());
	test_response response (request, rpc, system0.service);
	while (response.status == 0)
	{
		system0.poll ();
	}
	auto iterations (0);
	while (system0.nodes[0]->latest (paper::genesis_account) != system1.nodes[0]->latest (paper::genesis_account))
	{
		system0.poll ();
		system1.poll ();
		++iterations;
		ASSERT_GT (200, iterations);
	}
}

TEST (rpc, account_remove)
{
	paper::system system0 (24000, 1);
	auto key1 (system0.wallet (0)->deterministic_insert ());
	ASSERT_TRUE (system0.wallet (0)->exists (key1));
	paper::rpc rpc (system0.service, *system0.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "account_remove");
	request.put ("wallet", system0.nodes[0]->wallets.items.begin ()->first.to_string ());
	request.put ("account", key1.to_account ());
	test_response response (request, rpc, system0.service);
	while (response.status == 0)
	{
		system0.poll ();
	}
	ASSERT_FALSE (system0.wallet (0)->exists (key1));
}

TEST (rpc, representatives)
{
	paper::system system0 (24000, 1);
	paper::rpc rpc (system0.service, *system0.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "representatives");
	test_response response (request, rpc, system0.service);
	while (response.status == 0)
	{
		system0.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto & representatives_node (response.json.get_child ("representatives"));
	std::vector<paper::account> representatives;
	for (auto i (representatives_node.begin ()), n (representatives_node.end ()); i != n; ++i)
	{
		paper::account account;
		ASSERT_FALSE (account.decode_account (i->first));
		representatives.push_back (account);
	}
	ASSERT_EQ (1, representatives.size ());
	ASSERT_EQ (paper::genesis_account, representatives[0]);
}

TEST (rpc, wallet_change_seed)
{
	paper::system system0 (24000, 1);
	paper::keypair seed;
	{
		paper::transaction transaction (system0.nodes[0]->store.environment, nullptr, false);
		paper::raw_key seed0;
		system0.wallet (0)->store.seed (seed0, transaction);
		ASSERT_NE (seed.pub, seed0.data);
	}
	paper::rpc rpc (system0.service, *system0.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "wallet_change_seed");
	request.put ("wallet", system0.nodes[0]->wallets.items.begin ()->first.to_string ());
	request.put ("seed", seed.pub.to_string ());
	test_response response (request, rpc, system0.service);
	while (response.status == 0)
	{
		system0.poll ();
	}
	ASSERT_EQ (200, response.status);
	{
		paper::transaction transaction (system0.nodes[0]->store.environment, nullptr, false);
		paper::raw_key seed0;
		system0.wallet (0)->store.seed (seed0, transaction);
		ASSERT_EQ (seed.pub, seed0.data);
	}
}

TEST (rpc, wallet_frontiers)
{
	paper::system system0 (24000, 1);
	system0.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	paper::rpc rpc (system0.service, *system0.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "wallet_frontiers");
	request.put ("wallet", system0.nodes[0]->wallets.items.begin ()->first.to_string ());
	test_response response (request, rpc, system0.service);
	while (response.status == 0)
	{
		system0.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto & frontiers_node (response.json.get_child ("frontiers"));
	std::vector<paper::account> frontiers;
	for (auto i (frontiers_node.begin ()), n (frontiers_node.end ()); i != n; ++i)
	{
		frontiers.push_back (paper::block_hash (i->second.get<std::string> ("")));
	}
	ASSERT_EQ (1, frontiers.size ());
	ASSERT_EQ (system0.nodes[0]->latest (paper::genesis_account), frontiers[0]);
}

TEST (rpc, work_validate)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto & node1 (*system.nodes[0]);
	paper::keypair key;
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key.prv);
	paper::rpc rpc (system.service, node1, paper::rpc_config (true));
	rpc.start ();
	paper::block_hash hash (1);
	uint64_t work1 (node1.generate_work (hash));
	boost::property_tree::ptree request;
	request.put ("action", "work_validate");
	request.put ("hash", hash.to_string ());
	request.put ("work", paper::to_string_hex (work1));
	test_response response1 (request, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	std::string validate_text1 (response1.json.get<std::string> ("valid"));
	ASSERT_EQ ("1", validate_text1);
	uint64_t work2 (0);
	request.put ("work", paper::to_string_hex (work2));
	test_response response2 (request, rpc, system.service);
	while (response2.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response2.status);
	std::string validate_text2 (response2.json.get<std::string> ("valid"));
	ASSERT_EQ ("0", validate_text2);
}

TEST (rpc, successors)
{
	paper::system system (24000, 1);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	paper::keypair key;
	auto genesis (system.nodes[0]->latest (paper::test_genesis_key.pub));
	ASSERT_FALSE (genesis.is_zero ());
	auto block (system.wallet (0)->send_action (paper::test_genesis_key.pub, key.pub, 1));
	ASSERT_NE (nullptr, block);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "successors");
	request.put ("block", genesis.to_string ());
	request.put ("count", std::to_string (std::numeric_limits<uint64_t>::max ()));
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto & blocks_node (response.json.get_child ("blocks"));
	std::vector<paper::block_hash> blocks;
	for (auto i (blocks_node.begin ()), n (blocks_node.end ()); i != n; ++i)
	{
		blocks.push_back (paper::block_hash (i->second.get<std::string> ("")));
	}
	ASSERT_EQ (2, blocks.size ());
	ASSERT_EQ (genesis, blocks[0]);
	ASSERT_EQ (block->hash (), blocks[1]);
}

TEST (rpc, bootstrap_any)
{
	paper::system system0 (24000, 1);
	paper::system system1 (24001, 1);
	auto latest (system1.nodes[0]->latest (paper::test_genesis_key.pub));
	paper::send_block send (latest, paper::genesis_account, 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, system1.nodes[0]->generate_work (latest));
	{
		paper::transaction transaction (system1.nodes[0]->store.environment, nullptr, true);
		ASSERT_EQ (paper::process_result::progress, system1.nodes[0]->ledger.process (transaction, send).code);
	}
	paper::rpc rpc (system0.service, *system0.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "bootstrap_any");
	test_response response (request, rpc, system0.service);
	while (response.status == 0)
	{
		system0.poll ();
	}
	std::string success (response.json.get<std::string> ("success"));
	ASSERT_TRUE (success.empty ());
}

TEST (rpc, republish)
{
	paper::system system (24000, 2);
	paper::keypair key;
	paper::genesis genesis;
	auto latest (system.nodes[0]->latest (paper::test_genesis_key.pub));
	auto & node1 (*system.nodes[0]);
	paper::send_block send (latest, key.pub, 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, node1.generate_work (latest));
	system.nodes[0]->process (send);
	paper::open_block open (send.hash (), key.pub, key.pub, key.prv, key.pub, node1.generate_work (key.pub));
	ASSERT_EQ (paper::process_result::progress, system.nodes[0]->process (open).code);
	paper::rpc rpc (system.service, node1, paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "republish");
	request.put ("hash", send.hash ().to_string ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto iterations (0);
	while (system.nodes[1]->balance (paper::test_genesis_key.pub) == paper::genesis_amount)
	{
		system.poll ();
		++iterations;
		ASSERT_GT (200, iterations);
	}
	auto & blocks_node (response.json.get_child ("blocks"));
	std::vector<paper::block_hash> blocks;
	for (auto i (blocks_node.begin ()), n (blocks_node.end ()); i != n; ++i)
	{
		blocks.push_back (paper::block_hash (i->second.get<std::string> ("")));
	}
	ASSERT_EQ (1, blocks.size ());
	ASSERT_EQ (send.hash (), blocks[0]);

	request.put ("hash", genesis.hash ().to_string ());
	request.put ("count", 1);
	test_response response1 (request, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	blocks_node = response1.json.get_child ("blocks");
	blocks.clear ();
	for (auto i (blocks_node.begin ()), n (blocks_node.end ()); i != n; ++i)
	{
		blocks.push_back (paper::block_hash (i->second.get<std::string> ("")));
	}
	ASSERT_EQ (1, blocks.size ());
	ASSERT_EQ (genesis.hash (), blocks[0]);

	request.put ("hash", open.hash ().to_string ());
	request.put ("sources", 2);
	test_response response2 (request, rpc, system.service);
	while (response2.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response2.status);
	blocks_node = response2.json.get_child ("blocks");
	blocks.clear ();
	for (auto i (blocks_node.begin ()), n (blocks_node.end ()); i != n; ++i)
	{
		blocks.push_back (paper::block_hash (i->second.get<std::string> ("")));
	}
	ASSERT_EQ (3, blocks.size ());
	ASSERT_EQ (genesis.hash (), blocks[0]);
	ASSERT_EQ (send.hash (), blocks[1]);
	ASSERT_EQ (open.hash (), blocks[2]);
}

TEST (rpc, deterministic_key)
{
	paper::system system0 (24000, 1);
	paper::raw_key seed;
	{
		paper::transaction transaction (system0.nodes[0]->store.environment, nullptr, false);
		system0.wallet (0)->store.seed (seed, transaction);
	}
	paper::account account0 (system0.wallet (0)->deterministic_insert ());
	paper::account account1 (system0.wallet (0)->deterministic_insert ());
	paper::account account2 (system0.wallet (0)->deterministic_insert ());
	paper::rpc rpc (system0.service, *system0.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "deterministic_key");
	request.put ("seed", seed.data.to_string ());
	request.put ("index", "0");
	test_response response0 (request, rpc, system0.service);
	while (response0.status == 0)
	{
		system0.poll ();
	}
	ASSERT_EQ (200, response0.status);
	std::string validate_text (response0.json.get<std::string> ("account"));
	ASSERT_EQ (account0.to_account (), validate_text);
	request.put ("index", "2");
	test_response response1 (request, rpc, system0.service);
	while (response1.status == 0)
	{
		system0.poll ();
	}
	ASSERT_EQ (200, response1.status);
	validate_text = response1.json.get<std::string> ("account");
	ASSERT_NE (account1.to_account (), validate_text);
	ASSERT_EQ (account2.to_account (), validate_text);
}

TEST (rpc, accounts_balances)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "accounts_balances");
	boost::property_tree::ptree entry;
	boost::property_tree::ptree peers_l;
	entry.put ("", paper::test_genesis_key.pub.to_account ());
	peers_l.push_back (std::make_pair ("", entry));
	request.add_child ("accounts", peers_l);
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	for (auto & balances : response.json.get_child ("balances"))
	{
		std::string account_text (balances.first);
		ASSERT_EQ (paper::test_genesis_key.pub.to_account (), account_text);
		std::string balance_text (balances.second.get<std::string> ("balance"));
		ASSERT_EQ ("340282366920938463463374607431768211455", balance_text);
		std::string pending_text (balances.second.get<std::string> ("pending"));
		ASSERT_EQ ("0", pending_text);
	}
}

TEST (rpc, accounts_frontiers)
{
	paper::system system (24000, 1);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "accounts_frontiers");
	boost::property_tree::ptree entry;
	boost::property_tree::ptree peers_l;
	entry.put ("", paper::test_genesis_key.pub.to_account ());
	peers_l.push_back (std::make_pair ("", entry));
	request.add_child ("accounts", peers_l);
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	for (auto & frontiers : response.json.get_child ("frontiers"))
	{
		std::string account_text (frontiers.first);
		ASSERT_EQ (paper::test_genesis_key.pub.to_account (), account_text);
		std::string frontier_text (frontiers.second.get<std::string> (""));
		ASSERT_EQ (system.nodes[0]->latest (paper::genesis_account), frontier_text);
	}
}

TEST (rpc, accounts_pending)
{
	paper::system system (24000, 1);
	paper::keypair key1;
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	auto block1 (system.wallet (0)->send_action (paper::test_genesis_key.pub, key1.pub, 100));
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "accounts_pending");
	boost::property_tree::ptree entry;
	boost::property_tree::ptree peers_l;
	entry.put ("", key1.pub.to_account ());
	peers_l.push_back (std::make_pair ("", entry));
	request.add_child ("accounts", peers_l);
	request.put ("count", "100");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	for (auto & blocks : response.json.get_child ("blocks"))
	{
		std::string account_text (blocks.first);
		ASSERT_EQ (key1.pub.to_account (), account_text);
		paper::block_hash hash1 (blocks.second.begin ()->second.get<std::string> (""));
		ASSERT_EQ (block1->hash (), hash1);
	}
	request.put ("threshold", "100"); // Threshold test
	test_response response1 (request, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	std::unordered_map<paper::block_hash, paper::uint128_union> blocks;
	for (auto & pending : response1.json.get_child ("blocks"))
	{
		std::string account_text (pending.first);
		ASSERT_EQ (key1.pub.to_account (), account_text);
		for (auto i (pending.second.begin ()), j (pending.second.end ()); i != j; ++i)
		{
			paper::block_hash hash;
			hash.decode_hex (i->first);
			paper::uint128_union amount;
			amount.decode_dec (i->second.get<std::string> (""));
			blocks[hash] = amount;
		}
	}
	ASSERT_EQ (blocks[block1->hash ()], 100);
	request.put ("source", "true");
	test_response response2 (request, rpc, system.service);
	while (response2.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response2.status);
	std::unordered_map<paper::block_hash, paper::uint128_union> amounts;
	std::unordered_map<paper::block_hash, paper::account> sources;
	for (auto & pending : response2.json.get_child ("blocks"))
	{
		std::string account_text (pending.first);
		ASSERT_EQ (key1.pub.to_account (), account_text);
		for (auto i (pending.second.begin ()), j (pending.second.end ()); i != j; ++i)
		{
			paper::block_hash hash;
			hash.decode_hex (i->first);
			amounts[hash].decode_dec (i->second.get<std::string> ("amount"));
			sources[hash].decode_account (i->second.get<std::string> ("source"));
		}
	}
	ASSERT_EQ (amounts[block1->hash ()], 100);
	ASSERT_EQ (sources[block1->hash ()], paper::test_genesis_key.pub);
}

TEST (rpc, blocks)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "blocks");
	boost::property_tree::ptree entry;
	boost::property_tree::ptree peers_l;
	entry.put ("", system.nodes[0]->latest (paper::genesis_account).to_string ());
	peers_l.push_back (std::make_pair ("", entry));
	request.add_child ("hashes", peers_l);
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	for (auto & blocks : response.json.get_child ("blocks"))
	{
		std::string hash_text (blocks.first);
		ASSERT_EQ (system.nodes[0]->latest (paper::genesis_account).to_string (), hash_text);
		std::string blocks_text (blocks.second.get<std::string> (""));
		ASSERT_FALSE (blocks_text.empty ());
	}
}

TEST (rpc, wallet_balance_total)
{
	paper::system system (24000, 1);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	paper::keypair key;
	system.wallet (0)->insert_adhoc (key.prv);
	auto send (system.wallet (0)->send_action (paper::test_genesis_key.pub, key.pub, 1));
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "wallet_balance_total");
	request.put ("wallet", system.nodes[0]->wallets.items.begin ()->first.to_string ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string balance_text (response.json.get<std::string> ("balance"));
	ASSERT_EQ ("340282366920938463463374607431768211454", balance_text);
	std::string pending_text (response.json.get<std::string> ("pending"));
	ASSERT_EQ ("1", pending_text);
}

TEST (rpc, wallet_balances)
{
	paper::system system0 (24000, 1);
	system0.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	paper::rpc rpc (system0.service, *system0.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "wallet_balances");
	request.put ("wallet", system0.nodes[0]->wallets.items.begin ()->first.to_string ());
	test_response response (request, rpc, system0.service);
	while (response.status == 0)
	{
		system0.poll ();
	}
	ASSERT_EQ (200, response.status);
	for (auto & balances : response.json.get_child ("balances"))
	{
		std::string account_text (balances.first);
		ASSERT_EQ (paper::test_genesis_key.pub.to_account (), account_text);
		std::string balance_text (balances.second.get<std::string> ("balance"));
		ASSERT_EQ ("340282366920938463463374607431768211455", balance_text);
		std::string pending_text (balances.second.get<std::string> ("pending"));
		ASSERT_EQ ("0", pending_text);
	}
	paper::keypair key;
	system0.wallet (0)->insert_adhoc (key.prv);
	auto send (system0.wallet (0)->send_action (paper::test_genesis_key.pub, key.pub, 1));
	request.put ("threshold", "2");
	test_response response1 (request, rpc, system0.service);
	while (response1.status == 0)
	{
		system0.poll ();
	}
	ASSERT_EQ (200, response1.status);
	for (auto & balances : response1.json.get_child ("balances"))
	{
		std::string account_text (balances.first);
		ASSERT_EQ (paper::test_genesis_key.pub.to_account (), account_text);
		std::string balance_text (balances.second.get<std::string> ("balance"));
		ASSERT_EQ ("340282366920938463463374607431768211454", balance_text);
		std::string pending_text (balances.second.get<std::string> ("pending"));
		ASSERT_EQ ("0", pending_text);
	}
}

TEST (rpc, pending_exists)
{
	paper::system system (24000, 1);
	paper::keypair key1;
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	auto hash0 (system.nodes[0]->latest (paper::genesis_account));
	auto block1 (system.wallet (0)->send_action (paper::test_genesis_key.pub, key1.pub, 100));
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "pending_exists");
	request.put ("hash", hash0.to_string ());
	test_response response0 (request, rpc, system.service);
	while (response0.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response0.status);
	std::string exists_text (response0.json.get<std::string> ("exists"));
	ASSERT_EQ ("0", exists_text);
	request.put ("hash", block1->hash ().to_string ());
	test_response response1 (request, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	std::string exists_text1 (response1.json.get<std::string> ("exists"));
	ASSERT_EQ ("1", exists_text1);
}

TEST (rpc, wallet_pending)
{
	paper::system system0 (24000, 1);
	paper::keypair key1;
	system0.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	system0.wallet (0)->insert_adhoc (key1.prv);
	auto block1 (system0.wallet (0)->send_action (paper::test_genesis_key.pub, key1.pub, 100));
	paper::rpc rpc (system0.service, *system0.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "wallet_pending");
	request.put ("wallet", system0.nodes[0]->wallets.items.begin ()->first.to_string ());
	request.put ("count", "100");
	test_response response (request, rpc, system0.service);
	while (response.status == 0)
	{
		system0.poll ();
	}
	ASSERT_EQ (200, response.status);
	for (auto & pending : response.json.get_child ("blocks"))
	{
		std::string account_text (pending.first);
		ASSERT_EQ (key1.pub.to_account (), account_text);
		paper::block_hash hash1 (pending.second.begin ()->second.get<std::string> (""));
		ASSERT_EQ (block1->hash (), hash1);
	}
	request.put ("threshold", "100"); // Threshold test
	test_response response0 (request, rpc, system0.service);
	while (response0.status == 0)
	{
		system0.poll ();
	}
	ASSERT_EQ (200, response0.status);
	std::unordered_map<paper::block_hash, paper::uint128_union> blocks;
	for (auto & pending : response0.json.get_child ("blocks"))
	{
		std::string account_text (pending.first);
		ASSERT_EQ (key1.pub.to_account (), account_text);
		for (auto i (pending.second.begin ()), j (pending.second.end ()); i != j; ++i)
		{
			paper::block_hash hash;
			hash.decode_hex (i->first);
			paper::uint128_union amount;
			amount.decode_dec (i->second.get<std::string> (""));
			blocks[hash] = amount;
		}
	}
	ASSERT_EQ (blocks[block1->hash ()], 100);
	request.put ("threshold", "101");
	test_response response1 (request, rpc, system0.service);
	while (response1.status == 0)
	{
		system0.poll ();
	}
	ASSERT_EQ (200, response1.status);
	auto & pending1 (response1.json.get_child ("blocks"));
	ASSERT_EQ (0, pending1.size ());
	request.put ("threshold", "0");
	request.put ("source", "true");
	test_response response2 (request, rpc, system0.service);
	while (response2.status == 0)
	{
		system0.poll ();
	}
	ASSERT_EQ (200, response2.status);
	std::unordered_map<paper::block_hash, paper::uint128_union> amounts;
	std::unordered_map<paper::block_hash, paper::account> sources;
	for (auto & pending : response2.json.get_child ("blocks"))
	{
		std::string account_text (pending.first);
		ASSERT_EQ (key1.pub.to_account (), account_text);
		for (auto i (pending.second.begin ()), j (pending.second.end ()); i != j; ++i)
		{
			paper::block_hash hash;
			hash.decode_hex (i->first);
			amounts[hash].decode_dec (i->second.get<std::string> ("amount"));
			sources[hash].decode_account (i->second.get<std::string> ("source"));
		}
	}
	ASSERT_EQ (amounts[block1->hash ()], 100);
	ASSERT_EQ (sources[block1->hash ()], paper::test_genesis_key.pub);
}

TEST (rpc, receive_minimum)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "receive_minimum");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string amount (response.json.get<std::string> ("amount"));
	ASSERT_EQ (system.nodes[0]->config.receive_minimum.to_string_dec (), amount);
}

TEST (rpc, receive_minimum_set)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "receive_minimum_set");
	request.put ("amount", "100");
	ASSERT_NE (system.nodes[0]->config.receive_minimum.to_string_dec (), "100");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string success (response.json.get<std::string> ("success"));
	ASSERT_TRUE (success.empty ());
	ASSERT_EQ (system.nodes[0]->config.receive_minimum.to_string_dec (), "100");
}

TEST (rpc, work_get)
{
	paper::system system (24000, 1);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "work_get");
	request.put ("wallet", system.nodes[0]->wallets.items.begin ()->first.to_string ());
	request.put ("account", paper::test_genesis_key.pub.to_account ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string work_text (response.json.get<std::string> ("work"));
	uint64_t work (1);
	paper::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	system.nodes[0]->wallets.items.begin ()->second->store.work_get (transaction, paper::genesis_account, work);
	ASSERT_EQ (paper::to_string_hex (work), work_text);
}

TEST (rpc, wallet_work_get)
{
	paper::system system (24000, 1);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "wallet_work_get");
	request.put ("wallet", system.nodes[0]->wallets.items.begin ()->first.to_string ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	paper::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	for (auto & works : response.json.get_child ("works"))
	{
		std::string account_text (works.first);
		ASSERT_EQ (paper::test_genesis_key.pub.to_account (), account_text);
		std::string work_text (works.second.get<std::string> (""));
		uint64_t work (1);
		system.nodes[0]->wallets.items.begin ()->second->store.work_get (transaction, paper::genesis_account, work);
		ASSERT_EQ (paper::to_string_hex (work), work_text);
	}
}

TEST (rpc, work_set)
{
	paper::system system (24000, 1);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	uint64_t work0 (100);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "work_set");
	request.put ("wallet", system.nodes[0]->wallets.items.begin ()->first.to_string ());
	request.put ("account", paper::test_genesis_key.pub.to_account ());
	request.put ("work", paper::to_string_hex (work0));
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string success (response.json.get<std::string> ("success"));
	ASSERT_TRUE (success.empty ());
	uint64_t work1 (1);
	paper::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	system.nodes[0]->wallets.items.begin ()->second->store.work_get (transaction, paper::genesis_account, work1);
	ASSERT_EQ (work1, work0);
}

TEST (rpc, search_pending_all)
{
	paper::system system (24000, 1);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	paper::send_block block (system.nodes[0]->latest (paper::test_genesis_key.pub), paper::test_genesis_key.pub, paper::genesis_amount - system.nodes[0]->config.receive_minimum.number (), paper::test_genesis_key.prv, paper::test_genesis_key.pub, 0);
	ASSERT_EQ (paper::process_result::progress, system.nodes[0]->ledger.process (paper::transaction (system.nodes[0]->store.environment, nullptr, true), block).code);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "search_pending_all");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto iterations (0);
	while (system.nodes[0]->balance (paper::test_genesis_key.pub) != paper::genesis_amount)
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
}

TEST (rpc, wallet_republish)
{
	paper::system system (24000, 1);
	paper::genesis genesis;
	paper::keypair key;
	while (key.pub < paper::test_genesis_key.pub)
	{
		paper::keypair key1;
		key.pub = key1.pub;
		key.prv.data = key1.prv.data;
	}
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key.prv);
	auto & node1 (*system.nodes[0]);
	auto latest (system.nodes[0]->latest (paper::test_genesis_key.pub));
	paper::send_block send (latest, key.pub, 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, node1.generate_work (latest));
	system.nodes[0]->process (send);
	paper::open_block open (send.hash (), key.pub, key.pub, key.prv, key.pub, node1.generate_work (key.pub));
	ASSERT_EQ (paper::process_result::progress, system.nodes[0]->process (open).code);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "wallet_republish");
	request.put ("wallet", system.nodes[0]->wallets.items.begin ()->first.to_string ());
	request.put ("count", 1);
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto & blocks_node (response.json.get_child ("blocks"));
	std::vector<paper::block_hash> blocks;
	for (auto i (blocks_node.begin ()), n (blocks_node.end ()); i != n; ++i)
	{
		blocks.push_back (paper::block_hash (i->second.get<std::string> ("")));
	}
	ASSERT_EQ (2, blocks.size ());
	ASSERT_EQ (send.hash (), blocks[0]);
	ASSERT_EQ (open.hash (), blocks[1]);
}

TEST (rpc, delegators)
{
	paper::system system (24000, 1);
	paper::keypair key;
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key.prv);
	auto & node1 (*system.nodes[0]);
	auto latest (system.nodes[0]->latest (paper::test_genesis_key.pub));
	paper::send_block send (latest, key.pub, 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, node1.generate_work (latest));
	system.nodes[0]->process (send);
	paper::open_block open (send.hash (), paper::test_genesis_key.pub, key.pub, key.prv, key.pub, node1.generate_work (key.pub));
	ASSERT_EQ (paper::process_result::progress, system.nodes[0]->process (open).code);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "delegators");
	request.put ("account", paper::test_genesis_key.pub.to_account ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto & delegators_node (response.json.get_child ("delegators"));
	boost::property_tree::ptree delegators;
	for (auto i (delegators_node.begin ()), n (delegators_node.end ()); i != n; ++i)
	{
		delegators.put ((i->first), (i->second.get<std::string> ("")));
	}
	ASSERT_EQ (2, delegators.size ());
	ASSERT_EQ ("100", delegators.get<std::string> (paper::test_genesis_key.pub.to_account ()));
	ASSERT_EQ ("340282366920938463463374607431768211355", delegators.get<std::string> (key.pub.to_account ()));
}

TEST (rpc, delegators_count)
{
	paper::system system (24000, 1);
	paper::keypair key;
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key.prv);
	auto & node1 (*system.nodes[0]);
	auto latest (system.nodes[0]->latest (paper::test_genesis_key.pub));
	paper::send_block send (latest, key.pub, 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, node1.generate_work (latest));
	system.nodes[0]->process (send);
	paper::open_block open (send.hash (), paper::test_genesis_key.pub, key.pub, key.prv, key.pub, node1.generate_work (key.pub));
	ASSERT_EQ (paper::process_result::progress, system.nodes[0]->process (open).code);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "delegators_count");
	request.put ("account", paper::test_genesis_key.pub.to_account ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string count (response.json.get<std::string> ("count"));
	ASSERT_EQ ("2", count);
}

TEST (rpc, account_info)
{
	paper::system system (24000, 1);
	paper::keypair key;
	paper::genesis genesis;
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key.prv);
	auto & node1 (*system.nodes[0]);
	auto latest (system.nodes[0]->latest (paper::test_genesis_key.pub));
	paper::send_block send (latest, key.pub, 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, node1.generate_work (latest));
	system.nodes[0]->process (send);
	auto time (paper::seconds_since_epoch ());

	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "account_info");
	request.put ("account", paper::test_genesis_key.pub.to_account ());
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string frontier (response.json.get<std::string> ("frontier"));
	ASSERT_EQ (send.hash ().to_string (), frontier);
	std::string open_block (response.json.get<std::string> ("open_block"));
	ASSERT_EQ (genesis.hash ().to_string (), open_block);
	std::string representative_block (response.json.get<std::string> ("representative_block"));
	ASSERT_EQ (genesis.hash ().to_string (), representative_block);
	std::string balance (response.json.get<std::string> ("balance"));
	ASSERT_EQ ("100", balance);
	std::string modified_timestamp (response.json.get<std::string> ("modified_timestamp"));
	ASSERT_TRUE (time - stol (modified_timestamp) < 5);
	std::string block_count (response.json.get<std::string> ("block_count"));
	ASSERT_EQ ("2", block_count);
}

TEST (rpc, blocks_info)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "blocks_info");
	boost::property_tree::ptree entry;
	boost::property_tree::ptree peers_l;
	entry.put ("", system.nodes[0]->latest (paper::genesis_account).to_string ());
	peers_l.push_back (std::make_pair ("", entry));
	request.add_child ("hashes", peers_l);
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	for (auto & blocks : response.json.get_child ("blocks"))
	{
		std::string hash_text (blocks.first);
		ASSERT_EQ (system.nodes[0]->latest (paper::genesis_account).to_string (), hash_text);
		std::string account_text (blocks.second.get<std::string> ("block_account"));
		ASSERT_EQ (paper::test_genesis_key.pub.to_account (), account_text);
		std::string amount_text (blocks.second.get<std::string> ("amount"));
		ASSERT_EQ (paper::genesis_amount.convert_to<std::string> (), amount_text);
		std::string blocks_text (blocks.second.get<std::string> ("contents"));
		ASSERT_FALSE (blocks_text.empty ());
	}
}

TEST (rpc, work_peers_all)
{
	paper::system system (24000, 1);
	paper::node_init init1;
	auto & node1 (*system.nodes[0]);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	paper::rpc rpc (system.service, node1, paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "work_peer_add");
	request.put ("address", "::1");
	request.put ("port", "0");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string success (response.json.get<std::string> ("success"));
	ASSERT_TRUE (success.empty ());
	boost::property_tree::ptree request1;
	request1.put ("action", "work_peers");
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	auto & peers_node (response1.json.get_child ("work_peers"));
	std::vector<std::string> peers;
	for (auto i (peers_node.begin ()), n (peers_node.end ()); i != n; ++i)
	{
		peers.push_back (i->second.get<std::string> (""));
	}
	ASSERT_EQ (1, peers.size ());
	ASSERT_EQ ("::1:0", peers[0]);
	boost::property_tree::ptree request2;
	request2.put ("action", "work_peers_clear");
	test_response response2 (request2, rpc, system.service);
	while (response2.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response2.status);
	success = response2.json.get<std::string> ("success");
	ASSERT_TRUE (success.empty ());
	test_response response3 (request1, rpc, system.service);
	while (response3.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response3.status);
	peers_node = response3.json.get_child ("work_peers");
	ASSERT_EQ (0, peers_node.size ());
}

TEST (rpc, block_count_type)
{
	paper::system system (24000, 1);
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	auto send (system.wallet (0)->send_action (paper::test_genesis_key.pub, paper::test_genesis_key.pub, system.nodes[0]->config.receive_minimum.number ()));
	ASSERT_NE (nullptr, send);
	auto receive (system.wallet (0)->receive_action (static_cast<paper::send_block &> (*send), paper::test_genesis_key.pub, system.nodes[0]->config.receive_minimum.number ()));
	ASSERT_NE (nullptr, receive);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "block_count_type");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string send_count (response.json.get<std::string> ("send"));
	ASSERT_EQ ("1", send_count);
	std::string receive_count (response.json.get<std::string> ("receive"));
	ASSERT_EQ ("1", receive_count);
	std::string open_count (response.json.get<std::string> ("open"));
	ASSERT_EQ ("1", open_count);
	std::string change_count (response.json.get<std::string> ("change"));
	ASSERT_EQ ("0", change_count);
}

TEST (rpc, ledger)
{
	paper::system system (24000, 1);
	paper::keypair key;
	paper::genesis genesis;
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key.prv);
	auto & node1 (*system.nodes[0]);
	auto latest (system.nodes[0]->latest (paper::test_genesis_key.pub));
	paper::send_block send (latest, key.pub, 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, node1.generate_work (latest));
	system.nodes[0]->process (send);
	paper::open_block open (send.hash (), paper::test_genesis_key.pub, key.pub, key.prv, key.pub, node1.generate_work (key.pub));
	ASSERT_EQ (paper::process_result::progress, system.nodes[0]->process (open).code);
	auto time (paper::seconds_since_epoch ());
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "ledger");
	request.put ("sorting", "1");
	request.put ("count", "1");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	for (auto & accounts : response.json.get_child ("accounts"))
	{
		std::string account_text (accounts.first);
		ASSERT_EQ (key.pub.to_account (), account_text);
		std::string frontier (accounts.second.get<std::string> ("frontier"));
		ASSERT_EQ (open.hash ().to_string (), frontier);
		std::string open_block (accounts.second.get<std::string> ("open_block"));
		ASSERT_EQ (open.hash ().to_string (), open_block);
		std::string representative_block (accounts.second.get<std::string> ("representative_block"));
		ASSERT_EQ (open.hash ().to_string (), representative_block);
		std::string balance_text (accounts.second.get<std::string> ("balance"));
		ASSERT_EQ ("340282366920938463463374607431768211355", balance_text);
		std::string modified_timestamp (accounts.second.get<std::string> ("modified_timestamp"));
		ASSERT_EQ (std::to_string (time), modified_timestamp);
		std::string block_count (accounts.second.get<std::string> ("block_count"));
		ASSERT_EQ ("1", block_count);
	}
}

TEST (rpc, accounts_create)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "accounts_create");
	request.put ("wallet", system.nodes[0]->wallets.items.begin ()->first.to_string ());
	request.put ("count", "8");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	auto & accounts (response.json.get_child ("accounts"));
	for (auto i (accounts.begin ()), n (accounts.end ()); i != n; ++i)
	{
		std::string account_text (i->second.get<std::string> (""));
		paper::uint256_union account;
		ASSERT_FALSE (account.decode_account (account_text));
		ASSERT_TRUE (system.wallet (0)->exists (account));
	}
	ASSERT_EQ (8, accounts.size ());
}

TEST (rpc, block_create)
{
	paper::system system (24000, 1);
	paper::keypair key;
	paper::genesis genesis;
	system.wallet (0)->insert_adhoc (paper::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key.prv);
	auto & node1 (*system.nodes[0]);
	auto latest (system.nodes[0]->latest (paper::test_genesis_key.pub));
	auto send_work = node1.generate_work (latest);
	paper::send_block send (latest, key.pub, 100, paper::test_genesis_key.prv, paper::test_genesis_key.pub, send_work);
	auto open_work = node1.generate_work (key.pub);
	paper::open_block open (send.hash (), paper::test_genesis_key.pub, key.pub, key.prv, key.pub, open_work);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "block_create");
	request.put ("type", "send");
	request.put ("wallet", system.nodes[0]->wallets.items.begin ()->first.to_string ());
	request.put ("account", paper::test_genesis_key.pub.to_account ());
	request.put ("previous", latest.to_string ());
	request.put ("amount", "340282366920938463463374607431768211355");
	request.put ("destination", key.pub.to_account ());
	request.put ("work", paper::to_string_hex (send_work));
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string send_hash (response.json.get<std::string> ("hash"));
	ASSERT_EQ (send.hash ().to_string (), send_hash);
	auto send_text (response.json.get<std::string> ("block"));
	boost::property_tree::ptree block_l;
	std::stringstream block_stream (send_text);
	boost::property_tree::read_json (block_stream, block_l);
	auto send_block (paper::deserialize_block_json (block_l));
	ASSERT_EQ (send.hash (), send_block->hash ());
	system.nodes[0]->process (send);
	boost::property_tree::ptree request1;
	request1.put ("action", "block_create");
	request1.put ("type", "open");
	std::string key_text;
	key.prv.data.encode_hex (key_text);
	request1.put ("key", key_text);
	request1.put ("representative", paper::test_genesis_key.pub.to_account ());
	request1.put ("source", send.hash ().to_string ());
	request1.put ("work", paper::to_string_hex (open_work));
	test_response response1 (request1, rpc, system.service);
	while (response1.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response1.status);
	std::string open_hash (response1.json.get<std::string> ("hash"));
	ASSERT_EQ (open.hash ().to_string (), open_hash);
	auto open_text (response1.json.get<std::string> ("block"));
	std::stringstream block_stream1 (open_text);
	boost::property_tree::read_json (block_stream1, block_l);
	auto open_block (paper::deserialize_block_json (block_l));
	ASSERT_EQ (open.hash (), open_block->hash ());
	ASSERT_EQ (paper::process_result::progress, system.nodes[0]->process (open).code);
	request1.put ("representative", key.pub.to_account ());
	test_response response2 (request1, rpc, system.service);
	while (response2.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response2.status);
	std::string open2_hash (response2.json.get<std::string> ("hash"));
	ASSERT_NE (open.hash ().to_string (), open2_hash); // different blocks with wrong representative
	auto change_work = node1.generate_work (open.hash ());
	paper::change_block change (open.hash (), key.pub, key.prv, key.pub, change_work);
	request1.put ("type", "change");
	request1.put ("work", paper::to_string_hex (change_work));
	test_response response3 (request1, rpc, system.service);
	while (response3.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response3.status);
	ASSERT_FALSE (response3.json.get<std::string> ("error").empty ()); // error with missing previous block
	request1.put ("previous", open.hash ().to_string ());
	test_response response4 (request1, rpc, system.service);
	while (response4.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response4.status);
	std::string change_hash (response4.json.get<std::string> ("hash"));
	ASSERT_EQ (change.hash ().to_string (), change_hash);
	auto change_text (response4.json.get<std::string> ("block"));
	std::stringstream block_stream4 (change_text);
	boost::property_tree::read_json (block_stream4, block_l);
	auto change_block (paper::deserialize_block_json (block_l));
	ASSERT_EQ (change.hash (), change_block->hash ());
	ASSERT_EQ (paper::process_result::progress, node1.process (change).code);
	paper::send_block send2 (send.hash (), key.pub, 0, paper::test_genesis_key.prv, paper::test_genesis_key.pub, node1.generate_work (send.hash ()));
	ASSERT_EQ (paper::process_result::progress, system.nodes[0]->process (send2).code);
	boost::property_tree::ptree request2;
	request2.put ("action", "block_create");
	request2.put ("type", "receive");
	request2.put ("wallet", system.nodes[0]->wallets.items.begin ()->first.to_string ());
	request2.put ("account", key.pub.to_account ());
	request2.put ("source", send2.hash ().to_string ());
	request2.put ("previous", change.hash ().to_string ());
	request2.put ("work", paper::to_string_hex (node1.generate_work (change.hash ())));
	test_response response5 (request2, rpc, system.service);
	while (response5.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response5.status);
	std::string receive_hash (response4.json.get<std::string> ("hash"));
	auto receive_text (response5.json.get<std::string> ("block"));
	std::stringstream block_stream5 (change_text);
	boost::property_tree::read_json (block_stream5, block_l);
	auto receive_block (paper::deserialize_block_json (block_l));
	ASSERT_EQ (receive_hash, receive_block->hash ().to_string ());
	system.nodes[0]->process_active (std::move (receive_block));
	latest = system.nodes[0]->latest (key.pub);
	ASSERT_EQ (receive_hash, latest.to_string ());
}

TEST (rpc, wallet_lock)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	std::string wallet;
	system.nodes[0]->wallets.items.begin ()->first.encode_hex (wallet);
	ASSERT_TRUE (system.wallet (0)->valid_password ());
	request.put ("wallet", wallet);
	request.put ("action", "wallet_lock");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string account_text1 (response.json.get<std::string> ("locked"));
	ASSERT_EQ (account_text1, "1");
	ASSERT_FALSE (system.wallet (0)->valid_password ());
}

TEST (rpc, wallet_locked)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	rpc.start ();
	boost::property_tree::ptree request;
	std::string wallet;
	system.nodes[0]->wallets.items.begin ()->first.encode_hex (wallet);
	request.put ("wallet", wallet);
	request.put ("action", "wallet_locked");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ (200, response.status);
	std::string account_text1 (response.json.get<std::string> ("locked"));
	ASSERT_EQ (account_text1, "0");
}

TEST (rpc, wallet_create_fail)
{
	paper::system system (24000, 1);
	paper::rpc rpc (system.service, *system.nodes[0], paper::rpc_config (true));
	auto node = system.nodes[0];
	// lmdb_max_dbs should be removed once the wallet store is refactored to support more wallets.
	for (int i = 0; i < 113; i++)
	{
		paper::keypair key;
		node->wallets.create (key.pub);
	}
	rpc.start ();
	boost::property_tree::ptree request;
	request.put ("action", "wallet_create");
	test_response response (request, rpc, system.service);
	while (response.status == 0)
	{
		system.poll ();
	}
	ASSERT_EQ ("Failed to create wallet. Increase lmdb_max_dbs in node config.", response.json.get<std::string> ("error"));
}
