#include <gtest/gtest.h>

#include <memory>

#include <paper/lib/blocks.hpp>
#include <paper/lib/interface.h>
#include <paper/lib/numbers.hpp>
#include <paper/lib/work.hpp>

TEST (interface, ppr_uint256_to_string)
{
	paper::uint256_union zero (0);
	char text[65] = { 0 };
	ppr_uint256_to_string (zero.bytes.data (), text);
	ASSERT_STREQ ("0000000000000000000000000000000000000000000000000000000000000000", text);
}

TEST (interface, ppr_uint256_to_address)
{
	paper::uint256_union zero (0);
	char text[65] = { 0 };
	ppr_uint256_to_address (zero.bytes.data (), text);
	ASSERT_STREQ ("ppr_1111111111111111111111111111111111111111111111111111hifc8npp", text);
}

TEST (interface, ppr_uint512_to_string)
{
	paper::uint512_union zero (0);
	char text[129] = { 0 };
	ppr_uint512_to_string (zero.bytes.data (), text);
	ASSERT_STREQ ("00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", text);
}

TEST (interface, ppr_uint256_from_string)
{
	paper::uint256_union zero (0);
	ASSERT_EQ (0, ppr_uint256_from_string ("0000000000000000000000000000000000000000000000000000000000000000", zero.bytes.data ()));
	ASSERT_EQ (1, ppr_uint256_from_string ("00000000000000000000000000000000000000000000000000000000000000000", zero.bytes.data ()));
	ASSERT_EQ (1, ppr_uint256_from_string ("000000000000000000000000000%000000000000000000000000000000000000", zero.bytes.data ()));
}

TEST (interface, ppr_uint512_from_string)
{
	paper::uint512_union zero (0);
	ASSERT_EQ (0, ppr_uint512_from_string ("00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", zero.bytes.data ()));
	ASSERT_EQ (1, ppr_uint512_from_string ("000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", zero.bytes.data ()));
	ASSERT_EQ (1, ppr_uint512_from_string ("0000000000000000000000000000000000000000000000000000000000%000000000000000000000000000000000000000000000000000000000000000000000", zero.bytes.data ()));
}

TEST (interface, ppr_valid_address)
{
	ASSERT_EQ (0, ppr_valid_address ("ppr_1111111111111111111111111111111111111111111111111111hifc8npp"));
	ASSERT_EQ (1, ppr_valid_address ("ppr_1111111111111111111111111111111111111111111111111111hifc8nppp"));
	ASSERT_EQ (1, ppr_valid_address ("ppr_1111111211111111111111111111111111111111111111111111hifc8npp"));
}

TEST (interface, ppr_seed_create)
{
	paper::uint256_union seed;
	ppr_generate_random (seed.bytes.data ());
	ASSERT_FALSE (seed.is_zero ());
}

TEST (interface, ppr_seed_key)
{
	paper::uint256_union seed (0);
	paper::uint256_union prv;
	ppr_seed_key (seed.bytes.data (), 0, prv.bytes.data ());
	ASSERT_FALSE (prv.is_zero ());
}

TEST (interface, ppr_key_account)
{
	paper::uint256_union prv (0);
	paper::uint256_union pub;
	ppr_key_account (prv.bytes.data (), pub.bytes.data ());
	ASSERT_FALSE (pub.is_zero ());
}

TEST (interface, sign_transaction)
{
	paper::raw_key key;
	ppr_generate_random (key.data.bytes.data ());
	paper::uint256_union pub;
	ppr_key_account (key.data.bytes.data (), pub.bytes.data ());
	paper::send_block send (0, 0, 0, key, pub, 0);
	ASSERT_FALSE (paper::validate_message (pub, send.hash (), send.signature));
	send.signature.bytes[0] ^= 1;
	ASSERT_TRUE (paper::validate_message (pub, send.hash (), send.signature));
	auto transaction (ppr_sign_transaction (send.to_json ().c_str (), key.data.bytes.data ()));
	boost::property_tree::ptree block_l;
	std::string transaction_l (transaction);
	std::stringstream block_stream (transaction_l);
	boost::property_tree::read_json (block_stream, block_l);
	auto block (paper::deserialize_block_json (block_l));
	ASSERT_NE (nullptr, block);
	auto send1 (dynamic_cast<paper::send_block *> (block.get ()));
	ASSERT_NE (nullptr, send1);
	ASSERT_FALSE (paper::validate_message (pub, send.hash (), send1->signature));
	free (transaction);
}

TEST (interface, fail_sign_transaction)
{
	paper::uint256_union data (0);
	ppr_sign_transaction ("", data.bytes.data ());
}

TEST (interface, work_transaction)
{
	paper::raw_key key;
	ppr_generate_random (key.data.bytes.data ());
	paper::uint256_union pub;
	ppr_key_account (key.data.bytes.data (), pub.bytes.data ());
	paper::send_block send (1, 0, 0, key, pub, 0);
	auto transaction (ppr_work_transaction (send.to_json ().c_str ()));
	boost::property_tree::ptree block_l;
	std::string transaction_l (transaction);
	std::stringstream block_stream (transaction_l);
	boost::property_tree::read_json (block_stream, block_l);
	auto block (paper::deserialize_block_json (block_l));
	ASSERT_NE (nullptr, block);
	ASSERT_FALSE (paper::work_validate (*block));
	free (transaction);
}

TEST (interface, fail_work_transaction)
{
	paper::uint256_union data (0);
	ppr_work_transaction ("");
}
