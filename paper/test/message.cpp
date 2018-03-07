#include <gtest/gtest.h>
#include <paper/core/core.hpp>

TEST (message, keepalive_serialization)
{
    paper::keepalive request1;
    std::vector <uint8_t> bytes;
    {
        paper::vectorstream stream (bytes);
        request1.serialize (stream);
    }
    paper::keepalive request2;
    paper::bufferstream buffer (bytes.data (), bytes.size ());
    ASSERT_FALSE (request2.deserialize (buffer));
    ASSERT_EQ (request1, request2);
}

TEST (message, keepalive_deserialize)
{
    paper::keepalive message1;
    message1.peers [0] = paper::endpoint (boost::asio::ip::address_v6::loopback (), 10000);
    std::vector <uint8_t> bytes;
    {
        paper::vectorstream stream (bytes);
        message1.serialize (stream);
    }
    uint8_t version_max;
    uint8_t version_using;
    uint8_t version_min;
	paper::message_type type;
	std::bitset <16> extensions;
    paper::bufferstream header_stream (bytes.data (), bytes.size ());
    ASSERT_FALSE (paper::message::read_header (header_stream, version_max, version_using, version_min, type, extensions));
    ASSERT_EQ (paper::message_type::keepalive, type);
    paper::keepalive message2;
    paper::bufferstream stream (bytes.data (), bytes.size ());
    ASSERT_FALSE (message2.deserialize (stream));
    ASSERT_EQ (message1.peers, message2.peers);
}

TEST (message, publish_serialization)
{
    paper::publish publish (std::unique_ptr <paper::block> (new paper::send_block));
    ASSERT_EQ (paper::block_type::send, publish.block_type ());
    ASSERT_FALSE (publish.ipv4_only ());
    publish.ipv4_only_set (true);
    ASSERT_TRUE (publish.ipv4_only ());
    std::vector <uint8_t> bytes;
    {
        paper::vectorstream stream (bytes);
        publish.write_header (stream);
    }
    ASSERT_EQ (8, bytes.size ());
    ASSERT_EQ (0x52, bytes [0]);
    ASSERT_EQ (0x41, bytes [1]);
    ASSERT_EQ (0x01, bytes [2]);
    ASSERT_EQ (0x01, bytes [3]);
    ASSERT_EQ (0x01, bytes [4]);
    ASSERT_EQ (static_cast <uint8_t> (paper::message_type::publish), bytes [5]);
    ASSERT_EQ (0x02, bytes [6]);
    ASSERT_EQ (static_cast <uint8_t> (paper::block_type::send), bytes [7]);
    paper::bufferstream stream (bytes.data (), bytes.size ());
    uint8_t version_max;
    uint8_t version_using;
    uint8_t version_min;
    paper::message_type type;
    std::bitset <16> extensions;
    ASSERT_FALSE (paper::message::read_header (stream, version_max, version_using, version_min, type, extensions));
    ASSERT_EQ (0x01, version_min);
    ASSERT_EQ (0x01, version_using);
    ASSERT_EQ (0x01, version_max);
    ASSERT_EQ (paper::message_type::publish, type);
}

TEST (message, confirm_ack_serialization)
{
    paper::confirm_ack con1 (std::unique_ptr <paper::block> (new paper::send_block));
    paper::keypair key1;
    con1.vote.address = key1.pub;
    paper::sign_message (key1.prv, key1.pub, con1.vote.block->hash (), con1.vote.signature);
    std::vector <uint8_t> bytes;
    {
        paper::vectorstream stream1 (bytes);
        con1.serialize (stream1);
    }
    paper::bufferstream stream2 (bytes.data (), bytes.size ());
    paper::confirm_ack con2;
    con2.deserialize (stream2);
    ASSERT_EQ (con1, con2);
}