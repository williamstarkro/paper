#include <gtest/gtest.h>

#include <paper/secure.hpp>

TEST (uint128_union, decode_dec)
{
    paper::uint128_union value;
    std::string text ("16");
    ASSERT_FALSE (value.decode_dec (text));
    ASSERT_EQ (16, value.qwords [0]);
}

TEST (uint256_union, key_encryption)
{
    paper::keypair key1;
    paper::uint256_union secret_key;
    secret_key.bytes.fill (0);
    paper::uint256_union encrypted (key1.prv, secret_key, key1.pub.owords [0]);
    paper::private_key key4 (encrypted.prv (secret_key, key1.pub.owords [0]));
    ASSERT_EQ (key1.prv, key4);
    paper::public_key pub;
    ed25519_publickey (key4.bytes.data (), pub.bytes.data ());
    ASSERT_EQ (key1.pub, pub);
}

TEST (uint256_union, encryption)
{
    paper::uint256_union key;
    paper::uint256_union number1 (1);
    paper::uint256_union encrypted1 (number1, key, key.owords [0]);
    paper::uint256_union encrypted2 (number1, key, key.owords [0]);
    ASSERT_EQ (encrypted1, encrypted2);
    auto number2 (encrypted1.prv (key, key.owords [0]));
    ASSERT_EQ (number1, number2);
}

TEST (uint256_union, parse_zero)
{
    paper::uint256_union input (paper::uint256_t (0));
    std::string text;
    input.encode_hex (text);
    paper::uint256_union output;
    auto error (output.decode_hex (text));
    ASSERT_FALSE (error);
    ASSERT_EQ (input, output);
    ASSERT_TRUE (output.number ().is_zero ());
}

TEST (uint256_union, parse_zero_short)
{
    std::string text ("0");
    paper::uint256_union output;
    auto error (output.decode_hex (text));
    ASSERT_FALSE (error);
    ASSERT_TRUE (output.number ().is_zero ());
}

TEST (uint256_union, parse_one)
{
    paper::uint256_union input (paper::uint256_t (1));
    std::string text;
    input.encode_hex (text);
    paper::uint256_union output;
    auto error (output.decode_hex (text));
    ASSERT_FALSE (error);
    ASSERT_EQ (input, output);
    ASSERT_EQ (1, output.number ());
}

TEST (uint256_union, parse_error_symbol)
{
    paper::uint256_union input (paper::uint256_t (1000));
    std::string text;
    input.encode_hex (text);
    text [5] = '!';
    paper::uint256_union output;
    auto error (output.decode_hex (text));
    ASSERT_TRUE (error);
}

TEST (uint256_union, max_hex)
{
    paper::uint256_union input (std::numeric_limits <paper::uint256_t>::max ());
    std::string text;
    input.encode_hex (text);
    paper::uint256_union output;
    auto error (output.decode_hex (text));
    ASSERT_FALSE (error);
    ASSERT_EQ (input, output);
    ASSERT_EQ (paper::uint256_t ("0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"), output.number ());
}

TEST (uint256_union, decode_dec)
{
    paper::uint256_union value;
    std::string text ("16");
    ASSERT_FALSE (value.decode_dec (text));
    ASSERT_EQ (16, value.qwords [0]);
}

TEST (uint256_union, max_dec)
{
    paper::uint256_union input (std::numeric_limits <paper::uint256_t>::max ());
    std::string text;
    input.encode_dec (text);
    paper::uint256_union output;
    auto error (output.decode_dec (text));
    ASSERT_FALSE (error);
    ASSERT_EQ (input, output);
    ASSERT_EQ (paper::uint256_t ("0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"), output.number ());
}

TEST (uint256_union, parse_error_overflow)
{
    paper::uint256_union input (std::numeric_limits <paper::uint256_t>::max ());
    std::string text;
    input.encode_hex (text);
    text.push_back (0);
    paper::uint256_union output;
    auto error (output.decode_hex (text));
    ASSERT_TRUE (error);
}

TEST (uint256_union, big_endian_union_constructor)
{
	boost::multiprecision::uint256_t value1 (1);
	paper::uint256_union bytes1 (value1);
	ASSERT_EQ (1, bytes1.bytes [0]);
	boost::multiprecision::uint512_t value2 (1);
	paper::uint512_union bytes2 (value2);
	ASSERT_EQ (1, bytes2.bytes [0]);
}

TEST (uint256_union, big_endian_union_function)
{
	paper::uint256_union bytes1;
	bytes1.clear ();
	bytes1.bytes [0] = 1;
	ASSERT_EQ (paper::uint256_t (1), bytes1.number ());
	paper::uint512_union bytes2;
	bytes2.clear ();
	bytes2.bytes [0] = 1;
	ASSERT_EQ (paper::uint512_t (1), bytes2.number ());
}

TEST (uint256_union, transcode_test_key_base58check)
{
    std::string string;
    paper::test_genesis_key.pub.encode_base58check (string);
    paper::uint256_union value;
    ASSERT_FALSE (value.decode_base58check (string));
    ASSERT_EQ (paper::test_genesis_key.pub, value);
}